import { validateDeviceUrl } from '../dist/device-url.js';
try {
  validateDeviceUrl(process.argv[2] || '');
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
