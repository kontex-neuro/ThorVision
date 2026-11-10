import { randomUUID } from 'node:crypto';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { DeviceClient, DeviceError } from './device.js';

import { bits, digitalIOConfirmation, outputArguments } from './digital-io-schema.js';

export async function setDigitalOutputs(device: DeviceClient, input: z.infer<typeof outputArguments>) {
  const { mask, values } = outputArguments.parse(input);
  const current = await device.digitalIO();
  if (current.state !== 'ready') throw new DeviceError(503, `Digital I/O is ${current.state}; no write was sent`);
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
    description: 'Read four digital input and applied output latch bits on the selected device. 1=HIGH, 0=LOW; stale/unsupported channels are unknown. This does not measure output connector voltage.',
    annotations: { readOnlyHint: true },
  }, () => run(() => device.digitalIO()));
  server.registerTool('set_digital_outputs', {
    description: 'Set selected digital output latches on the selected device: mask 1..15, values only within mask; 1=HIGH, 0=LOW. Reads fresh status then sends one revision-checked write. A conflict or uncertain result is returned without retry. Continued SPI polling preserves outputs even if MCP disconnects.',
    inputSchema: { mask: z.number().int().min(1).max(15), values: bits },
    annotations: { readOnlyHint: false, destructiveHint: true, idempotentHint: false },
  }, input => run(() => setDigitalOutputs(device, input)));
}
