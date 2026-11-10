import { z } from 'zod';

const uint64 = z.string().regex(/^(0|[1-9][0-9]{0,19})$/)
  .refine(value => value.length < 20 || value <= '18446744073709551615');
const identifier = z.uuid().refine(value => value !== '00000000-0000-0000-0000-000000000000');
export const bits = z.number().int().min(0).max(15);
const common = z.object({
  schema_version: z.literal(1), publisher_instance: identifier.nullable(), protocol_version: z.literal(1).nullable(),
  valid_input_mask: z.union([z.literal(0), z.literal(15)]), valid_output_mask: z.union([z.literal(0), z.literal(15)]),
  output_revision: uint64.nullable(), fpga_timestamp_us: uint64.nullable(), spi_sequence: uint64.nullable(),
  sample_age_ms: z.number().nonnegative().nullable(), watchdog_timeout_ms: z.literal(2000),
});
const ready = common.extend({ state: z.literal('ready'), publisher_instance: identifier, protocol_version: z.literal(1),
  valid_input_mask: z.literal(15), valid_output_mask: z.literal(15), input_bits: bits, output_bits: bits,
  output_revision: uint64, fpga_timestamp_us: uint64, spi_sequence: uint64, sample_age_ms: z.number().min(0).lt(500) });
const unknown = common.extend({ state: z.enum(['unsupported', 'stale', 'unavailable']),
  input_bits: z.null(), output_bits: z.null() });
export const digitalIOStatus = z.discriminatedUnion('state', [ready, unknown]);
export type DigitalIOStatus = z.infer<typeof digitalIOStatus>;
const receipt = z.object({ request_id: identifier, confirmed: z.literal(true), confirmed_output_bits: bits,
  publisher_instance: identifier, protocol_version: z.literal(1), output_revision: uint64,
  valid_input_mask: z.literal(15), valid_output_mask: z.literal(15),
  confirmed_at_monotonic_ns: uint64.refine(value => value !== '0'),
  command_sequence: z.number().int().min(1).max(65535) });
export const digitalIOConfirmation = z.intersection(digitalIOStatus, receipt).refine(value =>
  (value.state === 'ready' && value.output_bits === value.confirmed_output_bits) ||
  (value.state === 'stale' && value.sample_age_ms !== null && value.sample_age_ms >= 500));
export const outputArguments = z.object({ mask: z.number().int().min(1).max(15), values: bits })
  .refine(value => (value.values & ~value.mask) === 0, 'values may set only masked channel bits');

export function unsupportedDigitalIO(): DigitalIOStatus {
  return { schema_version: 1, state: 'unsupported', publisher_instance: null, protocol_version: null,
    valid_input_mask: 0, valid_output_mask: 0, input_bits: null, output_bits: null, output_revision: null,
    fpga_timestamp_us: null, spi_sequence: null, sample_age_ms: null, watchdog_timeout_ms: 2000 };
}
