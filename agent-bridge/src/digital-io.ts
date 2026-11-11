import { randomUUID } from 'node:crypto';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { DeviceClient, DeviceError } from './device.js';

import { bits, digitalIOConfirmation, outputArguments } from './digital-io-schema.js';

export const DIGITAL_IO_INSTRUCTIONS = [
  'Direct operator-requested digital I/O uses get_digital_io and set_digital_outputs on the selected device.',
  'Do not create a camera callback or prepare a package for direct digital I/O control.',
  'Ready protocol 4 uses eight channel bits and valid masks 255; legacy protocol 1 uses four bits and masks 15.',
  'Reject other protocol or mask combinations. Current protocol 4 box channels 1, 2, 3 and 4 correspond to masks 1, 2, 4 and 8.',
  'The current box exposes ports 1 through 4; internal channels 5 through 8 remain physically unqualified.',
  'Printed box connector requests on legacy protocol 1 require separately verified operator mapping; do not infer current box wiring from logical compatibility.',
  'For an authorized OUT2 request on the current protocol 4 box use mask 2, values 2 for HIGH or values 0 for LOW, preserving every other output.',
  'Read fresh status and require the command acknowledgment; never retry a HIGH write automatically after a conflict or uncertain result.',
  'Digital OUT2 is a TTL connector, separate from camera-status RGB LED2/GPIO20. FPGA latch readback is not proof that an external LED lit.',
  'MCP disconnection does not clear outputs while SPI polling continues. Explicitly clear the requested output when the operator says stop.',
  'Use only the existing device API; no SSH, direct SPI or second publisher. Keep native-provider permissions and login ownership.',
  'Callbacks retain their isolation and cannot invoke DIO tools. Camera package activation and deletion remain human-reviewed panel actions.',
].join(' ');

export async function setDigitalOutputs(device: DeviceClient, input: z.infer<typeof outputArguments>) {
  const { mask, values } = outputArguments.parse(input);
  const current = await device.digitalIO();
  if (current.state !== 'ready') throw new DeviceError(503, `Digital I/O is ${current.state}; no write was sent`);
  if ((mask & ~current.valid_output_mask) !== 0)
    throw new DeviceError(422, 'Output mask exceeds the selected hardware profile; no write was sent');
  const expectedRevision = BigInt(current.output_revision);
  if (expectedRevision === 18446744073709551615n)
    throw new DeviceError(503, 'Digital I/O output revision is exhausted; no write was sent');
  const expectedOutputs = (current.output_bits & ~mask) | values;
  const requestId = randomUUID();
  const parsed = digitalIOConfirmation.safeParse(await device.writeDigitalOutputs({ request_id: requestId,
    publisher_instance: current.publisher_instance, expected_revision: current.output_revision, mask, values }));
  if (!parsed.success) throw new DeviceError(504, 'Digital I/O write reply is invalid; completion is uncertain. Do not resend automatically.');
  const result = parsed.data;
  if (result.request_id !== requestId || result.publisher_instance !== current.publisher_instance ||
      result.protocol_version !== current.protocol_version || result.valid_input_mask !== current.valid_input_mask ||
      result.valid_output_mask !== current.valid_output_mask ||
      BigInt(result.output_revision) !== expectedRevision + 1n ||
      result.confirmed_output_bits !== expectedOutputs)
    throw new DeviceError(504, 'Digital I/O confirmation does not match this command; completion is uncertain');
  return result;
}

export function registerDigitalIOTools(server: McpServer, device: DeviceClient): void {
  const run = async (action: () => Promise<unknown>): Promise<{ content: { type: 'text'; text: string }[]; isError?: boolean }> => {
    try {
      return { content: [{ type: 'text', text: JSON.stringify(await action(), null, 2) }] };
    } catch (error) {
      const value = { error: error instanceof Error ? error.message : String(error),
        http_status: error instanceof DeviceError ? error.status : null, retried: false };
      return { isError: true, content: [{ type: 'text', text: JSON.stringify(value) }] };
    }
  };
  server.registerTool('get_digital_io', {
    description: 'Read digital input and applied output latch bits for the selected hardware profile. 1=HIGH, 0=LOW; stale/unsupported channels are unknown. This does not measure output connector voltage.',
    annotations: { readOnlyHint: true },
  }, () => run(() => device.digitalIO()));
  server.registerTool('set_digital_outputs', {
    description: 'Set selected digital output latches on the selected device: mask 1..255 within the selected profile, values only within mask; 1=HIGH, 0=LOW. Reads fresh status then sends one revision-checked write. A conflict or uncertain result is returned without retry. Continued SPI polling preserves outputs even if MCP disconnects.',
    inputSchema: { mask: z.number().int().min(1).max(255), values: bits },
    annotations: { readOnlyHint: false, destructiveHint: true, idempotentHint: false },
  }, input => run(() => setDigitalOutputs(device, input)));
}
