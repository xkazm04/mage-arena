// The Leonardo call recipe shared by apps/vr/tools/assets/leonardo.mjs and apps/vr/tools/keyart/leonardo-keyart.mjs:
// key read, one HTTP call, balance, reported cost, log write and the nightly credit total. Dependency-free (Node's fetch and fs only).
// Nothing here makes a call by itself. The key is read at run time from bp's untracked .env and never printed, logged or
// written: every response body passes through redact before it is parsed or cut, so a key echoed back by the API,
// even across a truncation boundary, never reaches a log, stdout, stderr or an error message.
import fs from 'node:fs';
import path from 'node:path';

export const ENV_FILE = 'C:/Users/kazda/kiro/bp/.env';
export const API = 'https://cloud.leonardo.ai/api/rest';
export const MODEL = 'openai/gpt-image-2.5-flare';
export const COST = { LOW: 6, MEDIUM: 14, HIGH: 60 }; // LOW and MEDIUM measured by bp on 2026-10-20 (1376x768); HIGH a high guess
export const POLL_MS = 5000;
export const POLL_LIMIT_MS = 240000;
export const NIGHT_CAP = 400; // operator: 400 credits per night for this project, every asset and tool together

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
export const sumCredits = (calls) => calls.reduce((n, e) => n + (Number(e.credits) || 0), 0);

/** `text` with every copy of the key replaced: whatever goes to the log or stdout passes through here. */
export const redact = (text, key) => (key ? String(text).replaceAll(key, '[redacted]') : String(text));

export function readKey() {
  const m = fs.readFileSync(ENV_FILE, 'utf8').match(/^\s*LEONARDO_API_KEY\s*=\s*(.+?)\s*$/m);
  if (!m) throw new Error(`LEONARDO_API_KEY not found in ${ENV_FILE}`);
  return m[1].replace(/^['"]|['"]$/g, '');
}

export async function call(key, method, url, body) {
  const res = await fetch(url, {
    method,
    headers: { accept: 'application/json', authorization: `Bearer ${key}`, ...(body ? { 'content-type': 'application/json' } : {}) },
    body: body ? JSON.stringify(body) : undefined,
  });
  // Redacted before it is parsed or cut: a key echoed in the body and then cut in half would slip past redact.
  const text = redact(await res.text(), key);
  let json;
  try { json = JSON.parse(text); } catch { json = { raw: text.slice(0, 500) }; }
  // JSON.parse decodes \u escapes, so a key the body spelled escaped only appears now: redact the parsed value too.
  if (key && JSON.stringify(json).includes(key)) json = JSON.parse(redact(JSON.stringify(json), key));
  if (!res.ok) throw new Error(`${method} ${url} -> HTTP ${res.status}: ${JSON.stringify(json).slice(0, 500)}`);
  return json;
}

/** Sum of every numeric token pool on the account; the difference across a call is what it cost. */
export async function balance(key) {
  try {
    const me = await call(key, 'GET', `${API}/v1/me`);
    const u = me.user_details?.[0] ?? {};
    return Object.entries(u).filter(([k, v]) => /tokens$/i.test(k) && typeof v === 'number').reduce((n, [, v]) => n + v, 0);
  } catch {
    return null;
  }
}

/** A cost the API reports itself, if it does: any numeric field named like cost or credit. */
export function reportedCost(obj, depth = 0) {
  if (!obj || typeof obj !== 'object' || depth > 4) return null;
  for (const [k, v] of Object.entries(obj)) {
    if (/cost|credit/i.test(k)) {
      if (typeof v === 'number') return v;
      if (v && typeof v === 'object' && typeof v.amount === 'number') return v.amount;
    }
  }
  for (const v of Object.values(obj)) {
    const c = reportedCost(v, depth + 1);
    if (c != null) return c;
  }
  return null;
}

/** Writes a log document to `file`, creating its folder. */
export function writeLog(file, doc) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, JSON.stringify(doc, null, 2) + '\n');
}

/** Every leonardo-log.json under dir (node_modules and .git are never entered). */
export function findLogs(dir, out = []) {
  let ents = [];
  try { ents = fs.readdirSync(dir, { withFileTypes: true }); } catch { return out; }
  for (const e of ents) {
    const p = path.join(dir, e.name);
    if (e.isDirectory() && e.name !== 'node_modules' && e.name !== '.git') findLogs(p, out);
    else if (e.isFile() && e.name === 'leonardo-log.json') out.push(p);
  }
  return out;
}

// Other checkouts of this repo whose logs count against the night (read only, never written).
const NIGHT_ROOTS = ['C:/Users/kazda/kiro/mage-arena', 'C:/Users/kazda/.personas/headless-masters/worktrees/mage-arena'];

/** The apps/vr/art folders the night cap counts: `root`'s, the main checkout's and every sibling worktree's. */
export function nightArts(root) {
  const arts = [path.join(root, 'apps/vr/art'), path.join(NIGHT_ROOTS[0], 'apps/vr/art')];
  try {
    for (const e of fs.readdirSync(NIGHT_ROOTS[1], { withFileTypes: true })) {
      if (e.isDirectory()) arts.push(path.join(NIGHT_ROOTS[1], e.name, 'apps/vr/art'));
    }
  } catch { /* no sibling worktrees */ }
  return arts;
}

/** The credits for the UTC day `day` (YYYY-MM-DD) in every leonardo-log.json under the `arts` folders, each generation once. */
export function nightTotal(arts, day = new Date().toISOString().slice(0, 10)) {
  const seen = new Map();
  for (const f of new Set(arts.flatMap((d) => findLogs(d)).map((f) => path.resolve(f)))) {
    let calls = [];
    try { calls = JSON.parse(fs.readFileSync(f, 'utf8')).calls ?? []; } catch { continue; }
    for (const c of calls) {
      if (!String(c.date).startsWith(day)) continue;
      seen.set(c.generationId ?? `${f}|${c.date}|${c.seed}|${c.view ?? c.id}`, Number(c.credits) || 0);
    }
  }
  return [...seen.values()].reduce((n, x) => n + x, 0);
}
