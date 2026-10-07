// Runs the pinned TypeScript kernel and writes one JSON vector per scenario.
// The C++ port replays `frames` and `ops`; it does not re-decide these inputs.
import { mkdirSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const outFlag = process.argv.indexOf('--out');
if (outFlag < 0 || !process.argv[outFlag + 1]) {
  console.error('usage: oracle.mjs --out <dir>');
  process.exit(1);
}
const outDir = process.argv[outFlag + 1];
if (!process.env.CONFORMANCE_KERNEL_CACHE) {
  console.error('usage: run through generate.mjs, which sets CONFORMANCE_KERNEL_CACHE to the extracted kernel folder');
  process.exit(1);
}
const cacheRoot = join(process.env.CONFORMANCE_KERNEL_CACHE, 'packages', 'core', 'src', 'arena');

const kernel = await import(pathToFileURL(join(cacheRoot, 'kernel.ts')).href);
const catalog = await import(pathToFileURL(join(cacheRoot, 'catalog.ts')).href);
const trainingMod = await import(pathToFileURL(join(cacheRoot, 'training.ts')).href);
const enemies = await import(pathToFileURL(join(cacheRoot, 'enemies.ts')).href);
const water = await import(pathToFileURL(join(cacheRoot, 'water.ts')).href);
const gamesMod = await import(pathToFileURL(join(cacheRoot, 'games.ts')).href);
const mageAi = await import(pathToFileURL(join(cacheRoot, 'mage-ai.ts')).href);

const {
  addMage, combat, createArena, drainPerSecond, DT, perfectReturn, resetWave, runtime, stateHash, stepArena, ticks,
} = kernel;
const { idleInput } = await import(pathToFileURL(join(cacheRoot, 'types.ts')).href);
const { newWaterState, presets, spellFor } = catalog;
const { advanceGames, createGames, stepGames } = gamesMod;
const { attachMageAI, mageInput } = mageAi;
const { createTraining, stepTraining, timingBot } = trainingMod;
const { addEnemy, enemyInputs, queueDeathEffects } = enemies;
const { hasLineOfSight } = water;

const LINES = ['tide_orb', 'lash', 'mire', 'mend', 'mirror'];
const TOLERANCE = { relative: 1e-6, absolute: 1e-9 };

function fail(name, detail) {
  throw new Error(`${name}: ${detail}`);
}

function frame(aim, extra = {}) {
  return {
    move: { x: extra.move?.x ?? 0, y: extra.move?.y ?? 0 },
    aim: { x: aim.x, y: aim.y },
    slot: extra.slot ?? 0,
    cast: !!extra.cast,
    absorb: !!extra.absorb,
    roll: !!extra.roll,
    sprint: !!extra.sprint,
  };
}

function composition(line, branch = 'A') {
  return {
    name: 'Probe',
    lines: [line, ...LINES.filter((other) => other !== line).slice(0, 2)],
    branches: { lash: branch, mirror: branch, tide_orb: branch },
  };
}

function exportActor(actor) {
  const pending = actor.pending;
  const decoy = actor.water.decoy;
  return {
    id: actor.id,
    team: actor.team,
    label: actor.label,
    x: actor.pos.x,
    y: actor.pos.y,
    px: actor.previousPos.x,
    py: actor.previousPos.y,
    fx: actor.facing.x,
    fy: actor.facing.y,
    hp: actor.hp,
    mana: actor.mana,
    stamina: actor.stamina,
    down: actor.down,
    dummy: actor.dummy,
    absorb: actor.absorb,
    absorbFreshTick: actor.absorbFreshTick,
    releaseTick: actor.releaseTick,
    tier: actor.tier,
    lastUnlockTick: actor.lastUnlockTick,
    clockAdvanceTicks: actor.clockAdvanceTicks,
    flow: actor.water.flow,
    stored: actor.water.stored,
    crests: actor.water.crests,
    healing: actor.water.healing,
    controlTicks: actor.water.controlTicks,
    rootUntil: actor.water.rootUntil,
    encasedUntil: actor.water.encasedUntil,
    slowUntil: actor.water.slowUntil,
    slowMult: actor.water.slowMult,
    wardUntil: actor.water.wardUntil,
    sheenUntil: actor.water.sheenUntil,
    hotUntil: actor.water.hotUntil,
    hotPerTick: actor.water.hotPerTick,
    composition: {
      name: actor.water.composition.name,
      lines: [...actor.water.composition.lines],
      branches: {
        lash: actor.water.composition.branches.lash,
        mirror: actor.water.composition.branches.mirror,
        tide_orb: actor.water.composition.branches.tide_orb,
      },
    },
    cooldowns: { ...actor.water.cooldowns },
    unlocks: [...actor.unlockTicks],
    pendingKind: pending ? pending.kind : null,
    pendingSpell: pending?.spellId ?? null,
    pendingRelease: pending ? pending.releaseTick : null,
    pendingDamageMult: pending?.damageMult ?? null,
    metrics: { ...actor.metrics },
    enemy: actor.enemy
      ? {
          id: actor.enemy.id,
          readyTick: actor.enemy.readyTick,
          backoffUntil: actor.enemy.backoffUntil,
          stunnedUntil: actor.enemy.stunnedUntil,
          attackIndex: actor.enemy.attackIndex,
          deathQueued: actor.enemy.deathQueued,
        }
      : null,
    speedMps: actor.speedMps ?? null,
    decoy: decoy ? { x: decoy.pos.x, y: decoy.pos.y, until: decoy.until } : null,
  };
}

function snapshot(state) {
  return {
    tick: state.tick,
    rng: state.rng,
    nextId: state.nextId,
    stateHash: stateHash(state),
    eventCount: state.events.length,
    randomDraws: state.randomLog.length,
    actors: state.actors.map(exportActor),
    projectiles: state.projectiles.map((p) => ({
      id: p.id,
      ownerId: p.ownerId,
      activationId: p.activationId,
      x: p.pos.x,
      y: p.pos.y,
      px: p.previousPos.x,
      py: p.previousPos.y,
      vx: p.velocity.x,
      vy: p.velocity.y,
      damage: p.damage,
      remainingM: p.remainingM,
      radius: p.radius,
      family: p.family,
      tier: p.tier,
      bolt: !!p.bolt,
      reflected: !!p.reflected,
      piercing: !!p.piercing,
      burst: p.burstRadiusM ?? 0,
      delivery: p.delivery ?? null,
      hitIds: [...p.hitIds],
    })),
    telegraphs: state.telegraphs.map((t) => ({
      id: t.id,
      ownerId: t.ownerId,
      kind: t.kind,
      family: t.family,
      tier: t.tier,
      damage: t.damage,
      resolveTick: t.resolveTick,
      startTick: t.startTick,
      ox: t.origin.x,
      oy: t.origin.y,
      tx: t.target.x,
      ty: t.target.y,
      speedMps: t.speedMps,
      rangeM: t.rangeM,
      widthM: t.widthM,
      survives: !!t.survivesOwner,
    })),
    zones: state.zones.map((z) => ({
      id: z.id,
      ownerId: z.ownerId,
      kind: z.kind,
      x: z.pos.x,
      y: z.pos.y,
      radiusM: z.radiusM,
      until: z.until,
      slowMult: z.slowMult,
    })),
  };
}

function exportSetup(state) {
  return {
    actors: state.actors.map((actor) => ({
      id: actor.id,
      team: actor.team,
      x: actor.pos.x,
      y: actor.pos.y,
      label: actor.label,
      ranks: { vigor: actor.ranks.vigor, focus: actor.ranks.focus, nerve: actor.ranks.nerve },
      tier: actor.tier,
      lastUnlockTick: actor.lastUnlockTick,
      dummy: actor.dummy,
      hp: actor.hp,
      maxHp: actor.maxHp,
      mana: actor.mana,
      composition: {
        name: actor.water.composition.name,
        lines: [...actor.water.composition.lines],
        branches: {
          lash: actor.water.composition.branches.lash,
          mirror: actor.water.composition.branches.mirror,
          tide_orb: actor.water.composition.branches.tide_orb,
        },
      },
      enemy: actor.enemy ? actor.enemy.id : null,
      facing: { x: actor.facing.x, y: actor.facing.y },
      clockAdvanceTicks: actor.clockAdvanceTicks,
      absorb: actor.absorb,
      absorbFreshTick: actor.absorbFreshTick,
      releaseTick: actor.releaseTick,
      absorbExhausted: actor.absorbExhausted,
      flow: actor.water.flow,
    })),
  };
}

function exportEvents(state) {
  return state.events.map((event) => ({
    tick: event.tick,
    kind: event.kind,
    actorId: event.actorId,
    targetId: event.targetId ?? null,
    value: event.value,
  }));
}

function exportGames(games) {
  const result = games.result ?? null;
  return {
    wave: games.wave,
    phase: games.phase,
    wavesCleared: games.wavesCleared,
    playerId: games.player.id,
    resultKind: result ? result.kind : null,
    gold: result ? result.gold : null,
    renown: result ? result.renown : null,
    finalReached: result ? result.finalReached : null,
    finalWon: result ? result.finalWon : null,
  };
}

function drive(spec, ctx, inputs) {
  if (spec.driver === 'training') {
    const input = inputs[ctx.training.player.id] ?? idleInput(ctx.training.dummy.pos);
    stepTraining(ctx.training, input);
    return;
  }
  if (spec.driver === 'games') {
    const overlay = inputs[ctx.games.player.id];
    stepGames(ctx.games, overlay);
    if (spec.autoAdvance && ctx.games.phase === 'intermission') advanceGames(ctx.games);
    return;
  }
  if (spec.driver === 'mage') {
    const merged = {};
    for (const actor of ctx.state.actors) {
      if (actor.mageAI && !actor.down) merged[actor.id] = mageInput(ctx.state, actor);
    }
    for (const [id, input] of Object.entries(inputs)) merged[Number(id)] = input;
    stepArena(ctx.state, merged);
    return;
  }
  if (spec.driver === 'enemies') {
    const merged = enemyInputs(ctx.state);
    for (const [id, input] of Object.entries(inputs)) merged[Number(id)] = input;
    stepArena(ctx.state, merged);
    queueDeathEffects(ctx.state);
    return;
  }
  stepArena(ctx.state, inputs);
}

function applyOp(ctx, op) {
  if (op.op === 'advanceGames') {
    advanceGames(ctx.games);
    return;
  }
  if (op.op === 'clearWave') {
    for (const actor of ctx.games.state.actors) {
      if (actor.id === ctx.games.player.id) continue;
      actor.down = true;
      actor.hp = 0;
      if (actor.enemy) actor.enemy.deathQueued = true;
    }
    ctx.games.state.projectiles = [];
    ctx.games.state.telegraphs = [];
    return;
  }
  const state = ctx.state;
  const actor = state.actors.find((candidate) => candidate.id === op.actorId);
  if (!actor) fail(op.op, `missing actor ${op.actorId}`);
  if (op.op === 'resetWave') resetWave(state, actor);
  else if (op.op === 'setHp') actor.hp = op.hp;
  else if (op.op === 'setMana') actor.mana = op.mana;
  else if (op.op === 'setClockAdvanceTicks') actor.clockAdvanceTicks = op.ticks;
  else fail(op.op, 'unknown op');
}

function runScenario(spec) {
  const ctx = spec.create();
  ctx.checkpoints = [];
  const setup = exportSetup(ctx.state);
  const frames = [];
  const held = new Map();
  const checkpointEvery = spec.checkpointEvery ?? 30;
  const checkpointAt = new Set(spec.checkpointAt ?? []);
  let stopAt = spec.ticks;

  const pushCheckpoint = (tick, when) => {
    const row = { tick, when, ...snapshot(ctx.state) };
    if (ctx.games) row.games = exportGames(ctx.games);
    ctx.checkpoints.push(row);
  };
  pushCheckpoint(0, 'pre-ops');

  for (let tick = 1; tick <= stopAt; tick++) {
    const desired = spec.inputFor(tick, ctx) ?? {};
    const changed = {};
    let any = false;
    for (const [key, input] of Object.entries(desired)) {
      const id = Number(key);
      const encoded = JSON.stringify(input);
      if (held.get(id)?.encoded !== encoded) {
        changed[String(id)] = input;
        held.set(id, { encoded, input });
        any = true;
      }
    }
    if (any) frames.push({ tick, inputs: changed });
    const inputs = {};
    for (const [id, row] of held) inputs[id] = row.input;
    drive(spec, ctx, inputs);
    if (spec.until && spec.until(ctx)) stopAt = Math.min(stopAt, tick + (spec.tail ?? 0));
    const ops = (spec.ops ?? []).filter((op) => op.afterTick === tick);
    const sampled = typeof spec.sample === 'function' && spec.sample(ctx, tick);
    const want = tick % checkpointEvery === 0 || tick === stopAt || checkpointAt.has(tick) || ops.length > 0 || sampled;
    if (want) pushCheckpoint(tick, 'pre-ops');
    for (const op of ops) applyOp(ctx, op);
    if (ops.length) pushCheckpoint(tick, 'post-ops');
    if (tick === stopAt) break;
  }
  spec.expect(ctx, spec.name);
  if (spec.name === 'bolt-hit') {
    const sample = {
      hash: stateHash(ctx.state),
      keys: {
        actor: Object.keys(ctx.state.actors[0]),
        water: Object.keys(ctx.state.actors[0].water),
        metrics: Object.keys(ctx.state.actors[0].metrics),
        input: Object.keys(ctx.state.actors[0].lastInput),
      },
      json: JSON.parse(JSON.stringify(ctx.state)),
    };
    writeFileSync(join(cacheRoot, 'sample-state.json'), JSON.stringify(sample, null, 2));
  }
  const vector = {
    name: spec.name,
    note: spec.note,
    seed: spec.seed,
    driver: spec.driver,
    trainingKind: spec.trainingKind ?? null,
    checkpointEvery,
    tolerance: TOLERANCE,
    ticks: stopAt,
    replaySetup: spec.driver !== 'training' && spec.driver !== 'games',
    setup,
    frames,
    ops: spec.ops ?? [],
    events: exportEvents(ctx.state),
    checkpoints: ctx.checkpoints,
  };
  if (spec.driver === 'games') {
    vector.games = {
      startWave: spec.startWave ?? 0,
      referencePlayer: !!spec.referencePlayer,
      preset: spec.preset ?? null,
      autoAdvance: !!spec.autoAdvance,
    };
    if (spec.playerHp !== undefined) vector.games.playerHp = spec.playerHp;
    if (spec.playerMana !== undefined) vector.games.playerMana = spec.playerMana;
  }
  if (spec.mageAttachments) vector.mageAttachments = spec.mageAttachments;
  return vector;
}

function boltHitTick(distance) {
  const state = createArena(1);
  const player = addMage(state, 0, { x: 10, y: 10 }, 'P');
  const foe = addMage(state, 1, { x: 10 + distance, y: 10 }, 'F');
  let hitTick = -1;
  for (let tick = 1; tick <= 80 && hitTick < 0; tick++) {
    stepArena(state, {
      [player.id]: frame({ x: 10 + distance, y: 10 }, { absorb: true }),
      [foe.id]: frame({ x: 10, y: 10 }, { cast: tick === 1, slot: 0 }),
    });
    if (state.events.some((event) => event.kind === 'hit' && event.actorId === player.id)) hitTick = state.tick;
  }
  return { hitTick, age: hitTick < 0 ? null : hitTick - 1, perfects: state.actors[0].metrics.perfects };
}

function distanceForAge(age) {
  let found = null;
  for (let step = 0; step <= 1200; step++) {
    const distance = 0.8 + step * 0.01;
    const probe = boltHitTick(distance);
    if (probe.age === age) {
      found = { distance, ...probe };
      break;
    }
  }
  if (!found) fail('window', `no bolt distance hits at age ${age}`);
  return found;
}

function tideOrbHitTick(distance) {
  const state = createArena(1);
  const player = mage(state, 0, 10, 10, 'P');
  const foe = dress(mage(state, 1, 10 + distance, 10, 'F'), 'tide_orb', 1, 'A');
  let hitTick = -1;
  for (let tick = 1; tick <= 80 && hitTick < 0; tick++) {
    stepArena(state, {
      [foe.id]: frame({ x: 10, y: 10 }, { slot: 1, cast: tick === 1 }),
    });
    if (state.events.some((event) => event.kind === 'hit' && event.actorId === player.id)) hitTick = state.tick;
  }
  return hitTick;
}

const windowTicks = ticks(combat.absorb.perfect.windowS);
const tierOneDistance = 2.6;
const tierOneHit = tideOrbHitTick(tierOneDistance);
const tierOneRaise = tierOneHit - windowTicks;
if (tierOneRaise < 1) fail('mana-nerves', `tide orb hits too soon at tick ${tierOneHit}`);
const inside = distanceForAge(windowTicks);
const outside = distanceForAge(windowTicks + 1);
if (inside.perfects !== 1) fail('window', `inside age ${inside.age} perfects ${inside.perfects}`);
if (outside.perfects !== 0) fail('window', `outside age ${outside.age} perfects ${outside.perfects}`);
console.log(`perfect window distances: inside ${inside.distance} m (hit ${inside.hitTick}), outside ${outside.distance} m (hit ${outside.hitTick})`);

function mage(state, team, x, y, label, ranks) {
  return addMage(state, team, { x, y }, label, ranks);
}

function dress(actor, line, tier, branch, extra = {}) {
  actor.water = newWaterState(composition(line, branch));
  actor.tier = tier;
  actor.lastUnlockTick = 1e9;
  if (extra.hp !== undefined) actor.hp = extra.hp;
  if (extra.mana !== undefined) actor.mana = extra.mana;
  if (extra.flow !== undefined) actor.water.flow = extra.flow;
  return actor;
}

const scenarios = [];

function add(spec) {
  scenarios.push({ checkpointEvery: 30, checkpointAt: [], ops: [], tail: 0, trainingKind: null, ...spec });
}

add({
  name: 'bolt-hit',
  note: 'A Rain Needle cast from slot 0 hits the other mage.',
  seed: 17,
  driver: 'arena',
  ticks: 40,
  create() {
    const state = createArena(17);
    const caster = mage(state, 0, 10, 10, 'Caster');
    const target = mage(state, 1, 16, 10, 'Target');
    return { state, caster, target };
  },
  inputFor(tick, ctx) {
    return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 0, cast: tick === 1 }) };
  },
  expect(ctx, name) {
    const loss = ctx.target.maxHp - ctx.target.hp;
    if (Math.abs(loss - catalog.spells.find((spell) => spell.line === 'bolt').damage) > 1e-6) {
      fail(name, `bolt damage ${loss}`);
    }
    if (!ctx.state.events.some((event) => event.kind === 'hit' && event.actorId === ctx.target.id)) fail(name, 'no hit');
  },
});

