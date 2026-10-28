// One still from the operator's Leonardo web studio (GPT Image 2.5 Sunburst), saved to disk, then deleted in the
// studio. It drives the signed-in ~/.leonardo-chrome profile through gravitone-gcloud's studio provider, which
// downloads the image and then deletes the generation (operator rule 2026-10-23: keep the studio clean).
//
// Run from the gravitone checkout so its path aliases and .env.local resolve:
//   cd C:/Users/kazda/kiro/gravitone-gcloud
//   npx tsx C:/Users/kazda/kiro/mage-arena/apps/vr/tools/keyart/studio-still.mts health
//   npx tsx C:/Users/kazda/kiro/mage-arena/apps/vr/tools/keyart/studio-still.mts gen --aspect 1:1 --out <file.png> --prompt "<text>"
// One image per call. The prompt file form (--prompt-file) avoids shell quoting. Every call appends one line to
// <out dir>/studio-log.jsonl (prompt, aspect, generation id, cleanup, credits), never the API key.

import { appendFileSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const GRAVITONE = process.env.GRAVITONE_ROOT ?? "C:/Users/kazda/kiro/gravitone-gcloud";

function loadEnvLocal(): void {
  let text = "";
  try {
    text = readFileSync(join(GRAVITONE, ".env.local"), "utf8");
  } catch {
    return;
  }
  for (const line of text.split(/\r?\n/)) {
    const m = /^([A-Z0-9_]+)=(.*)$/.exec(line.trim());
    if (m && process.env[m[1]] === undefined) process.env[m[1]] = m[2].replace(/^["']|["']$/g, "");
  }
}

function arg(name: string): string | undefined {
  const i = process.argv.indexOf(`--${name}`);
  return i > 0 ? process.argv[i + 1] : undefined;
}

async function main(): Promise<void> {
  loadEnvLocal();
  const mod = (p: string) => import(pathToFileURL(join(GRAVITONE, p)).href);
  const cmd = process.argv[2];
  if (cmd === "health") {
    const { leonardoStudioHealth } = await mod("lib/imaging/providers/leonardoStudio.ts");
    console.log(JSON.stringify(await leonardoStudioHealth(), null, 2));
    return;
  }
  if (cmd !== "gen") {
    console.error("usage: studio-still.mts health | gen --aspect 1:1|16:9|9:16 --out <file> (--prompt <text> | --prompt-file <file>) [--quality low|medium]");
    process.exit(2);
  }
  const aspect = arg("aspect") ?? "1:1";
  const out = arg("out");
  const prompt = arg("prompt") ?? (arg("prompt-file") ? readFileSync(arg("prompt-file")!, "utf8").trim() : undefined);
  if (!out || !prompt) {
    console.error("gen needs --out and --prompt or --prompt-file");
    process.exit(2);
  }
  const quality = (arg("quality") as "low" | "medium" | undefined) ?? "medium";
  const { leonardoStudioProvider } = await mod("lib/imaging/providers/leonardoStudio.ts");
  const result = await leonardoStudioProvider().generate({ prompt, aspect, count: 1, quality });
  const image = result.images[0];
  const path = resolve(out);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, Buffer.from(image.base64, "base64"));
  const p = result.provenance as { remoteIds?: string[]; cleanup?: string; studio?: { credits?: number; balance?: number }; model?: string };
  const line = { at: new Date().toISOString(), out: path, aspect, quality, model: p.model, id: p.remoteIds?.[0], cleanup: p.cleanup, credits: p.studio?.credits, balance: p.studio?.balance, prompt };
  appendFileSync(join(dirname(path), "studio-log.jsonl"), JSON.stringify(line) + "\n");
  console.log(JSON.stringify({ ...line, prompt: undefined, width: image.width, height: image.height }));
  if (p.cleanup !== "deleted") console.error(`WARNING: studio generation ${p.remoteIds?.[0]} was NOT deleted; delete it by hand in the studio.`);
}

// The CDP connection keeps the event loop alive; exit explicitly (the operator's Chrome stays open).
main().then(() => process.exit(0)).catch((e) => {
  console.error(e instanceof Error ? e.message : String(e));
  process.exit(1);
});
