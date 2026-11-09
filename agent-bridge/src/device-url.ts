import { isIP } from 'node:net';

/** Limit native device access to the local network and the established analytics gateway. */
export function validateDeviceUrl(value: string): URL {
  const url = new URL(value);
  const octets = url.hostname.split('.').map(Number);
  const local = isIP(url.hostname) === 4 && (octets[0] === 127 || octets[0] === 10 ||
    (octets[0] === 192 && octets[1] === 168) || (octets[0] === 172 && octets[1] >= 16 && octets[1] <= 31) ||
    (octets[0] === 169 && octets[1] === 254));
  if (!local || !['http:', 'https:'].includes(url.protocol) || url.username || url.password || url.search || url.hash ||
      !['/', '/framelab', '/analytics'].includes(url.pathname) || (url.pathname === '/' && octets[0] !== 127))
    throw new Error('FrameLab requires a private or link-local IPv4 device API URL ending in /framelab (legacy /analytics is supported), or a loopback API URL');
  return url;
}
