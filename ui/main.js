// Electron main process: window, settings/library storage, ROM scanning, box art,
// save-state files, and the headless core child process.
const { app, BrowserWindow, Menu, ipcMain, dialog, shell } = require('electron');
const { spawn } = require('child_process');
const { pathToFileURL } = require('url');
const fs = require('fs');
const path = require('path');

const REPO = path.resolve(__dirname, '..');
const FRAME_BYTES = 256 * 240 * 4;
const SLOTS = 8; // slot 0 is the automatic resume slot

const DEFAULT_SETTINGS = {
  romDirs: [path.join(REPO, 'roms')],
  recent: [],
  filter: 'nearest',     // nearest | bilinear
  integerScale: true,
  aspect: 'par',         // par (8:7 CRT) | square
  scanlines: false,
  showFps: true,
  mute: false,
  autoResume: true,
  boxArt: true,
  sidebarCollapsed: false,
  libraryView: 'grid',   // grid | list
  librarySort: 'name',   // name | recent | playtime
  cardSize: 180,
  ffSpeed: 4,
  keys: {
    up: 'KeyW', down: 'KeyS', left: 'KeyA', right: 'KeyD',
    a: 'KeyJ', b: 'KeyK', start: 'Enter', select: 'ShiftRight',
  },
};

let win = null;
let core = null;

// ---------- storage ----------

const userFile = (...p) => path.join(app.getPath('userData'), ...p);

function readJson(file, fallback) {
  try { return JSON.parse(fs.readFileSync(file, 'utf8')); } catch { return fallback; }
}

function writeJson(file, data) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, JSON.stringify(data, null, 2));
}

function loadSettings() {
  const saved = readJson(userFile('settings.json'), {});
  return { ...structuredClone(DEFAULT_SETTINGS), ...saved, keys: { ...DEFAULT_SETTINGS.keys, ...saved.keys } };
}

const saveSettings = (s) => writeJson(userFile('settings.json'), s);
const loadLibrary = () => readJson(userFile('library.json'), {});

// Stable per-game id from the file name (shared across folders on purpose).
const gameId = (file) => file.replace(/\.nes$/i, '').replace(/[^a-z0-9]+/gi, '_').toLowerCase();

const fileUrl = (p) => {
  try { return `${pathToFileURL(p).href}?v=${fs.statSync(p).mtimeMs | 0}`; } catch { return null; }
};

function writeDataUrl(file, dataUrl) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, Buffer.from(dataUrl.split(',')[1], 'base64'));
}

// ---------- ROMs ----------

function listRoms() {
  const lib = loadLibrary();
  const seen = new Set();
  const roms = [];
  for (const dir of loadSettings().romDirs) {
    let entries = [];
    try { entries = fs.readdirSync(dir, { withFileTypes: true }); } catch { continue; }
    for (const e of entries) {
      if (!e.isFile() || !e.name.toLowerCase().endsWith('.nes')) continue;
      const full = path.join(dir, e.name);
      if (seen.has(full)) continue;
      seen.add(full);
      roms.push(describeRom(full, lib));
    }
  }
  return roms;
}

function describeRom(full, lib = loadLibrary()) {
  const file = path.basename(full);
  const id = gameId(file);
  let size = 0;
  try { size = fs.statSync(full).size; } catch {}
  return {
    id,
    path: full,
    file,
    size,
    meta: lib[id] || {},
    snap: fileUrl(userFile('snaps', `${id}.png`)),
    cover: fileUrl(userFile('covers', `${id}.png`)),
  };
}

// ---------- box art (libretro-thumbnails) ----------

const THUMBS_REPO = 'libretro-thumbnails/Nintendo_-_Nintendo_Entertainment_System';
let artIndex = null; // Map normalizedTitle -> [file names]

const normTitle = (s) => s.toLowerCase()
  .replace(/\.(png|nes)$/, '')
  .replace(/\s*[[(][^\])]*[\])]/g, '')
  .replace(/^the\s+|,\s*the$/g, '')
  .replace(/&/g, 'and')
  .replace(/[^a-z0-9]/g, '');