function waterDamage(name, note, line, tier, branch, range, check) {
  add({
    name,
    note,
    seed: 11,
    driver: 'arena',
    ticks: 1,
    create() {
      const state = createArena(11);
      const caster = dress(mage(state, 0, 10, 10, 'Caster'), line, tier, branch);
      const target = mage(state, 1, 10 + range, 10, 'Target');
      const spell = spellFor(caster, 1);
      const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
      const travel = spell.kind === 'projectile' ? Math.ceil((range / spell.speedMps) * combat.simStepHz) + 8 : 4;
      this.ticks = release + travel;
      this.checkpointAt = [release, this.ticks];
      return { state, caster, target, spell, release };
    },
    inputFor(tick, ctx) {
      return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
    },
    expect(ctx, scenarioName) {
      check(ctx, scenarioName);
    },
  });
}

waterDamage('tide-orb-1', 'Tide Orb I, Bubble Shot, hits for the CSV damage.', 'tide_orb', 1, 'A', 2.6, (ctx, name) => {
  if (Math.abs(ctx.target.maxHp - ctx.target.hp - ctx.spell.damage) > 1e-6) fail(name, `hp loss ${ctx.target.maxHp - ctx.target.hp}`);
});
waterDamage('tide-orb-2', 'Tide Orb II, Crash Orb, damages once (burst dedups the direct target).', 'tide_orb', 2, 'A', 2.6, (ctx, name) => {
  if (Math.abs(ctx.target.maxHp - ctx.target.hp - ctx.spell.damage) > 1e-6) fail(name, `hp loss ${ctx.target.maxHp - ctx.target.hp}, expected one hit of ${ctx.spell.damage}`);
});
waterDamage('tide-orb-4a', 'Tide Orb IV-A, Leviathan Orb, unblockable projectile hits.', 'tide_orb', 4, 'A', 4, (ctx, name) => {
  if (ctx.spell.family !== 'unblockable') fail(name, 'leviathan should be unblockable');
  if (Math.abs(ctx.target.maxHp - ctx.target.hp - ctx.spell.damage) > 1e-6) fail(name, `hp loss ${ctx.target.maxHp - ctx.target.hp}`);
});

