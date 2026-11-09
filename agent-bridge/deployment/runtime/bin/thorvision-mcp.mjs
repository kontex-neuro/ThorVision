#!/usr/bin/env node
// Compatibility entry point. Both names use one implementation and state store.
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { main } from './thorvision-framelab.mjs';
export * from './thorvision-framelab.mjs';

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main().catch(error => { console.error(error.message); process.exitCode = 1; });
}