// Name list ships with the app (data/boxart-index.json) so covers never depend on the
// rate-limited GitHub API; the API is only a fallback if that file is missing.
async function getArtIndex() {
  if (artIndex) return artIndex;
  const cacheFile = userFile('boxart', 'index.json');
  let names = readJson(path.join(__dirname, 'data', 'boxart-index.json'), null) || readJson(cacheFile, null);
  if (!names) {
    const gh = (u) => fetch(u, { headers: { 'User-Agent': 'RetroNNetScaler' } }).then((r) => {
      if (!r.ok) throw new Error(`GitHub ${r.status}`);
      return r.json();
    });
    const root = await gh(`https://api.github.com/repos/${THUMBS_REPO}/git/trees/master`);
    const dir = root.tree.find((t) => t.path === 'Named_Boxarts');
    const tree = await gh(`https://api.github.com/repos/${THUMBS_REPO}/git/trees/${dir.sha}`);
    names = tree.tree.map((t) => t.path).filter((p) => p.endsWith('.png'));
    writeJson(cacheFile, names);
  }
  artIndex = new Map();
  for (const n of names) {
    const k = normTitle(n);
    if (!artIndex.has(k)) artIndex.set(k, []);
    artIndex.get(k).push(n);
  }
  return artIndex;
}

function pickArt(candidates, file) {
  const prefer = /\((J|Japan)\)/.test(file) ? ['Japan', 'USA', 'World'] : ['USA', 'World', 'Europe', 'Japan'];
  const score = (n) => {
    const i = prefer.findIndex((r) => n.includes(r));
    return (i < 0 ? 9 : i) * 10 + (n.includes('Rev') ? 1 : 0) + (n.includes('Beta') || n.includes('Proto') ? 5 : 0);
  };
  return [...candidates].sort((a, b) => score(a) - score(b))[0];
}

const artInflight = new Map();

async function getArt(id, file) {
  const out = userFile('boxart', `${id}.png`);
  if (fs.existsSync(out)) return fileUrl(out);
  const misses = readJson(userFile('boxart', 'misses.json'), []);
  if (misses.includes(id)) return null;
  if (artInflight.has(id)) return artInflight.get(id);

  const job = (async () => {
    try {
      const index = await getArtIndex();
      const candidates = index.get(normTitle(file));
      if (!candidates) {
        writeJson(userFile('boxart', 'misses.json'), [...misses, id]);
        return null;
      }
      // Some entries are git symlinks: the raw file is just the target's name.
      let name = pickArt(candidates, file);
      let data = null;
      for (let hop = 0; hop < 3 && name; hop++) {
        const url = `https://raw.githubusercontent.com/${THUMBS_REPO}/master/Named_Boxarts/${encodeURIComponent(name)}`;
        const r = await fetch(url);
        if (!r.ok) return null;
        data = Buffer.from(await r.arrayBuffer());
        const isPng = data.length > 8 && data.readUInt32BE(0) === 0x89504e47;
        if (isPng) break;
        const target = data.toString('utf8').trim();
        name = data.length < 512 && target.endsWith('.png') ? path.posix.basename(target) : null;
        data = null;
      }
      if (!data) return null;
      fs.mkdirSync(path.dirname(out), { recursive: true });
      fs.writeFileSync(out, data);
      return fileUrl(out);
    } catch {
      return null; // offline or rate-limited: try again next launch
    } finally {
      artInflight.delete(id);
    }
  })();
  artInflight.set(id, job);
  return job;
}

// ---------- save slots ----------

const slotBase = (id, slot) => userFile('saves', id, `slot${slot}`);

function listSlots(id) {
  const slots = [];
  for (let slot = 0; slot <= SLOTS; slot++) {
    const state = `${slotBase(id, slot)}.state`;
    let time = null;
    try { time = fs.statSync(state).mtimeMs; } catch {}
    slots.push({ slot, time, thumb: time ? fileUrl(`${slotBase(id, slot)}.png`) : null });
  }
  return slots;
}

