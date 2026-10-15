// Check the WebAssembly build against PyOpenColorIO bakes from reference.py.
//
//   node test/compare.mjs reference.json
//
// A config given as a file path is copied into the module's file system along with the
// folder it sits in, so relative LUT paths inside the config resolve.

import fs from 'node:fs';
import path from 'node:path';
import Heaviside from '../build/heaviside.mjs';

const reference = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const H = await Heaviside();
console.log(`OCIO ${H.version()}`);

function copyTree(from, to) {
  H.FS.mkdirTree(to);
  for (const entry of fs.readdirSync(from, { withFileTypes: true })) {
    const src = path.join(from, entry.name);
    if (entry.isDirectory()) copyTree(src, `${to}/${entry.name}`);
    else H.FS.writeFile(`${to}/${entry.name}`, fs.readFileSync(src));
  }
}

let failed = 0;
for (const { config, bakes } of reference) {
  let target = config;
  if (!config.startsWith('ocio://')) {
    const dir = `/configs/${fs.realpathSync(path.dirname(config)).replace(/\W+/g, '_')}`;
    if (!H.FS.analyzePath(dir).exists) copyTree(path.dirname(config), dir);
    target = `${dir}/${path.basename(config)}`;
  }
  H.load(target);
  console.log(`\n${config}`);
  for (const b of bakes) {
    const start = performance.now();
    const got = H.bakeView(b.source, b.display, b.view, b.size);
    const ms = performance.now() - start;
    let worst = 0, sum = 0;
    for (let i = 0; i < got.length; i++) {
      const d = Math.abs(got[i] - b.values[i]);
      sum += d;
      worst = Math.max(worst, d);
    }
    const mean = sum / got.length;
    // Measured against PyOpenColorIO 2.5.2 on Windows: under 1e-3 for the built-in config,
    // up to 4e-3 at single grid points for views built from LUT files, mean under 5e-5.
    const ok = worst < 5e-3 && mean < 1e-4;
    if (!ok) failed++;
    console.log(`  ${ok ? 'ok  ' : 'FAIL'} ${b.source} -> ${b.view}: worst ${worst.toExponential(1)}, mean ${mean.toExponential(1)}, ${ms.toFixed(0)} ms`);
  }
}

try {
  H.load('/missing.ocio');
  console.log('\nFAIL a missing config loaded');
  failed++;
} catch (e) {
  console.log(`\nok   missing config throws: ${e.message}`);
}
process.exit(failed ? 1 : 0);
