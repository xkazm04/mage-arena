// Zero-consumer gate for private and protected data members. Node built-ins only.
// The compiler does not catch these in a UCLASS: -Wunused-private-field only fires when every
// member of the class is defined in one translation unit, and UHT puts part of it in .gen.cpp.
// The arena-greybox sweep found UHandPresentationComponent::Wards by grep (de92c3c); this is that grep.
//
// A member counts as used when its name appears again in the class body or in any source file that
// defines `Class::` (or `Friend::` for a declared friend). UPROPERTY members are skipped: reflection,
// config and Blueprint can read them. A declaration line carrying `lint:allow-unused <reason>` is skipped.
//
// Usage: node apps/vr/tools/lint/unused-members.mjs [--root <Source dir>] [--self-test]
import { mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync, existsSync, mkdirSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const defaultRoot = join(here, '..', '..', 'Game', 'Source');

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let root = defaultRoot;
  let selfTest = false;
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--self-test') selfTest = true;
    else if (arg === '--root') {
      root = argv[++i];
      if (!root) fail('--root requires a path');
    } else if (arg.startsWith('--root=')) root = arg.slice('--root='.length);
    else fail(`unknown argument: ${arg}`);
  }
  return { root, selfTest };
}

function listSources(dir) {
  const out = [];
  const walk = (current) => {
    for (const entry of readdirSync(current, { withFileTypes: true })) {
      const full = join(current, entry.name);
      if (entry.isDirectory()) walk(full);
      else if (/\.(h|hpp|cpp|inl)$/i.test(entry.name)) out.push(full);
    }
  };
  walk(dir);
  return out.sort();
}

// End offset (exclusive) of a raw string R"delim( ... )delim" whose quote is at i, or -1.
function rawStringEnd(text, i) {
  if (text[i - 1] !== 'R') return -1;
  const paren = text.indexOf('(', i);
  if (paren < 0) return -1;
  const delim = text.slice(i + 1, paren);
  if (!/^[^\s()\\"]{0,16}$/.test(delim)) return -1;
  const close = text.indexOf(`)${delim}"`, paren);
  return close < 0 ? text.length : close + delim.length + 2;
}

// Blank comments, string and char literals and preprocessor lines, keeping every newline and every
// offset, so braces in text cannot unbalance the parse and line numbers still match the file.
export function strip(text) {
  const out = text.split('');
  const blank = (from, to) => {
    for (let k = from; k < to; k++) if (out[k] !== '\n' && out[k] !== '\r') out[k] = ' ';
  };
  let i = 0;
  let lineStart = true;
  while (i < text.length) {
    const ch = text[i];
    const next = text[i + 1];
    if (ch === '\n') {
      lineStart = true;
      i++;
      continue;
    }
    if (lineStart && ch === '#') {
      let j = i;
      while (j < text.length && text[j] !== '\n') {
        if (text[j] === '\\' && text[j + 1] === '\n') j++;
        j++;
      }
      blank(i, j);
      i = j;
      continue;
    }
    if (ch !== ' ' && ch !== '\t' && ch !== '\r') lineStart = false;
    if (ch === '/' && next === '/') {
      let j = i;
      while (j < text.length && text[j] !== '\n') j++;
      blank(i, j);
      i = j;
    } else if (ch === '/' && next === '*') {
      const end = text.indexOf('*/', i + 2);
      const j = end < 0 ? text.length : end + 2;
      blank(i, j);
      i = j;
    } else if (ch === '"' && rawStringEnd(text, i) > 0) {
      const j = rawStringEnd(text, i);
      blank(i + 1, j);
      i = j;
    } else if (ch === '"' || ch === "'") {
      // A ' between two digits is a C++14 digit separator, not a char literal.
      if (ch === "'" && /[0-9A-Fa-f]/.test(text[i - 1] ?? '') && /[0-9A-Fa-f]/.test(next ?? '')) {
        i++;
        continue;
      }
      let j = i + 1;
      while (j < text.length && text[j] !== ch && text[j] !== '\n') {
        if (text[j] === '\\') j++;
        j++;
      }
      blank(i + 1, j);
      i = j + 1;
    } else {
      i++;
    }
  }
  return out.join('');
}

function matchBrace(text, open) {
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === '{') depth++;
    else if (text[i] === '}') {
      depth--;
      if (depth === 0) return i;
    }
  }
  return -1;
}