function waterCount(name, note, line, tier, branch, range) {
  add({
    name,
    note,
    seed: 11,
    driver: 'arena',
    ticks: 1,
    create() {
      const state = createArena(11);
      const caster = dress(mage(state, 0, 10, 10, 'Caster'), line, tier, branch);
      const target = mage(state, 1, 10 + range, 10, 'Target');
      const spell = spellFor(caster, 1);
      this.ticks = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
      return { state, caster, target, spell };
    },
    inputFor(tick, ctx) {
      return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
    },
    expect(ctx, scenarioName) {
      if (ctx.state.projectiles.length !== ctx.spell.count) {
        fail(scenarioName, `projectiles ${ctx.state.projectiles.length} wanted ${ctx.spell.count}`);
      }
      const first = ctx.state.projectiles[0];
      const last = ctx.state.projectiles[ctx.state.projectiles.length - 1];
      if (ctx.spell.count > 1 && Math.abs(first.velocity.y + last.velocity.y) > 1e-6) {
        fail(scenarioName, `fan not symmetric ${first.velocity.y} ${last.velocity.y}`);
      }
    },
  });
}

waterCount('tide-orb-3', 'Tide Orb III, Twin Tides, two symmetric projectiles.', 'tide_orb', 3, 'A', 10);
waterCount('tide-orb-4b', 'Tide Orb IV-B, Rain of Orbs, five-orb fan.', 'tide_orb', 4, 'B', 10);

waterDamage('lash-1', 'Lash I cone hits the mage in front.', 'lash', 1, 'A', 2.6, (ctx, name) => {
  if (Math.abs(ctx.target.maxHp - ctx.target.hp - ctx.spell.damage) > 1e-6) fail(name, `hp loss ${ctx.target.maxHp - ctx.target.hp}`);
});

function lashPush(name, note, branch, expectGreater) {
  add({
    name,
    note,
    seed: 11,
    driver: 'arena',
    ticks: 1,
    create() {
      const state = createArena(11);
      const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'lash', 2, branch);
      const target = mage(state, 1, 12.6, 10, 'Target');
      const spell = spellFor(caster, 1);
      this.ticks = 1 + ticks(Math.max(spell.castS, spell.telegraphS)) + 2;
      return { state, caster, target, spell, initialX: target.pos.x };
    },
    inputFor(tick, ctx) {
      return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
    },
    expect(ctx, scenarioName) {
      const moved = expectGreater ? ctx.target.pos.x > ctx.initialX : ctx.target.pos.x < ctx.initialX;
      if (!moved) fail(scenarioName, `x ${ctx.target.pos.x} from ${ctx.initialX}`);
    },
  });
}

