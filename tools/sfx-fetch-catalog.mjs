// Read-only catalog snapshot for the sound audit. Uses the game's TLS/SFML packet format.
import tls from 'node:tls';
import fs from 'node:fs';
import path from 'node:path';

const out = path.resolve('output/sfx-audit');
fs.mkdirSync(out, { recursive: true });
const packet = await new Promise((resolve, reject) => {
  const s = tls.connect({ host: 'game.gloomthorn.com', port: 55004, servername: 'game.gloomthorn.com', rejectUnauthorized: true }, () => {
    s.write(Buffer.from([0, 0, 0, 1, 10])); // CardListRequest; no writes or account access.
  });
  let data = Buffer.alloc(0);
  s.setTimeout(15000, () => s.destroy(new Error('Catalog request timed out')));
  s.on('error', reject);
  s.on('data', b => {
    data = Buffer.concat([data, b]);
    if (data.length >= 4) {
      const n = data.readUInt32BE(0);
      if (n > 16 * 1024 * 1024) return s.destroy(new Error('Invalid packet size'));
      if (data.length >= n + 4) { resolve(data.subarray(4, n + 4)); s.end(); }
    }
  });
});
let offset = 0;
const u8 = () => packet.readUInt8(offset++);
const u32 = () => { const v = packet.readUInt32BE(offset); offset += 4; return v; };
const i32 = () => { const v = packet.readInt32BE(offset); offset += 4; return v; };
const str = () => { const n = u32(); if (n > packet.length - offset) throw Error('Invalid string'); const v = packet.toString('utf8', offset, offset + n); offset += n; return v; };
const list = fn => { const n = u32(); if (n > 4096) throw Error('Invalid list'); return Array.from({length:n}, fn); };
const response = u8(), success = u8(), message = str();
if (response !== 11 || !success) throw Error('Catalog rejected: ' + message);
if (u32() !== 0xffffffff) throw Error('Legacy catalog not expected');
const version = u32(), count = u32();
if (version < 6 || version > 12 || count > 4096) throw Error('Unsupported catalog schema');
const action = () => {
  const a = {name:str(), state:i32()};
  if (version >= 7) a.nextState = i32();
  Object.assign(a, {kind:str(), pattern:str(), minRange:i32(), maxRange:i32(), damage:i32(), heal:i32(), canMove:!!u8(), canAttack:!!u8(), passThrough:!!u8(), lineOfSight:!!u8(), statusTurns:i32(), cooldownTurns:i32(), push:i32()});
  if (version >= 8) a.control = i32();
  if (version >= 9) a.repeat = i32();
  a.targetFilter = list(str);
  if (version >= 10) a.infest = str();
  if (version >= 11) a.pull = !!u8();
  if (version >= 12) a.lifedrain = !!u8();
  return a;
};
const cards = Array.from({length:count}, () => {
  const c = {title:str(), type:str(), imagePath:str(), traits:list(str), keywords:list(str)};
  c.integers = Object.fromEntries(list(() => [str(), i32()]));
  c.strings = Object.fromEntries(list(() => [str(), str()]));
  c.lists = Object.fromEntries(list(() => [str(), list(str)]));
  c.actionNames = list(str); c.actionDisplayNames = list(str); c.actions = list(action);
  return c;
});
if (offset !== packet.length) throw Error('Unconsumed catalog data: ' + (packet.length-offset));
const snapshot = {source:'game.gloomthorn.com:55004 CardListRequest', fetchedAt:new Date().toISOString(), version, count, cards};
fs.writeFileSync(path.join(out, 'catalog.json'), JSON.stringify(snapshot, null, 2));
console.log(JSON.stringify({version, count, types:cards.reduce((r,c)=>(r[c.type]=(r[c.type]||0)+1,r),{}), actions:new Set(cards.flatMap(c=>c.actions.map(a=>a.name))).size}));