function lineOf(text, offset) {
  let line = 1;
  for (let i = 0; i < offset && i < text.length; i++) if (text[i] === '\n') line++;
  return line;
}

const ACCESS = /\b(public|private|protected)\s*:(?!:)/g;
const NOT_DATA = /^(using|typedef|friend|static|template|enum|struct|class|union|virtual|explicit|inline|operator|DECLARE_\w*|UE_\w*)\b/;

// One data member name from a member declaration statement, or null when it is not one.
export function declaredName(statement) {
  const s = statement.replace(/\s+/g, ' ').trim();
  if (!s || NOT_DATA.test(s)) return null;
  const cut = s.search(/[={]/);
  let decl = (cut < 0 ? s : s.slice(0, cut)).trim();
  if (decl.includes('(') || decl.includes('~') || /\boperator\b/.test(decl)) return null;
  decl = decl.replace(/\s*:\s*\d+\s*$/, '');
  let angle = 0;
  for (const ch of decl) {
    if (ch === '<') angle++;
    else if (ch === '>') angle--;
    else if (ch === ',' && angle === 0) return null;
  }
  decl = decl.replace(/(\s*\[[^\]]*\])+\s*$/, '');
  const m = decl.match(/^(.*[\s*&>])([A-Za-z_]\w*)$/);
  if (!m || !m[1].trim()) return null;
  return m[2];
}

// Walk one class body at its own depth. Nested types and inline function bodies are skipped;
// a brace initializer (FSimVec Aim{X, Y};) stays part of its statement.
export function parseBody(rawBody, kind) {
  // GENERATED_BODY() has no semicolon, so it would glue itself to the next statement. Blank it in place.
  const body = rawBody.replace(/\bGENERATED_\w*BODY\s*\(\s*\)/g, (s) => ' '.repeat(s.length));
  let access = kind === 'struct' ? 'public' : 'private';
  const members = [];
  const friends = [];
  let buf = '';
  let i = 0;
  // The last access specifier in the pending text wins; everything before it was not a member.
  const takeAccess = () => {
    let last = null;
    let m;
    ACCESS.lastIndex = 0;
    while ((m = ACCESS.exec(buf))) last = m;
    if (last) {
      access = last[1];
      buf = buf.slice(last.index + last[0].length);
    }
  };
  while (i < body.length) {
    const ch = body[i];
    if (ch === '{') {
      takeAccess();
      const head = buf.trim();
      const close = matchBrace(body, i);
      if (close < 0) break;
      if (!head || head.includes('(') || /^(struct|class|enum|union)\b/.test(head)) {
        buf = '';
      } else {
        buf += body.slice(i, close + 1);
      }
      i = close + 1;
      continue;
    }
    if (ch === ';') {
      takeAccess();
      const statement = buf.trim();
      buf = '';
      const friend = statement.match(/^friend\s+(?:class|struct)\s+(\w+)$/);
      if (friend) friends.push(friend[1]);
      else if (access !== 'public') {
        const name = declaredName(statement);
        if (name) members.push({ name, offset: i });
      }
      i++;
      continue;
    }
    buf += ch;
    i++;
  }
  return { members, friends };
}