lashPush('lash-2a', 'Lash II-A Undertow pulls the target.', 'A', false);
lashPush('lash-2b', 'Lash II-B Riptide pushes the target.', 'B', true);

add({
  name: 'lash-3',
  note: 'Lash III ring hits a mage behind the caster as well as in front.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'lash', 3, 'A');
    const front = mage(state, 1, 12.6, 10, 'Front');
    const behind = mage(state, 1, 8, 10, 'Behind');
    const spell = spellFor(caster, 1);
    this.ticks = 1 + ticks(Math.max(spell.castS, spell.telegraphS)) + 2;
    return { state, caster, front, behind, spell };
  },
  inputFor(tick, ctx) {
    return { [ctx.caster.id]: frame(ctx.front.pos, { slot: 1, cast: tick === 1 }) };
  },
  expect(ctx, name) {
    if (Math.abs(ctx.front.maxHp - ctx.front.hp - ctx.spell.damage) > 1e-6) fail(name, 'front missed');
    if (Math.abs(ctx.behind.maxHp - ctx.behind.hp - ctx.spell.damage) > 1e-6) fail(name, 'behind missed');
  },
});

add({
  name: 'lash-4',
  note: 'Lash IV Drown Coil roots after the unblockable warning.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'lash', 4, 'A');
    const target = mage(state, 1, 12, 10, 'Target');
    const spell = spellFor(caster, 1);
    const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
    this.ticks = release + 2;
    this.checkpointAt = [release - 1, release, this.ticks];
    return { state, caster, target, spell, release };
  },
  inputFor(tick, ctx) {
    const inputs = { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
    if (tick > ctx.release) inputs[ctx.target.id] = frame({ x: 20, y: 10 }, { move: { x: 1, y: 0 }, roll: true });
    return inputs;
  },
  expect(ctx, name) {
    const pre = ctx.checkpoints.find((row) => row.tick === ctx.release - 1 && row.when === 'pre-ops');
    const rooted = pre.actors.find((actor) => actor.id === ctx.target.id);
    if (rooted.rootUntil !== 0) fail(name, 'rooted before the warning ended');
    if (ctx.target.water.rootUntil <= ctx.state.tick) fail(name, 'not rooted after release');
    if (ctx.target.pos.x !== 12) fail(name, `rooted target moved to ${ctx.target.pos.x}`);
  },
});

add({
  name: 'mire-1',
  note: 'Mire I Puddle slows the mage standing in it.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mire', 1, 'A');
    const target = mage(state, 1, 12.6, 10, 'Target');
    const spell = spellFor(caster, 1);
    const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
    this.ticks = release + 1;
    return { state, caster, target, spell, release, xAtRelease: null };
  },
  inputFor(tick, ctx) {
    const inputs = { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
    if (tick === ctx.release + 1) inputs[ctx.target.id] = frame({ x: 20, y: 10 }, { move: { x: 1, y: 0 } });
    return inputs;
  },
  expect(ctx, name) {
    const step = combat.movement.walkMps * (1 - ctx.spell.amount) * DT;
    const moved = ctx.target.pos.x - 12.6;
    if (Math.abs(moved - step) > 1e-6) fail(name, `slow step ${moved} wanted ${step}`);
  },
});

add({
  name: 'mire-2',
  note: 'Mire II Fog Bank blocks line of sight while the zone is up.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mire', 2, 'A');
    const target = mage(state, 1, 12.6, 10, 'Target');
    const spell = spellFor(caster, 1);
    this.ticks = 1 + ticks(Math.max(spell.castS, spell.telegraphS)) + 5;
    return { state, caster, target };
  },
  inputFor(tick, ctx) {
    return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
  },
  expect(ctx, name) {
    if (hasLineOfSight(ctx.state, ctx.caster.pos, ctx.target.pos)) fail(name, 'fog did not block sight');
    if (!ctx.state.zones.some((zone) => zone.kind === 'fog')) fail(name, 'no fog zone');
  },
});

waterDamage('mire-3', 'Mire III Freeze Field roots at its centre and deals its damage.', 'mire', 3, 'A', 2.6, (ctx, name) => {
  if (ctx.target.water.rootUntil <= ctx.state.tick) fail(name, 'not rooted');
  if (Math.abs(ctx.target.maxHp - ctx.target.hp - ctx.spell.damage) > 1e-6) fail(name, `hp loss ${ctx.target.maxHp - ctx.target.hp}`);
});

add({
  name: 'mire-4',
  note: 'Mire IV Glacier Tomb clears Flow and encases the target.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mire', 4, 'A');
    const target = mage(state, 1, 14, 10, 'Target');
    target.water.flow = 4;
    const spell = spellFor(caster, 1);
    const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
    this.ticks = release + 2;
    return { state, caster, target, release };
  },
  inputFor(tick, ctx) {
    const inputs = { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
    if (tick > ctx.release) inputs[ctx.target.id] = frame(ctx.caster.pos, { cast: true, move: { x: 1, y: 0 } });
    return inputs;
  },
  expect(ctx, name) {
    if (ctx.target.water.flow !== 0) fail(name, 'flow not cleared');
    if (ctx.target.water.encasedUntil <= ctx.state.tick) fail(name, 'not encased');
    if (ctx.target.metrics.casts !== 0) fail(name, 'encased target cast');
    if (ctx.target.pos.x !== 14) fail(name, 'encased target moved');
  },
});

add({
  name: 'mend-1',
  note: 'Mend I Cool Draught heals the caster.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mend', 1, 'A', { hp: 50 });
    const target = mage(state, 1, 14, 10, 'Target');
    const spell = spellFor(caster, 1);
    this.ticks = 1 + ticks(Math.max(spell.castS, spell.telegraphS)) + 2;
    return { state, caster, target, spell };
  },
  inputFor(tick, ctx) {
    return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
  },
  expect(ctx, name) {
    if (Math.abs(ctx.caster.hp - (50 + ctx.spell.amount)) > 1e-6) fail(name, `hp ${ctx.caster.hp}`);
  },
});

add({
  name: 'mend-2',
  note: 'Mend II Tide Ward cuts absorb drain on the following tick.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mend', 2, 'A');
    const target = mage(state, 1, 14, 10, 'Target');
    const spell = spellFor(caster, 1);
    const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
    this.ticks = release + 1;
    return { state, caster, target, spell, release };
  },
  inputFor(tick, ctx) {
    const cast = tick === 1;
    const absorb = tick === ctx.release + 1;
    return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast, absorb }) };
  },
  expect(ctx, name) {
    const expected = drainPerSecond(ctx.caster.ranks.nerve) * (1 - ctx.spell.amount) * DT;
    if (Math.abs(ctx.caster.metrics.manaDrained - expected) > 1e-6) {
      fail(name, `drained ${ctx.caster.metrics.manaDrained} wanted ${expected}`);
    }
  },
});

add({
  name: 'mend-3',
  note: 'Mend III Spring heals over its duration.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  checkpointEvery: 60,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mend', 3, 'A', { hp: 50 });
    const target = mage(state, 1, 14, 10, 'Target');
    const spell = spellFor(caster, 1);
    this.ticks = 1 + ticks(Math.max(spell.castS, spell.telegraphS)) + ticks(spell.durationS) + 2;
    return { state, caster, target, spell };
  },
  inputFor(tick, ctx) {
    return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 1, cast: tick === 1 }) };
  },
  expect(ctx, name) {
    if (Math.abs(ctx.caster.hp - 80) > 1e-4) fail(name, `hp ${ctx.caster.hp}`);
  },
});

