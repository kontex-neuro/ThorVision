// The plugin is copied into Claude's cache. Resolve the configured shared
// runtime explicitly rather than assuming its siblings were copied too.
import { existsSync } from 'node:fs';
import { isAbsolute, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const configured = process.env.THORVISION_CONFIG_BRIDGE_DIR;
const bridge = configured || fileURLToPath(new URL('../../../', import.meta.url));
if (!isAbsolute(bridge) || !existsSync(resolve(bridge, 'dist/tools/server.js'))) {
  throw new Error('Configure the built ThorVision bridge directory with /plugin configure thorvision-framelab.');
}
process.env.THORVISION_EXTENSIONS_URL ||= process.env.THORVISION_CONFIG_DEVICE_URL || '';
process.env.THORVISION_FRAMELAB_URL ||= process.env.THORVISION_CONFIG_DEVICE_URL || process.env.THORVISION_EXTENSIONS_URL;
process.env.THORVISION_WORKSPACE ||= process.env.THORVISION_CONFIG_WORKSPACE || '';
process.env.THORVISION_SDK_SNAPSHOT ||= resolve(bridge, 'sdk-snapshot');
const { startStdioServer } = await import(pathToFileURL(resolve(bridge, 'dist/tools/server.js')).href);
await startStdioServer();
