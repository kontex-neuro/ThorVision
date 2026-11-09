#!/usr/bin/env node
// Compatibility entry point. Both names use one implementation and state store.
import { isEntry, main } from './thorvision-framelab.mjs';
export * from './thorvision-framelab.mjs';

if (isEntry(import.meta.url)) {
  main().catch(error => { console.error(error.message); process.exitCode = 1; });
}