add({
  name: 'mend-4',
  note: 'Mend IV Font restores mana and roots the caster while the cast is in hand.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mend', 4, 'A', { mana: 10 });
    const target = mage(state, 1, 14, 10, 'Target');
    const spell = spellFor(caster, 1);
    const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
    this.ticks = release + 2;
    return { state, caster, target, spell, release };
  },
  inputFor(tick, ctx) {
    // The cast starts after movement on tick 1, so the root exists on the following ticks.
    const rooted = tick > 1 && tick <= ctx.release;
    return {
      [ctx.caster.id]: frame(ctx.target.pos, {
        slot: 1,
        cast: tick === 1,
        move: rooted ? { x: 1, y: 0 } : { x: 0, y: 0 },
        roll: rooted,
      }),
    };
  },
  expect(ctx, name) {
    if (ctx.caster.pos.x !== 10) fail(name, `font let the caster walk to ${ctx.caster.pos.x}`);
    if (ctx.caster.mana <= 70) fail(name, `mana ${ctx.caster.mana}`);
  },
});

add({
  name: 'mirror-1',
  note: 'Mirror I Sheen increases the mana a perfect absorb returns.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mirror', 1, 'A');
    const distance = inside.distance;
    const foe = mage(state, 1, 10 + distance, 10, 'Foe');
    const spell = spellFor(caster, 1);
    const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
    this.ticks = release + inside.hitTick + 2;
    return { state, caster, foe, spell, release, distance };
  },
  inputFor(tick, ctx) {
    const raisedAt = ctx.release + 1;
    return {
      [ctx.caster.id]: frame(ctx.foe.pos, { slot: 1, cast: tick === 1, absorb: tick >= raisedAt }),
      [ctx.foe.id]: frame(ctx.caster.pos, { slot: 0, cast: tick === raisedAt }),
    };
  },
  expect(ctx, name) {
    const plain = perfectReturn(0, ctx.caster.ranks.nerve);
    const boosted = plain * (1 + ctx.spell.amount);
    if (ctx.caster.metrics.perfects !== 1) fail(name, `perfects ${ctx.caster.metrics.perfects}`);
    if (Math.abs(ctx.caster.metrics.manaReturned - boosted) > 1e-6) {
      fail(name, `returned ${ctx.caster.metrics.manaReturned} wanted ${boosted}`);
    }
  },
});

add({
  name: 'mirror-2a',
  note: 'Mirror II-A Reflection returns a perfected magic bolt.',
  seed: 11,
  driver: 'arena',
  ticks: inside.hitTick + 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mirror', 2, 'A');
    const foe = mage(state, 1, 10 + inside.distance, 10, 'Foe');
    return { state, caster, foe };
  },
  inputFor(tick, ctx) {
    return {
      [ctx.caster.id]: frame(ctx.foe.pos, { absorb: true }),
      [ctx.foe.id]: frame(ctx.caster.pos, { slot: 0, cast: tick === 1 }),
    };
  },
  expect(ctx, name) {
    if (ctx.caster.metrics.perfects !== 1) fail(name, `perfects ${ctx.caster.metrics.perfects}`);
    if (!ctx.state.projectiles.some((projectile) => projectile.reflected && projectile.ownerId === ctx.caster.id)) {
      fail(name, 'no reflected projectile');
    }
  },
});

add({
  name: 'mirror-2b',
  note: 'Mirror II-B Ripple deals a ring of damage when a perfect absorb lands.',
  seed: 11,
  driver: 'arena',
  ticks: 30,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mirror', 2, 'B');
    const foe = mage(state, 1, 12, 10, 'Foe');
    const spell = catalog.spells.find((row) => row.effect === 'ripple');
    this.ticks = 20;
    return { state, caster, foe, spell };
  },
  inputFor(tick, ctx) {
    return {
      [ctx.caster.id]: frame(ctx.foe.pos, { absorb: true }),
      [ctx.foe.id]: frame(ctx.caster.pos, { slot: 0, cast: tick === 1 }),
    };
  },
  expect(ctx, name) {
    if (ctx.caster.metrics.perfects !== 1) fail(name, `perfects ${ctx.caster.metrics.perfects}`);
    if (Math.abs(ctx.foe.maxHp - ctx.foe.hp - ctx.spell.damage) > 1e-6) {
      fail(name, `ripple damage ${ctx.foe.maxHp - ctx.foe.hp}`);
    }
  },
});

add({
  name: 'mirror-3',
  note: 'Mirror III Mirage places a decoy, then the decoy expires.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mirror', 3, 'A');
    const target = mage(state, 1, 14, 10, 'Target');
    const spell = spellFor(caster, 1);
    const release = 1 + ticks(Math.max(spell.castS, spell.telegraphS));
    this.ticks = release + ticks(spell.durationS) + 2;
    this.checkpointAt = [release + 1];
    return { state, caster, target, spell, release };
  },
  inputFor(tick, ctx) {
    return { [ctx.caster.id]: frame({ x: 14, y: 10 }, { slot: 1, cast: tick === 1 }) };
  },
  expect(ctx, name) {
    const mid = ctx.checkpoints.find((row) => row.tick === ctx.release + 1 && row.when === 'pre-ops');
    const caster = mid.actors.find((actor) => actor.id === ctx.caster.id);
    if (!caster.decoy) fail(name, 'decoy missing while the duration is running');
    if (ctx.caster.water.decoy) fail(name, 'decoy did not expire');
  },
});

add({
  name: 'mirror-4',
  note: 'Mirror IV Return Tide stores absorbed damage and releases it as a piercing wave.',
  seed: 11,
  driver: 'arena',
  ticks: 80,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'mirror', 4, 'A');
    const foe = mage(state, 1, 13, 10, 'Foe');
    return { state, caster, foe };
  },
  inputFor(tick, ctx) {
    const absorb = tick < 40;
    const cast = tick === 50;
    return {
      [ctx.caster.id]: frame(ctx.foe.pos, { slot: 1, absorb, cast }),
      [ctx.foe.id]: frame(ctx.caster.pos, { slot: 0, cast: tick === 1 }),
    };
  },
  expect(ctx, name) {
    if (ctx.caster.water.stored !== 0) fail(name, `stored ${ctx.caster.water.stored}`);
    if (!ctx.state.events.some((event) => event.kind === 'hit' && event.actorId === ctx.caster.id)) fail(name, 'never absorbed');
    if (!ctx.state.projectiles.some((projectile) => projectile.piercing && projectile.ownerId === ctx.caster.id) && ctx.caster.metrics.casts < 1) {
      fail(name, 'return tide did not cast');
    }
    if (ctx.caster.metrics.casts < 1) fail(name, 'no cast');
  },
});

add({
  name: 'flow-crest',
  note: 'Alternating lines reach Flow 5, and the next cast is a free Crest.',
  seed: 11,
  driver: 'arena',
  ticks: 1,
  checkpointEvery: 1000,
  create() {
    const state = createArena(11);
    const caster = dress(mage(state, 0, 10, 10, 'Caster'), 'tide_orb', 1, 'A');
    caster.water = newWaterState({
      name: 'Probe',
      lines: ['tide_orb', 'lash', 'mirror'],
      branches: { lash: 'A', mirror: 'A', tide_orb: 'A' },
    });
    const target = mage(state, 1, 13, 10, 'Target');
    const gap = ticks(1.5);
    const slots = [0, 1, 2, 0, 1, 2];
    const castTicks = slots.map((_, index) => 1 + index * gap);
    const sixth = castTicks[5];
    // Lash is still in hand on the cast tick. Empty mana on the tick after it releases,
    // then Crest on the next tick, before idle reset and before mana regen can afford a Bolt.
    const lash = spellFor(caster, 2);
    const crest = sixth + ticks(Math.max(lash.castS, lash.telegraphS)) + 2;
    this.ticks = crest;
    this.checkpointAt = [sixth, crest];
    this.ops = [{ afterTick: crest - 1, op: 'setMana', actorId: caster.id, mana: 0 }];
    return { state, caster, target, slots, castTicks, sixth, crest };
  },
  inputFor(tick, ctx) {
    const index = ctx.castTicks.indexOf(tick);
    if (tick === ctx.crest) return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 0, cast: true }) };
    if (index >= 0) return { [ctx.caster.id]: frame(ctx.target.pos, { slot: ctx.slots[index], cast: true }) };
    return { [ctx.caster.id]: frame(ctx.target.pos, { slot: 0, cast: false }) };
  },
  expect(ctx, name) {
    const atFive = ctx.checkpoints.find((row) => row.tick === ctx.sixth && row.when === 'pre-ops');
    const flow = atFive.actors.find((actor) => actor.id === ctx.caster.id).flow;
    if (flow !== 5) fail(name, `flow at cast six is ${flow}`);
    if (ctx.caster.water.crests !== 1 || ctx.caster.water.flow !== 0) {
      fail(name, `crests ${ctx.caster.water.crests} flow ${ctx.caster.water.flow}`);
    }
    if (ctx.caster.metrics.casts !== 7) fail(name, `casts ${ctx.caster.metrics.casts}`);
    if (ctx.caster.mana >= 3) fail(name, `crest spent mana, left ${ctx.caster.mana}`);
  },
});

