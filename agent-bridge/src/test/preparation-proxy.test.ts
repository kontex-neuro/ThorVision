import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { decodeProxyChunk, PreparationProxy, validateProxyTarget, validateRelayUrl } from '../preparation-proxy.js';

describe('preparation proxy boundaries', () => {
  it('permits only repository HTTPS connections, never private or arbitrary destinations', () => {
    assert.deepEqual(validateProxyTarget('1', 'PyPI.org', 443), { id: '1', host: 'pypi.org', port: 443 });
    for (const host of ['127.0.0.1', '192.168.177.100', 'localhost', 'pypi.org.attacker.example', 'example.com', 'user@pypi.org'])
      assert.throws(() => validateProxyTarget('1', host, 443));
    for (const port of [80, 22, '443', undefined]) assert.throws(() => validateProxyTarget('1', 'pypi.org', port));
    assert.throws(() => validateProxyTarget('../connection', 'pypi.org', 443));
  });

  it('round-trips bounded binary data and rejects oversized/noncanonical data', () => {
    const bytes = Buffer.alloc(48 * 1024, 0xa5);
    assert.deepEqual(decodeProxyChunk(bytes.toString('base64')), bytes);
    for (const value of [Buffer.alloc(48 * 1024 + 1).toString('base64'), 'a', 'a===', '%%%=', null, {}, 'YQ==\n'])
      assert.throws(() => decodeProxyChunk(value));
  });

  it('allows token-bound device API relays and refuses unrelated endpoints before opening sockets', () => {
    assert.doesNotThrow(() => validateRelayUrl(`ws://192.168.177.100:8000/analytics/v1/jobs/${'a'.repeat(32)}/proxy?token=abc`));
    assert.doesNotThrow(() => validateRelayUrl(`ws://127.0.0.1:8770/v1/jobs/${'a'.repeat(32)}/proxy?token=abc`));
    for (const url of ['ws://192.168.177.100:8770/v1/jobs/x/proxy', 'wss://example.com/proxy', 'ws://127.0.0.1:8770/other'])
      assert.throws(() => new PreparationProxy(url), /token-bound device API relay/);
  });
});