// ---------- core process ----------

function stopCore() {
  if (!core) return;
  const c = core;
  core = null;
  try { c.stdin.write('quit\n'); } catch {}
  setTimeout(() => { if (c.exitCode === null) c.kill(); }, 500);
}

function corePath() {
  if (process.env.RETRO_CORE) return process.env.RETRO_CORE;
  return path.join(REPO, 'build', process.platform === 'win32' ? '4kscaler-core.exe' : '4kscaler-core');
}

function startCore(romPath, { mute }) {
  stopCore();
  const exe = corePath();
  if (!fs.existsSync(exe)) {
    return { ok: false, error: `Core not found: ${exe}\nBuild it with: cmake --build build --target 4kscaler-core` };
  }
  const args = ['--rom', romPath];
  if (mute) args.push('--mute');
  const child = spawn(exe, args, { stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
  core = child;
  const send = (ch, data) => { if (win && core === child) win.webContents.send(ch, data); };

  // Reassemble the raw stream into whole frames.
  let frame = Buffer.allocUnsafe(FRAME_BYTES);
  let filled = 0;
  child.stdout.on('data', (chunk) => {
    let off = 0;
    while (off < chunk.length) {
      const n = Math.min(FRAME_BYTES - filled, chunk.length - off);
      chunk.copy(frame, filled, off, off + n);
      filled += n;
      off += n;
      if (filled === FRAME_BYTES) {
        send('core:frame', frame);
        frame = Buffer.allocUnsafe(FRAME_BYTES);
        filled = 0;
      }
    }
  });

  // stderr carries logs plus "@ok|@err <cmd> <path>" replies.
  let stderr = '';
  let pending = '';
  child.stderr.on('data', (d) => {
    stderr = (stderr + d).slice(-4000);
    pending += d;
    let nl;
    while ((nl = pending.indexOf('\n')) >= 0) {
      const line = pending.slice(0, nl).trim();
      pending = pending.slice(nl + 1);
      const m = line.match(/^@(ok|err) (\w+) (.*)$/);
      if (m) send('core:reply', { ok: m[1] === 'ok', cmd: m[2], path: m[3] });
    }
  });
  child.on('error', (err) => { stderr += String(err); });
  child.on('exit', (code) => {
    if (win && core === child && code) win.webContents.send('core:exit', { code, stderr });
    if (core === child) core = null;
  });
  return { ok: true };
}

// ---------- window ----------

function createWindow() {
  const isMac = process.platform === 'darwin';
  win = new BrowserWindow({
    width: 1360,
    height: 860,
    minWidth: 900,
    minHeight: 560,
    backgroundColor: '#0b0d12',
    title: 'RetroNNetScaler',
    titleBarStyle: 'hidden',
    ...(isMac ? { trafficLightPosition: { x: 14, y: 12 } }
      : { titleBarOverlay: { color: '#0b0d12', symbolColor: '#c9cfdb', height: 40 } }),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      sandbox: true,
    },
  });
  win.loadFile(path.join(__dirname, 'renderer', 'index.html'));
  win.on('enter-full-screen', () => win.webContents.send('win:fullscreen', true));
  win.on('leave-full-screen', () => win.webContents.send('win:fullscreen', false));
  // With a game running, let the renderer auto-save first; it calls app:quit when done.
  let closing = false;
  win.on('close', (e) => {
    if (!core || closing) return;
    e.preventDefault();
    closing = true;
    win.webContents.send('app:closing');
    setTimeout(() => app.quit(), 3000); // never hang on a stuck renderer
  });
  win.on('closed', () => { win = null; stopCore(); });
}

// ---------- IPC ----------

ipcMain.handle('app:info', () => ({ platform: process.platform, version: app.getVersion(), userData: app.getPath('userData') }));
ipcMain.handle('settings:get', () => loadSettings());
ipcMain.handle('settings:set', (_e, s) => { saveSettings(s); return true; });

ipcMain.handle('roms:list', () => listRoms());
ipcMain.handle('roms:describe', (_e, p) => describeRom(p));
ipcMain.handle('roms:addFolder', async () => {
  const r = await dialog.showOpenDialog(win, { properties: ['openDirectory'] });
  if (r.canceled || !r.filePaths[0]) return null;
  const s = loadSettings();
  if (!s.romDirs.includes(r.filePaths[0])) s.romDirs.push(r.filePaths[0]);
  saveSettings(s);
  return s;
});
ipcMain.handle('roms:removeFolder', (_e, dir) => {
  const s = loadSettings();
  s.romDirs = s.romDirs.filter((d) => d !== dir);
  saveSettings(s);
  return s;
});
ipcMain.handle('roms:openFile', async () => {
  const r = await dialog.showOpenDialog(win, { properties: ['openFile'], filters: [{ name: 'NES ROM', extensions: ['nes'] }] });
  return r.canceled ? null : r.filePaths[0];
});

ipcMain.handle('lib:update', (_e, id, patch) => {
  const lib = loadLibrary();
  lib[id] = { ...lib[id], ...patch };
  writeJson(userFile('library.json'), lib);
  return lib[id];
});

ipcMain.handle('art:get', (_e, id, file) => getArt(id, file));
ipcMain.handle('art:setCustom', async (_e, id) => {
  const r = await dialog.showOpenDialog(win, { properties: ['openFile'], filters: [{ name: 'Images', extensions: ['png', 'jpg', 'jpeg', 'webp'] }] });
  if (r.canceled || !r.filePaths[0]) return null;
  const out = userFile('covers', `${id}.png`);
  fs.mkdirSync(path.dirname(out), { recursive: true });
  fs.copyFileSync(r.filePaths[0], out);
  return fileUrl(out);
});
ipcMain.handle('art:clearCustom', (_e, id) => { fs.rmSync(userFile('covers', `${id}.png`), { force: true }); return true; });

ipcMain.handle('snap:write', (_e, id, dataUrl) => {
  const out = userFile('snaps', `${id}.png`);
  writeDataUrl(out, dataUrl);
  return fileUrl(out);
});

ipcMain.handle('saves:list', (_e, id) => listSlots(id));
ipcMain.handle('saves:path', (_e, id, slot) => {
  fs.mkdirSync(path.dirname(slotBase(id, slot)), { recursive: true });
  return `${slotBase(id, slot)}.state`;
});
ipcMain.handle('saves:thumb', (_e, id, slot, dataUrl) => { writeDataUrl(`${slotBase(id, slot)}.png`, dataUrl); return true; });
ipcMain.handle('saves:delete', (_e, id, slot) => {
  for (const ext of ['.state', '.png']) fs.rmSync(slotBase(id, slot) + ext, { force: true });
  return true;
});

ipcMain.handle('screenshot:save', (_e, name, dataUrl) => {
  const stamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
  const out = path.join(app.getPath('pictures'), 'RetroNNetScaler', `${name}-${stamp}.png`);
  writeDataUrl(out, dataUrl);
  return out;
});
ipcMain.on('shell:show', (_e, p) => shell.showItemInFolder(p));
ipcMain.on('shell:open', (_e, p) => shell.openPath(p));

ipcMain.handle('core:start', (_e, romPath, opts) => startCore(romPath, opts || {}));
ipcMain.on('core:cmd', (_e, line) => { if (core) core.stdin.write(line + '\n'); });
ipcMain.on('core:stop', () => stopCore());

ipcMain.on('win:toggleFullscreen', () => { if (win) win.setFullScreen(!win.isFullScreen()); });
ipcMain.on('win:setFullscreen', (_e, on) => { if (win) win.setFullScreen(!!on); });
ipcMain.on('app:quit', () => app.quit());

// The renderer draws its own menu bar; drop Electron's default one (and its Ctrl+R reload).
Menu.setApplicationMenu(null);
app.whenReady().then(createWindow);
app.on('window-all-closed', () => { stopCore(); app.quit(); });