function wardDuel(name, note, distance, expectPerfect) {
  add({
    name,
    note,
    seed: 17,
    driver: 'arena',
    ticks: 40,
    create() {
      const state = createArena(17);
      const player = mage(state, 0, 10, 10, 'Player');
      const foe = mage(state, 1, 10 + distance, 10, 'Foe');
      this.ticks = 25;
      return { state, player, foe };
    },
    inputFor(tick, ctx) {
      return {
        [ctx.player.id]: frame(ctx.foe.pos, { absorb: true }),
        [ctx.foe.id]: frame(ctx.player.pos, { slot: 0, cast: tick === 1 }),
      };
    },
    expect(ctx, scenarioName) {
      if (expectPerfect && ctx.player.metrics.perfects !== 1) fail(scenarioName, `perfects ${ctx.player.metrics.perfects}`);
      if (!expectPerfect && ctx.player.metrics.perfects !== 0) fail(scenarioName, `perfects ${ctx.player.metrics.perfects}`);
      if (!expectPerfect && ctx.player.metrics.blocks < 1) fail(scenarioName, 'held hit was not a block');
      if (ctx.player.metrics.hits < 1) fail(scenarioName, 'no hit');
    },
  });
}

wardDuel('perfect-inside', 'A fresh ward perfects a bolt on the last tick of the window.', inside.distance, true);
wardDuel('perfect-outside', 'One tick past the window is a block, not a perfect.', outside.distance, false);

add({
  name: 'perfect-advance',
  note: 'A perfect absorb advances the tier clock, so tier II arrives before 15 s.',
  seed: 17,
  driver: 'arena',
  ticks: 1,
  checkpointEvery: 120,
  create() {
    const state = createArena(17);
    const player = mage(state, 0, 10, 10, 'Player');
    const foe = mage(state, 1, 10 + inside.distance, 10, 'Foe');
    const expected = ticks(combat.tierClock.unlockAtSeconds['2']) - ticks(combat.tierClock.perfectAbsorbAdvanceS);
    this.ticks = expected;
    this.checkpointAt = [inside.hitTick, expected];
    return { state, player, foe, expected };
  },
  inputFor(tick, ctx) {
    return {
      [ctx.player.id]: frame(ctx.foe.pos, { absorb: tick <= inside.hitTick }),
      [ctx.foe.id]: frame(ctx.player.pos, { slot: 0, cast: tick === 1 }),
    };
  },
  expect(ctx, name) {
    if (ctx.player.metrics.perfects !== 1) fail(name, `perfects ${ctx.player.metrics.perfects}`);
    if (ctx.player.tier !== 2) fail(name, `tier ${ctx.player.tier}`);
    if (ctx.player.unlockTicks[1] !== ctx.expected) fail(name, `unlock ${ctx.player.unlockTicks.join(',')}`);
    if (ctx.expected >= ticks(combat.tierClock.unlockAtSeconds['2'])) fail(name, 'advance did not beat the clock');
  },
});

add({
  name: 'held-ward',
  note: 'A ward that stays raised only blocks a training stream; later hits are not perfect.',
  seed: 5,
  driver: 'training',
  trainingKind: 'stream',
  ticks: ticks(8),
  checkpointEvery: 60,
  create() {
    const row = createTraining('stream', 5);
    return { state: row.state, training: row, player: row.player, dummy: row.dummy };
  },
  inputFor(_tick, ctx) {
    return { [ctx.player.id]: frame(ctx.dummy.pos, { absorb: true }) };
  },
  expect(ctx, name) {
    if (ctx.player.metrics.perfects !== 0) fail(name, `perfects ${ctx.player.metrics.perfects}`);
    if (ctx.player.metrics.hits < 2 || ctx.player.metrics.blocks < 2) {
      fail(name, `hits ${ctx.player.metrics.hits} blocks ${ctx.player.metrics.blocks}`);
    }
  },
});

add({
  name: 'physical-ward',
  note: 'A physical training shot is reduced by the ward and is never perfect.',
  seed: 5,
  driver: 'training',
  trainingKind: 'physical',
  ticks: ticks(6),
  checkpointEvery: 60,
  create() {
    const row = createTraining('physical', 5);
    return { state: row.state, training: row, player: row.player, dummy: row.dummy };
  },
  inputFor(_tick, ctx) {
    return { [ctx.player.id]: frame(ctx.dummy.pos, { absorb: true }) };
  },
  expect(ctx, name) {
    if (ctx.player.metrics.perfects !== 0) fail(name, 'physical perfected');
    if (ctx.player.metrics.blocks < 1 || ctx.player.metrics.damageTaken <= 0) {
      fail(name, `blocks ${ctx.player.metrics.blocks} taken ${ctx.player.metrics.damageTaken}`);
    }
  },
});

add({
  name: 'unblockable-ward',
  note: 'A charge lane is unblockable: the ward does not reduce it and does not perfect.',
  seed: 5,
  driver: 'training',
  trainingKind: 'charge',
  ticks: ticks(6),
  checkpointEvery: 60,
  create() {
    const row = createTraining('charge', 5);
    return { state: row.state, training: row, player: row.player, dummy: row.dummy };
  },
  inputFor(_tick, ctx) {
    return { [ctx.player.id]: frame(ctx.dummy.pos, { absorb: true }) };
  },
  expect(ctx, name) {
    const damage = runtime.training.charge.damage;
    if (ctx.player.metrics.perfects !== 0) fail(name, 'unblockable perfected');
    if (ctx.player.metrics.blocks !== 0) fail(name, `blocks ${ctx.player.metrics.blocks}`);
    if (ctx.player.metrics.hits < 1 || Math.abs(ctx.player.metrics.damageTaken - ctx.player.metrics.hits * damage) > 1e-6) {
      fail(name, `taken ${ctx.player.metrics.damageTaken} over ${ctx.player.metrics.hits} hits`);
    }
  },
});

add({
  name: 'tier-clock',
  note: 'With no perfects, tiers unlock at 0, 15, 30 and 45 seconds.',
  seed: 2,
  driver: 'arena',
  ticks: ticks(45),
  checkpointEvery: 300,
  create() {
    const state = createArena(2);
    const player = mage(state, 0, 10, 10, 'Player');
    return { state, player };
  },
  inputFor() {
    return {};
  },
  expect(ctx, name) {
    const want = [0, ticks(15), ticks(30), ticks(45)];
    if (JSON.stringify(ctx.player.unlockTicks) !== JSON.stringify(want)) {
      fail(name, `unlocks ${ctx.player.unlockTicks.join(',')} wanted ${want.join(',')}`);
    }
    if (ctx.player.tier !== 4) fail(name, `tier ${ctx.player.tier}`);
  },
});

