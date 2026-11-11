import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { existsSync, mkdtempSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { after, before, beforeEach, describe, it } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { DeviceClient, DeviceError } from '../device.js';
import { registerDigitalIOTools, setDigitalOutputs } from '../digital-io.js';
import { digitalIOStatus } from '../digital-io-schema.js';
import { createServer as createFrameLabServer } from '../tools/server.js';

const instance = '87654321-4321-4321-8321-cba987654321';
const fresh = { schema_version: 1, state: 'ready', publisher_instance: instance, protocol_version: 1,
  valid_input_mask: 15, valid_output_mask: 15, input_bits: 2, output_bits: 0,
  output_revision: '9223372036854775809', fpga_timestamp_us: '9223372036854775810',
  spi_sequence: '9223372036854775811', sample_age_ms: 20, watchdog_timeout_ms: 2000 };

describe('selected-device digital I/O', () => {
  let url = '';
  let mode = 200;
  let state: unknown = fresh;
  let confirmationOverride: Record<string, unknown> = {};
  const requests: { method: string; path: string; body: unknown }[] = [];
  const http = createServer(async (request, response) => {
    let body = '';
    for await (const chunk of request) body += String(chunk);
    const value: unknown = body ? JSON.parse(body) : null;
    requests.push({ method: request.method ?? '', path: request.url ?? '', body: value });
    response.setHeader('content-type', 'application/json');
    if (request.method === 'GET') {
      response.statusCode = mode === 404 ? 404 : 200;
      response.end(JSON.stringify(state));
      return;
    }
    if (mode === 0) { response.destroy(); return; }
    if (mode === 299) { response.end(JSON.stringify({ confirmed: false })); return; }
    if (mode === 298) {
      response.writeHead(200,{'content-length':'1000'});
      response.flushHeaders();
      response.write('{');
      setTimeout(() => response.destroy(),10);
      return;
    }
    response.statusCode = mode;
    if (mode !== 200) { response.end(JSON.stringify({ error: 'conflict or uncertain' })); return; }
    const parsed = z.object({ request_id: z.string(), mask:z.number(), values: z.number() }).parse(value);
    const current = digitalIOStatus.parse(state);
    const applied = current.state === 'ready' ? (current.output_bits & ~parsed.mask) | parsed.values : 0;
    response.end(JSON.stringify({ ...fresh, ...current, output_revision: '9223372036854775810', output_bits: applied,
      request_id: parsed.request_id, confirmed: true, confirmed_output_bits: applied, command_sequence: 1,
      confirmed_at_monotonic_ns:'1234567890', ...confirmationOverride }));
  });

  before(async () => {
    await new Promise<void>(resolve => http.listen(0, '127.0.0.1', resolve));
    const address = http.address();
    assert.ok(address && typeof address !== 'string');
    url = `http://127.0.0.1:${address.port}/framelab`;
  });
  after(async () => { await new Promise<void>((resolve, reject) => http.close(error => error ? reject(error) : resolve())); });
  beforeEach(() => { mode = 200; state = fresh; requests.length = 0; confirmationOverride = {}; });

  it('reads the root sibling and retains exact uint64 strings', async () => {
    const status = await new DeviceClient(url).digitalIO();
    assert.equal(status.output_revision, fresh.output_revision);
    assert.deepEqual(requests.map(value => value.path), ['/digital-io']);
  });

  it('one fresh GET precedes one revision-checked PUT', async () => {
    const result = await setDigitalOutputs(new DeviceClient(url), { mask: 3, values: 2 });
    assert.equal(result.output_bits, 2);
    assert.deepEqual(requests.map(value => [value.method, value.path]), [['GET','/digital-io'],['PUT','/digital-io/outputs']]);
    const write = z.object({ publisher_instance: z.string(), expected_revision: z.string() }).parse(requests[1].body);
    assert.equal(write.publisher_instance, instance);
    assert.equal(write.expected_revision, fresh.output_revision);
  });

  it('does not retry conflicts, capacity or uncertain results', async () => {
    for (const code of [409,429,503,504]) {
      mode = code; requests.length = 0;
      await assert.rejects(setDigitalOutputs(new DeviceClient(url), { mask: 1, values: 1 }),
        error => error instanceof DeviceError && error.status === code);
      assert.equal(requests.length, 2);
    }
  });

  it('reports disconnected or malformed write confirmations as uncertain without retry', async () => {
    for (const code of [0,298,299]) {
      mode = code; requests.length = 0;
      await assert.rejects(setDigitalOutputs(new DeviceClient(url), {mask:1,values:1}),
        error => error instanceof DeviceError && error.status === 504);
      assert.equal(requests.length,2);
    }
  });

  it('rejects contradictory masks, zero confirmation time and incompatible revisions', async () => {
    for (const change of [{valid_input_mask:0},{valid_output_mask:0},{confirmed_at_monotonic_ns:'0'},
      {confirmed_at_monotonic_ns:0},{confirmed_at_monotonic_ns:'bad'},
      {output_revision:'bad'},{output_revision:'0'},{output_revision:fresh.output_revision},
      {output_revision:'9223372036854775811'}]) {
      confirmationOverride = change; requests.length = 0;
      await assert.rejects(setDigitalOutputs(new DeviceClient(url),{mask:1,values:1}),
        error => error instanceof DeviceError && error.status === 504);
      assert.equal(requests.length,2);
    }
  });

  it('accepts an aged confirmed receipt while current channels remain unknown', async () => {
    confirmationOverride = {state:'stale',input_bits:null,output_bits:null,sample_age_ms:500};
    const result = await setDigitalOutputs(new DeviceClient(url),{mask:1,values:1});
    assert.equal(result.confirmed,true);
    assert.equal(result.confirmed_output_bits,1);
    assert.equal(result.output_bits,null);
  });

  it('requires consistent ready or aged stale receipts and preserves unmasked latches', async () => {
    state = {...fresh,output_bits:2};
    for (const change of [{output_bits:0,confirmed_output_bits:3},
      {output_bits:1,confirmed_output_bits:1},
      {state:'unsupported',input_bits:null,output_bits:null,confirmed_output_bits:3},
      {state:'stale',input_bits:null,output_bits:null,confirmed_output_bits:3,sample_age_ms:20}]) {
      confirmationOverride = change; requests.length = 0;
      await assert.rejects(setDigitalOutputs(new DeviceClient(url),{mask:1,values:1}),
        error => error instanceof DeviceError && error.status === 504);
      assert.equal(requests.length,2);
    }
    confirmationOverride = {};
    const ready = await setDigitalOutputs(new DeviceClient(url),{mask:1,values:1});
    assert.equal(ready.confirmed_output_bits,3);
    confirmationOverride = {state:'stale',input_bits:null,output_bits:null,sample_age_ms:500};
    const aged = await setDigitalOutputs(new DeviceClient(url),{mask:1,values:1});
    assert.equal(aged.confirmed_output_bits,3);
    assert.equal(aged.output_bits,null);
  });

  it('does not send a write when the revision cannot advance without overflow', async () => {
    state = {...fresh,output_revision:'18446744073709551615'};
    await assert.rejects(setDigitalOutputs(new DeviceClient(url),{mask:1,values:1}),
      error => error instanceof DeviceError && error.status === 503);
    assert.equal(requests.length,1);
  });

  it('legacy firmware is unsupported and never written', async () => {
    mode = 404;
    assert.equal((await new DeviceClient(url).digitalIO()).state, 'unsupported');
    await assert.rejects(setDigitalOutputs(new DeviceClient(url), { mask: 1, values: 1 }));
    assert.ok(requests.every(value => value.method === 'GET'));
  });

  it('stale and malformed status cannot become an output write', async () => {
    state = { ...fresh, state: 'stale', input_bits: null, output_bits: null, sample_age_ms: 500 };
    await assert.rejects(setDigitalOutputs(new DeviceClient(url), { mask: 1, values: 1 }));
    assert.equal(requests.length, 1);
    for (const change of [{ input_bits:16 }, { sample_age_ms:500 }, { output_revision:9223372036854775809n.toString()+'0' },
      { output_revision:'01' }, { output_bits:null }]) {
      assert.equal(digitalIOStatus.safeParse({ ...fresh, ...change }).success, false);
    }
  });

  it('reserved bits and values outside mask fail before any network request', async () => {
    for (const input of [{mask:0,values:0},{mask:256,values:0},{mask:1,values:2}])
      await assert.rejects(setDigitalOutputs(new DeviceClient(url), input));
    assert.equal(requests.length,0);
  });

  it('uses the box-numbered v4 profile across all eight bits', async () => {
    state = {...fresh,protocol_version:4,valid_input_mask:255,valid_output_mask:255,input_bits:128};
    const result = await setDigitalOutputs(new DeviceClient(url),{mask:128,values:128});
    assert.equal(result.protocol_version,4);
    assert.equal(result.input_bits,128);
    assert.equal(result.confirmed_output_bits,128);
    assert.equal(requests.length,2);
  });

  it('maps OUT2 HIGH and LOW to mask 2 while preserving other v4 output latches', async () => {
    for (const [output, values, expected] of [[8, 2, 10], [10, 0, 8]]) {
      state = {...fresh,protocol_version:4,valid_input_mask:255,valid_output_mask:255,output_bits:output};
      requests.length = 0;
      const result = await setDigitalOutputs(new DeviceClient(url),{mask:2,values});
      assert.equal(result.confirmed_output_bits,expected);
      assert.equal(requests.length,2);
      const command = z.object({mask:z.number(),values:z.number()}).parse(requests[1].body);
      assert.deepEqual(command,{mask:2,values});
    }
  });

  it('requires exact protocol/mask profiles, including unavailable status, without fallback', async () => {
    for (const [protocol,inputMask,outputMask] of [[4,15,255],[4,255,15],[1,255,255],[2,255,255],[3,255,255],[5,255,255]]) {
      const invalid = {...fresh,protocol_version:protocol,valid_input_mask:inputMask,valid_output_mask:outputMask};
      assert.equal(digitalIOStatus.safeParse(invalid).success,false);
      assert.equal(digitalIOStatus.safeParse({...invalid,state:'stale',input_bits:null,output_bits:null,sample_age_ms:500}).success,false);
      state = invalid; requests.length = 0;
      await assert.rejects(setDigitalOutputs(new DeviceClient(url),{mask:2,values:2}));
      assert.deepEqual(requests.map(value => value.method),['GET']);
    }
  });

  it('rejects masks outside the selected legacy profile before PUT', async () => {
    await assert.rejects(setDigitalOutputs(new DeviceClient(url),{mask:16,values:0}),
      error => error instanceof DeviceError && error.status === 422);
    assert.deepEqual(requests.map(value => value.method),['GET']);
  });

  it('rejects historical eight-channel mappings and mismatched profile receipts', async () => {
    for (const protocol of [2,3]) {
      state={...fresh,protocol_version:protocol,valid_input_mask:255,valid_output_mask:255};
      requests.length=0;
      await assert.rejects(setDigitalOutputs(new DeviceClient(url),{mask:1,values:1}));
      assert.deepEqual(requests.map(value => value.method),['GET']);
    }
    state={...fresh,protocol_version:4,valid_input_mask:255,valid_output_mask:255};
    requests.length=0;confirmationOverride={protocol_version:1,valid_input_mask:15,valid_output_mask:15};
    await assert.rejects(setDigitalOutputs(new DeviceClient(url),{mask:1,values:1}),
      error => error instanceof DeviceError && error.status === 504);
    assert.equal(requests.length,2);
  });

  it('rejects arbitrary/public/credentialled target URLs', () => {
    for (const value of ['http://example.com/framelab','http://192.168.1.2/','http://u:p@192.168.1.2/framelab'])
      assert.throws(() => new DeviceClient(value));
  });

  it('registers both tools in the existing MCP server and returns uncertainty without retry', async () => {
    const server = new McpServer({name:'dio-test',version:'1'});
    registerDigitalIOTools(server,new DeviceClient(url));
    const client = new Client({name:'dio-client',version:'1'});
    const [left,right] = InMemoryTransport.createLinkedPair();
    await Promise.all([server.connect(left),client.connect(right)]);
    try {
      assert.deepEqual((await client.listTools()).tools.map(tool => tool.name),['get_digital_io','set_digital_outputs']);
      mode = 504;
      const result = await client.callTool({name:'set_digital_outputs',arguments:{mask:1,values:1}});
      assert.equal(result.isError,true);
      assert.equal(requests.length,2);
      const content = result.content;
      assert.ok(Array.isArray(content));
      assert.match(JSON.stringify(content),/504/);
    } finally { await client.close(); await server.close(); }
  });

  it('serves direct DIO instructions and control without creating or preparing a callback project', async () => {
    state = {...fresh,protocol_version:4,valid_input_mask:255,valid_output_mask:255};
    const workspace = mkdtempSync(join(tmpdir(),'framelab-direct-dio-'));
    const server = createFrameLabServer({workspace,camera:'',device:new DeviceClient(url),snapshot:''});
    const client = new Client({name:'direct-dio-client',version:'1'});
    const [left,right] = InMemoryTransport.createLinkedPair();
    await Promise.all([server.connect(left),client.connect(right)]);
    try {
      assert.match(client.getInstructions() ?? '',/Do not create a camera callback or prepare a package/);
      assert.match(client.getInstructions() ?? '',/protocol 4.*masks 255/);
      assert.match(client.getInstructions() ?? '',/legacy protocol 1 require separately verified operator mapping/);
      const result = await client.callTool({name:'set_digital_outputs',arguments:{mask:2,values:2}});
      assert.notEqual(result.isError,true);
      assert.deepEqual(requests.map(value => [value.method,value.path]),[['GET','/digital-io'],['PUT','/digital-io/outputs']]);
      assert.equal(existsSync(join(workspace,'package')),false);
      assert.equal(existsSync(join(workspace,'history')),false);
    } finally { await client.close(); await server.close(); }
  });
});
