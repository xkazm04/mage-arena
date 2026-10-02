// Mouse-drawn sigil clips. Same loader as the hand clips; source is "mouse".
// 30 paths per class. Seeds sit in the mouse band and never overlap templates or the corpus.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { SIGIL_CLASSES, MOUSE_BASE, buildVariedClip } from './corpus.mjs';

const PER_CLASS = 30;

function main() {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const dir = path.resolve(here, '..', '..', 'Game', 'Clips', 'mouse');
  fs.mkdirSync(dir, { recursive: true });
  for (const name of fs.readdirSync(dir)) {
    if (name.endsWith('.jsonl')) {
      fs.unlinkSync(path.join(dir, name));
    }
  }
  let count = 0;
  for (let c = 0; c < SIGIL_CLASSES.length; c++) {
    const [action, stroke] = SIGIL_CLASSES[c];
    for (let i = 0; i < PER_CLASS; i++) {
      const seed = MOUSE_BASE + c * 1000 + i;
      const built = buildVariedClip(action, stroke, 'normal', seed, 'mouse', true, 'mouse');
      fs.writeFileSync(path.join(dir, `${action}.normal.m${seed}.jsonl`), built.text, { encoding: 'utf8' });
      count++;
    }
  }
  process.stdout.write(`wrote ${count} mouse clips to Game/Clips/mouse\n`);
}

main();