add({
  name: 'mana-nerves',
  note: 'Nerve 5 drains less and refunds more than Nerve 1 on the same tier-I perfect.',
  seed: 19,
  driver: 'arena',
  ticks: tierOneHit + 2,
  create() {
    const state = createArena(19);
    const low = mage(state, 0, 10, 10, 'Nerve 1', { vigor: 1, focus: 1, nerve: 1 });
    const high = mage(state, 0, 10, 20, 'Nerve 5', { vigor: 1, focus: 1, nerve: 5 });
    // Leave headroom so the refund is the formula, not the mana cap.
    low.mana = 20;
    high.mana = 20;
    const foeLow = dress(mage(state, 1, 10 + tierOneDistance, 10, 'Foe 1'), 'tide_orb', 1, 'A');
    const foeHigh = dress(mage(state, 1, 10 + tierOneDistance, 20, 'Foe 5'), 'tide_orb', 1, 'A');
    return { state, low, high, foeLow, foeHigh };
  },
  inputFor(tick, ctx) {
    const absorb = tick >= tierOneRaise;
    return {
      [ctx.low.id]: frame(ctx.foeLow.pos, { absorb }),
      [ctx.high.id]: frame(ctx.foeHigh.pos, { absorb }),
      [ctx.foeLow.id]: frame(ctx.low.pos, { slot: 1, cast: tick === 1 }),
      [ctx.foeHigh.id]: frame(ctx.high.pos, { slot: 1, cast: tick === 1 }),
    };
  },
  expect(ctx, name) {
    for (const actor of [ctx.low, ctx.high]) {
      const want = perfectReturn(1, actor.ranks.nerve);
      if (actor.metrics.perfects !== 1) fail(name, `${actor.label} perfects ${actor.metrics.perfects}`);
      if (Math.abs(actor.metrics.manaReturned - want) > 1e-6) fail(name, `${actor.label} returned ${actor.metrics.manaReturned} wanted ${want}`);
    }
    if (!(ctx.high.metrics.manaDrained < ctx.low.metrics.manaDrained)) fail(name, 'nerve did not cut drain');
    if (!(ctx.high.metrics.manaReturned > ctx.low.metrics.manaReturned)) fail(name, 'nerve did not raise the refund');
  },
});

add({
  name: 'dummy-wave',
  note: 'A training dummy completes a wave of magic attacks while the timing bot perfects them.',
  seed: 7,
  driver: 'training',
  trainingKind: 'magic',
  ticks: ticks(12),
  checkpointEvery: 60,
  create() {
    const row = createTraining('magic', 7);
    return { state: row.state, training: row, player: row.player, dummy: row.dummy };
  },
  inputFor(_tick, ctx) {
    return { [ctx.player.id]: timingBot(ctx.training, 'perfect') };
  },
  expect(ctx, name) {
    if (ctx.training.attacks < 4) fail(name, `attacks ${ctx.training.attacks}`);
    if (ctx.player.metrics.perfects < 1 || ctx.player.metrics.hits < 1) {
      fail(name, `perfects ${ctx.player.metrics.perfects} hits ${ctx.player.metrics.hits}`);
    }
    if (ctx.player.down) fail(name, 'player down');
  },
});

add({
  name: 'bot-duel',
  note: 'Two mages cast Rain Needle until one is down.',
  seed: 4,
  driver: 'arena',
  ticks: ticks(30),
  tail: 3,
  checkpointEvery: 60,
  create() {
    const state = createArena(4);
    const player = mage(state, 0, 10, 10, 'Cassia');
    const foe = mage(state, 1, 16, 10, 'Foe');
    return { state, player, foe, aimPlayer: { ...foe.pos }, aimFoe: { ...player.pos } };
  },
  inputFor(tick, ctx) {
    return {
      [ctx.player.id]: frame(ctx.aimPlayer, { slot: 0, cast: true }),
      [ctx.foe.id]: frame(ctx.aimFoe, { slot: 0, cast: tick >= 8 }),
    };
  },
  until(ctx) {
    return ctx.player.down || ctx.foe.down;
  },
  expect(ctx, name) {
    if (!ctx.player.down && !ctx.foe.down) fail(name, 'duel did not finish');
    if (!ctx.state.events.some((event) => event.kind === 'down')) fail(name, 'no down event');
    if (ctx.player.down && ctx.foe.down) fail(name, 'both mages down');
    if (!ctx.foe.down || ctx.player.down) fail(name, 'expected the later caster to fall');
  },
});

add({
  name: 'reset-waves',
  note: 'A pushed tier clock unlocks on the minimum gap, then resetWave restores the between-wave rules.',
  seed: 3,
  driver: 'arena',
  ticks: ticks(18),
  checkpointEvery: 360,
  create() {
    const state = createArena(3);
    const player = mage(state, 0, 10, 10, 'Cassia');
    player.clockAdvanceTicks = ticks(100);
    const end = ticks(18);
    this.ops = [
      { afterTick: end, op: 'setHp', actorId: player.id, hp: 45 },
      { afterTick: end, op: 'resetWave', actorId: player.id },
    ];
    return { state, player, end };
  },
  inputFor() {
    return {};
  },
  expect(ctx, name) {
    const pre = ctx.checkpoints.find((row) => row.tick === ctx.end && row.when === 'pre-ops');
    const unlocks = pre.actors[0].unlocks;
    const want = [0, ticks(6), ticks(12), ticks(18)];
    if (JSON.stringify(unlocks) !== JSON.stringify(want)) fail(name, `unlocks ${unlocks.join(',')}`);
    if (ctx.player.tier !== 1 || ctx.player.clockAdvanceTicks !== 0) fail(name, 'clock did not reset');
    if (ctx.player.hp !== 60) fail(name, `hp ${ctx.player.hp}`);
    if (ctx.player.water.flow !== 0 || ctx.player.mana !== ctx.player.maxMana) fail(name, 'resources did not refill');
  },
});

add({
  name: 'soldier-wave',
  note: 'One Hastatus conscript fights a tier-IV Leviathan caster until the conscript is down.',
  seed: 9,
  driver: 'enemies',
  ticks: ticks(20),
  tail: 5,
  checkpointEvery: 60,
  create() {
    const state = createArena(9);
    const player = dress(mage(state, 0, 12, 10, 'Cassia'), 'tide_orb', 4, 'A');
    const foe = addEnemy(state, 'conscript', { x: 18, y: 10 });
    return { state, player, foe, aim: { x: 18, y: 10 } };
  },
  inputFor(_tick, ctx) {
    return { [ctx.player.id]: frame(ctx.aim, { slot: 1, cast: true }) };
  },
  until(ctx) {
    return ctx.foe.down;
  },
  expect(ctx, name) {
    if (!ctx.foe.down) fail(name, `conscript hp ${ctx.foe.hp} player hp ${ctx.player.hp}`);
    if (ctx.player.down) fail(name, 'player down');
    if (!ctx.state.events.some((event) => event.kind === 'down' && event.actorId === ctx.foe.id)) fail(name, 'no down event');
  },
});

function referenceComposition() {
  const preset = presets.find((candidate) => candidate.name === runtime.games.referencePreset);
  if (!preset) fail('reference', runtime.games.referencePreset);
  return preset;
}

function addDuel(name, note, seed, startWave) {
  add({
    name,
    note,
    seed,
    driver: 'games',
    startWave,
    referencePlayer: true,
    preset: runtime.games.referencePreset,
    ticks: ticks(runtime.games.fightTimeoutS),
    checkpointEvery: 300,
    create() {
      const games = createGames(seed, referenceComposition(), startWave, true);
      return { games, state: games.state };
    },
    inputFor() { return {}; },
    until(ctx) { return ctx.games.phase !== 'active'; },
    expect(ctx, scenario) {
      if (ctx.games.phase === 'active') fail(scenario, 'timed out');
      if (ctx.state.randomLog.length === 0) fail(scenario, 'no rng');
    },
  });
}

addDuel('duel-competence-1', 'Reference Rotation versus the competence-1 Tiro semifinal, to a result.', 40000, 2);
addDuel('duel-competence-1-5', 'Reference Rotation versus the competence-1.5 Tiro final, to a result.', 40001, 3);