// Every class or struct definition in a stripped header, with its body range.
export function findClasses(stripped) {
  const classes = [];
  const re = /\b(class|struct)\s+(?:[A-Z][A-Z0-9_]*_API\s+)?([A-Za-z_]\w*)\s*(?:final\s*)?(?::(?!:)[^;{]*)?\{/g;
  let m;
  while ((m = re.exec(stripped))) {
    if (/\benum\s+$/.test(stripped.slice(Math.max(0, m.index - 12), m.index))) continue;
    const open = m.index + m[0].length - 1;
    const close = matchBrace(stripped, open);
    if (close < 0) continue;
    classes.push({ kind: m[1], name: m[2], open, close });
  }
  return classes;
}

function countWord(text, name) {
  const re = new RegExp(`\\b${name}\\b`, 'g');
  return (text.match(re) || []).length;
}

export function scan(root) {
  const files = listSources(root).map((path) => {
    const raw = readFileSync(path, 'utf8');
    return { path, raw, text: strip(raw) };
  });
  const findings = [];
  let classCount = 0;
  let memberCount = 0;
  for (const header of files.filter((f) => /\.(h|hpp)$/i.test(f.path))) {
    const rawLines = header.raw.split('\n');
    for (const cls of findClasses(header.text)) {
      const body = header.text.slice(cls.open + 1, cls.close);
      const { members, friends } = parseBody(body, cls.kind);
      if (!members.length) continue;
      classCount++;
      const owners = [cls.name, ...friends].map((n) => new RegExp(`\\b${n}::`));
      const scope = [body];
      for (const file of files) {
        let text = file.text;
        if (file === header) text = text.slice(0, cls.open) + ' '.repeat(cls.close - cls.open + 1) + text.slice(cls.close + 1);
        if (owners.some((re) => re.test(text))) scope.push(text);
      }
      for (const member of members) {
        memberCount++;
        const uses = scope.reduce((sum, text) => sum + countWord(text, member.name), 0);
        if (uses > 1) continue;
        const line = lineOf(header.text, cls.open + 1 + member.offset);
        if (/lint:allow-unused\b/.test(rawLines[line - 1] ?? '')) continue;
        findings.push({ file: relative(root, header.path).split(sep).join('/'), line, cls: cls.name, name: member.name });
      }
    }
  }
  return { findings, classCount, memberCount };
}

function report(root, result) {
  for (const f of result.findings) {
    console.error(`${f.file}:${f.line}: ${f.cls}::${f.name} is declared but never read or written (class body and files defining ${f.cls}::)`);
  }
  if (result.findings.length) {
    console.error(`unused-members: ${result.findings.length} unused member(s). Remove them, or mark the line "// lint:allow-unused <reason>".`);
    return 1;
  }
  console.log(`unused-members OK: ${result.memberCount} private/protected members in ${result.classCount} classes under ${relative(process.cwd(), root) || '.'}`);
  return 0;
}

// Fixtures: the Wards regression (dead member, while a different class uses the same name), plus the
// shapes that must stay silent (UPROPERTY, static constexpr, brace init, initializer with a call,
// nested types, inline bodies, a friend that touches the member, and the allow marker).
const FIXTURE_HEADER = `#pragma once
#include "Fixture.generated.h"

UCLASS()
class MAGEARENAVR_API UFixtureComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	bool IsReady() const { return bReady; }

private:
	enum class EStep : uint8
	{
		Idle,
		Busy
	};

	struct FSample
	{
		double Time = 0.0;
	};

	void Tick();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Joints;

	static constexpr double PlaneDistanceCm = 40.0;
	bool bReady = false;
	FVector2D Aim{1.0, 2.0};
	bool bStones[3] = {false, false, false};
	FString RunId = TEXT("T08");
	TMap<FString, FString> Strings;
	FDelegateHandle CastHandle;
	TWeakObjectPtr<USigilRecognizerSubsystem> Sigils;
	TWeakObjectPtr<UWardDetectorSubsystem> Wards;
};

class FFriendly
{
private:
	int32 Secret = 0;
	friend class FReader;
};
`;
const FIXTURE_CPP = `#include "Fixture.h"

void UFixtureComponent::Tick()
{
	// Wards is only named in this comment, which does not count.
	const FString Note = TEXT("Wards");
	if (Sigils.IsValid() && CastHandle.IsValid() && bStones[0] && Aim.X > 0.0 && RunId.Len() && Strings.Num())
	{
	}
}

int32 FReader::Read(const FFriendly& In)
{
	return In.Secret;
}
`;
const OTHER_HEADER = `#pragma once

class FOther
{
public:
	void Use();

private:
	TWeakObjectPtr<UWardDetectorSubsystem> Wards;
};
`;
const OTHER_CPP = `#include "Other.h"

void FOther::Use()
{
	Wards.Reset();
}
`;

function writeTree(dir, files) {
  for (const [name, text] of Object.entries(files)) {
    const full = join(dir, name);
    mkdirSync(dirname(full), { recursive: true });
    writeFileSync(full, text);
  }
}

function selfTest() {
  const problems = [];
  const tmp = mkdtempSync(join(tmpdir(), 'mage-arena-unused-'));
  try {
    const fixture = join(tmp, 'fixture');
    writeTree(fixture, { 'Fixture.h': FIXTURE_HEADER, 'Fixture.cpp': FIXTURE_CPP, 'Other.h': OTHER_HEADER, 'Other.cpp': OTHER_CPP });
    const found = scan(fixture).findings.map((f) => `${f.cls}::${f.name}`);
    if (found.join(',') !== 'UFixtureComponent::Wards') problems.push(`fixture: expected only UFixtureComponent::Wards, got [${found.join(', ')}]`);

    const allowed = join(tmp, 'allowed');
    writeTree(allowed, {
      'Fixture.h': FIXTURE_HEADER.replace('UWardDetectorSubsystem> Wards;\n};', 'UWardDetectorSubsystem> Wards; // lint:allow-unused kept for T99\n};'),
      'Fixture.cpp': FIXTURE_CPP,
    });
    const afterAllow = scan(allowed).findings.map((f) => `${f.cls}::${f.name}`);
    if (afterAllow.length) problems.push(`allow marker: expected no findings, got [${afterAllow.join(', ')}]`);

    // The real regression: put the member de92c3c removed back into the real component.
    const realDir = join(defaultRoot, 'MageArenaVR', 'Greybox');
    const realHeader = join(realDir, 'HandPresentationComponent.h');
    const realCpp = join(realDir, 'HandPresentationComponent.cpp');
    const anchor = 'TWeakObjectPtr<USigilRecognizerSubsystem> Sigils;';
    if (existsSync(realHeader) && existsSync(realCpp) && readFileSync(realHeader, 'utf8').includes(anchor)) {
      const real = join(tmp, 'real');
      const header = readFileSync(realHeader, 'utf8').replace(anchor, `${anchor}\n\tTWeakObjectPtr<class UWardDetectorSubsystem> Wards;`);
      writeTree(real, { 'HandPresentationComponent.h': header, 'HandPresentationComponent.cpp': readFileSync(realCpp, 'utf8') });
      const realFound = scan(real).findings.map((f) => `${f.cls}::${f.name}`);
      if (!realFound.includes('UHandPresentationComponent::Wards')) problems.push(`real component: re-added Wards was not flagged (got [${realFound.join(', ')}])`);
      else console.log('self-test: re-added UHandPresentationComponent::Wards is flagged (the de92c3c regression)');
    } else {
      console.log('self-test: HandPresentationComponent anchor moved; real-file case skipped, fixtures still checked');
    }
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
  if (problems.length) {
    console.error(problems.join('\n'));
    process.exit(1);
  }
  console.log('self-test OK: dead member flagged, UPROPERTY/static/brace-init/friend/allow cases stay silent');
  process.exit(0);
}

function main() {
  const { root, selfTest: doSelf } = parseArgs(process.argv.slice(2));
  if (doSelf) selfTest();
  if (!existsSync(root)) fail(`source root not found: ${root}`);
  process.exit(report(root, scan(root)));
}

main();