add({
  name: 'tiro-sequence',
  note: 'Reference player clears every Tiro wave, advancing as soon as a bout ends.',
  seed: 40015,
  driver: 'games',
  startWave: 0,
  referencePlayer: true,
  preset: runtime.games.referencePreset,
  autoAdvance: true,
  ticks: ticks(runtime.games.fightTimeoutS) * 4,
  checkpointEvery: 300,
  create() {
    const games = createGames(40015, referenceComposition(), 0, true);
    return { games, state: games.state };
  },
  inputFor() { return {}; },
  until(ctx) { return ctx.games.phase === 'complete' || ctx.games.phase === 'lost'; },
  expect(ctx, name) {
    if (ctx.games.phase !== 'complete' || ctx.games.wavesCleared !== 4) fail(name, `phase ${ctx.games.phase} waves ${ctx.games.wavesCleared}`);
    if (!ctx.games.result || ctx.games.result.kind !== 'champion') fail(name, 'not champion');
    if (ctx.state.randomLog.length === 0) fail(name, 'no rng');
  },
});

add({
  name: 'creature-bout',
  note: 'Reference player fights the cinder hounds and the mire maw. A death burst is checkpointed.',
  seed: 40003,
  driver: 'games',
  startWave: 1,
  referencePlayer: true,
  preset: runtime.games.referencePreset,
  ticks: ticks(runtime.games.fightTimeoutS),
  checkpointEvery: 300,
  create() {
    const games = createGames(40003, referenceComposition(), 1, true);
    return { games, state: games.state };
  },
  inputFor() { return {}; },
  sample(ctx) {
    if (this.sawBurst) return false;
    if (ctx.state.telegraphs.some((telegraph) => telegraph.survivesOwner)) {
      this.sawBurst = true;
      return true;
    }
    return false;
  },
  until(ctx) { return ctx.games.phase !== 'active'; },
  expect(ctx, name) {
    if (ctx.games.phase === 'active') fail(name, 'timed out');
    if (ctx.state.randomLog.length === 0) fail(name, 'no rng');
    const burst = ctx.checkpoints.some((row) => row.telegraphs.some((telegraph) => telegraph.survives));
    if (!burst) fail(name, 'no death burst');
  },
});

add({
  name: 'soldiers-bout',
  note: 'Reference player fights the opening soldier wave through to a result.',
  seed: 40004,
  driver: 'games',
  startWave: 0,
  referencePlayer: true,
  preset: runtime.games.referencePreset,
  ticks: ticks(runtime.games.fightTimeoutS),
  checkpointEvery: 300,
  create() {
    const games = createGames(40004, referenceComposition(), 0, true);
    return { games, state: games.state };
  },
  inputFor() { return {}; },
  until(ctx) { return ctx.games.phase !== 'active'; },
  expect(ctx, name) {
    if (ctx.games.phase === 'active') fail(name, 'timed out');
    if (ctx.state.randomLog.length === 0) fail(name, 'no rng');
  },
});

add({
  name: 'wave-reset',
  note: 'A wounded player clears the soldier wave. Advance heals once and spawns creatures.',
  seed: 3,
  driver: 'games',
  startWave: 0,
  referencePlayer: false,
  preset: presets[0].name,
  playerHp: 45,
  playerMana: 2,
  ticks: 2,
  checkpointEvery: 1,
  create() {
    const games = createGames(3, presets[0], 0, false);
    games.player.hp = 45;
    games.player.mana = 2;
    return { games, state: games.state };
  },
  inputFor() { return {}; },
  ops: [
    { afterTick: 1, op: 'clearWave' },
    { afterTick: 2, op: 'advanceGames' },
  ],
  expect(ctx, name) {
    if (ctx.state.randomLog.length === 0) fail(name, 'no rng');
    if (ctx.games.wave !== 1 || ctx.games.phase !== 'active') fail(name, `phase ${ctx.games.phase} wave ${ctx.games.wave}`);
    const hounds = ctx.state.actors.filter((actor) => actor.enemy?.id === 'cinder_hound').length;
    const maws = ctx.state.actors.filter((actor) => actor.enemy?.id === 'mire_maw').length;
    if (hounds !== 3 || maws !== 1) fail(name, `hounds ${hounds} maws ${maws}`);
    const before = ctx.checkpoints.find((row) => row.tick === 2 && row.when === 'pre-ops');
    const prior = before.actors.find((actor) => actor.id === ctx.games.player.id);
    const player = ctx.games.player;
    const healed = prior.hp + (player.maxHp - prior.hp) * combat.betweenWaves.healFractionOfMissingHp;
    if (player.hp !== healed) fail(name, `hp ${player.hp} wanted ${healed}`);
    if (player.mana !== player.maxMana || player.tier !== 1) fail(name, `mana ${player.mana} tier ${player.tier}`);
  },
});

add({
  name: 'tiro-ladder',
  note: 'Scripted clears of all four Tiro waves. One champion payout, not a sum.',
  seed: 11,
  driver: 'games',
  startWave: 0,
  referencePlayer: false,
  preset: presets[0].name,
  ticks: 8,
  checkpointEvery: 1,
  create() {
    const games = createGames(11, presets[0], 0, false);
    return { games, state: games.state };
  },
  inputFor() { return {}; },
  ops: [
    { afterTick: 1, op: 'clearWave' },
    { afterTick: 2, op: 'advanceGames' },
    { afterTick: 3, op: 'clearWave' },
    { afterTick: 4, op: 'advanceGames' },
    { afterTick: 5, op: 'clearWave' },
    { afterTick: 6, op: 'advanceGames' },
    { afterTick: 7, op: 'clearWave' },
  ],
  expect(ctx, name) {
    if (ctx.state.randomLog.length === 0) fail(name, 'no rng');
    if (ctx.games.phase !== 'complete' || ctx.games.wavesCleared !== 4) fail(name, `phase ${ctx.games.phase} waves ${ctx.games.wavesCleared}`);
    const result = ctx.games.result;
    const tier = gamesMod.tiro;
    if (!result || result.kind !== 'champion' || result.gold !== tier.payoutGold[3] || result.renown !== tier.renown[3]) {
      fail(name, `result ${JSON.stringify(result)}`);
    }
    if (!result.finalReached || !result.finalWon) fail(name, 'final flags');
  },
});

add({
  name: 'mage-react',
  note: 'A competence-1 mage draws aim RNG while the other mage casts a bolt.',
  seed: 100,
  driver: 'mage',
  ticks: 2,
  mageAttachments: [{ actorId: 1, competence: 1 }],
  create() {
    const state = createArena(100);
    const mage = addMage(state, 0, { x: 10, y: 10 }, 'Cassia');
    const foe = addMage(state, 1, { x: 20, y: 10 }, 'Foe');
    attachMageAI(mage, 1, state.tick);
    return { state, mage, foe };
  },
  inputFor(tick, ctx) {
    return { [ctx.foe.id]: frame(ctx.mage.pos, { slot: 0, cast: tick === 1 }) };
  },
  expect(ctx, name) {
    if (ctx.state.randomLog.length === 0) fail(name, 'no rng');
    if (!ctx.state.randomLog.some((draw) => draw.purpose === 'mage 1 aim')) fail(name, 'no aim draw');
  },
});

mkdirSync(outDir, { recursive: true });
for (const name of readdirSync(outDir)) {
  if (name.endsWith('.json')) rmSync(join(outDir, name));
}

const written = [];
for (const spec of scenarios) {
  const vector = runScenario(spec);
  const file = join(outDir, `${vector.name}.json`);
  writeFileSync(file, `${JSON.stringify(vector, null, 2)}\n`);
  written.push(`${vector.name}  ticks=${vector.ticks}  events=${vector.events.length}  checkpoints=${vector.checkpoints.length}`);
  if (vector.name === 'bolt-hit') {
    const sampleDir = join(cacheRoot, '..', '..', '..', '..');
    writeFileSync(join(cacheRoot, 'sample-state.json'), JSON.stringify(spec.create && null));
  }
}
console.log(`${written.length} scenarios`);
for (const line of written) console.log(line);

if (written.length < 45) fail('scenarios', `only ${written.length}`);
