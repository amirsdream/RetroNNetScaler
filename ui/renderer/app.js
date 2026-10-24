'use strict';
// Renderer: menus, library, details drawer, game view, save states, settings, input.

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => [...root.querySelectorAll(sel)];
const api = window.retro;

const NES_W = 256;
const NES_H = 240;
const SLOTS = 8;
const BIT = { a: 0, b: 1, select: 2, start: 3, up: 4, down: 5, left: 6, right: 7 };
const PAD_LABELS = [
  ['up', 'Up', '↑'], ['down', 'Down', '↓'], ['left', 'Left', '←'], ['right', 'Right', '→'],
  ['a', 'A', 'A'], ['b', 'B', 'B'], ['start', 'Start', 'ST'], ['select', 'Select', 'SE'],
];
const DEFAULT_KEYS = {
  up: 'KeyW', down: 'KeyS', left: 'KeyA', right: 'KeyD',
  a: 'KeyJ', b: 'KeyK', start: 'Enter', select: 'ShiftRight',
};
// Standard-mapping gamepad: right face = NES A, bottom/left face = NES B.
const PAD_BUTTONS = { a: [1], b: [0, 2], select: [8], start: [9], up: [12], down: [13], left: [14], right: [15] };

const state = {
  settings: null,
  info: null,
  roms: [],
  page: 'home',
  libMode: 'library',   // library | favorites | recent
  current: null,        // ROM being played
  paused: false,
  pauseView: null,      // null | save | load | settings
  slot: 1,
  ff: false,
  sessionSeconds: 0,
  gotFrame: false,
  drawerRom: null,
};
let latestFrame = null;
let framesThisSecond = 0;
let lastMask = -1;
const keysDown = new Set();
const replyWaiters = new Map();

// ---------- formatting ----------

function prettyName(file) {
  return file.replace(/\.nes$/i, '').replace(/\s*[[(][^\])]*[\])]/g, '').trim() || file;
}

function region(file) {
  const m = file.match(/\((U|JU|J|E|UE|USA|Europe|Japan|World)\)/);
  return m ? ({ U: 'USA', JU: 'Japan · USA', J: 'Japan', E: 'Europe', UE: 'USA · Europe' }[m[1]] || m[1]) : '';
}

function hashHue(s) {
  let h = 7;
  for (const c of s) h = (h * 31 + c.charCodeAt(0)) >>> 0;
  return h % 360;
}

function initials(name) {
  const words = name.split(/[\s\-.:]+/).filter((w) => /[a-z0-9]/i.test(w));
  return (words.length > 1 ? words[0][0] + words[1][0] : name.slice(0, 2)).toUpperCase();
}

function fmtDur(sec) {
  if (!sec) return 'Not played';
  if (sec < 60) return '< 1m';
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  return h ? `${h}h ${m}m` : `${m}m`;
}

function fmtAgo(ts) {
  if (!ts) return 'Never';
  const s = (Date.now() - ts) / 1000;
  if (s < 60) return 'Just now';
  if (s < 3600) return `${Math.floor(s / 60)}m ago`;
  if (s < 86400) return `${Math.floor(s / 3600)}h ago`;
  if (s < 86400 * 7) return `${Math.floor(s / 86400)}d ago`;
  return new Date(ts).toLocaleDateString();
}

const fmtSize = (b) => (b >= 1024 * 1024 ? `${(b / 1048576).toFixed(1)} MB` : `${Math.round(b / 1024)} KB`);

const ARROWS = { ArrowUp: '↑', ArrowDown: '↓', ArrowLeft: '←', ArrowRight: '→' };
const keyLabel = (code) => ARROWS[code] || code.replace(/^(Key|Digit)/, '').replace(/^(Shift|Control|Alt)(Left|Right)$/, '$2 $1');

function el(tag, cls, html) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (html != null) e.innerHTML = html;
  return e;
}

function esc(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

// ---------- toasts & modal ----------

function toast(msg, { icon = 'info', img = null, sub = null, ms = 2200 } = {}) {
  const t = el('div', 'toast');
  t.innerHTML = (img ? `<img src="${esc(img)}" alt="">` : `<span class="t-ico">${iconSvg(icon, 16)}</span>`)
    + `<div>${esc(msg)}${sub ? `<small>${esc(sub)}</small>` : ''}</div>`;
  const box = $('#toasts');
  box.append(t);
  while (box.children.length > 3) box.firstChild.remove();
  setTimeout(() => { t.classList.add('out'); setTimeout(() => t.remove(), 200); }, ms);
}

function alertModal(title, body) {
  $('#modal-title').textContent = title;
  $('#modal-body').textContent = body;
  $('#modal').hidden = false;
  $('#modal-ok').focus();
}
$('#modal-ok').addEventListener('click', () => { $('#modal').hidden = true; });

// ---------- settings ----------

const persist = () => api.setSettings(state.settings);

function setSetting(key, value) {
  state.settings[key] = value;
  persist();
  syncSettingControls();
  applySettings();
  if (key === 'boxArt') { artCache.clear(); renderAll(); }
}

function syncSettingControls() {
  const s = state.settings;
  for (const seg of $$('.seg[data-set]')) {
    for (const b of $$('button[data-v]', seg)) b.classList.toggle('on', String(s[seg.dataset.set]) === b.dataset.v);
  }
  for (const cb of $$('input[type=checkbox][data-set]')) cb.checked = !!s[cb.dataset.set];
  for (const b of $$('#view-toggle button')) b.classList.toggle('on', b.dataset.view === s.libraryView);
  $('#sort').value = s.librarySort;
  $('#card-size').value = s.cardSize;
}

function applySettings() {
  const s = state.settings;
  document.body.classList.toggle('side-collapsed', !!s.sidebarCollapsed);
  $('#lib-grid').style.setProperty('--card', `${s.cardSize}px`);
  layoutScreen();
}

for (const seg of $$('.seg[data-set]')) {
  seg.addEventListener('click', (e) => {
    const b = e.target.closest('button[data-v]');
    if (!b) return;
    setSetting(seg.dataset.set, 'num' in seg.dataset ? Number(b.dataset.v) : b.dataset.v);
  });
}
for (const cb of $$('input[type=checkbox][data-set]')) {
  cb.addEventListener('change', () => setSetting(cb.dataset.set, cb.checked));
}

// ---------- box art ----------

const artCache = new Map();

function artFor(rom) {
  if (rom.cover) return Promise.resolve(rom.cover);
  if (!state.settings.boxArt) return Promise.resolve(null);
  if (!artCache.has(rom.id)) {
    // Don't remember failures: a later render retries (main skips known misses itself).
    const job = api.getArt(rom.id, rom.file).catch(() => null)
      .then((url) => { if (!url) artCache.delete(rom.id); return url; });
    artCache.set(rom.id, job);
  }
  return artCache.get(rom.id);
}

const artObserver = new IntersectionObserver((entries) => {
  for (const e of entries) {
    if (!e.isIntersecting) continue;
    artObserver.unobserve(e.target);
    e.target._loadArt();
  }
}, { rootMargin: '300px' });

function setImg(img, url) {
  if (!url) return;
  img.onload = () => img.classList.add('loaded');
  img.src = url;
}

// ---------- cards ----------

function fallbackArt(rom) {
  const name = prettyName(rom.file);
  const hue = hashHue(name);
  const fb = el('div', 'card-fallback');
  fb.style.background = `radial-gradient(120% 90% at 0% 0%, hsl(${hue} 70% 52%), transparent 60%),`
    + `linear-gradient(160deg, hsl(${hue} 55% 34%), hsl(${(hue + 50) % 360} 60% 16%))`;
  fb.innerHTML = `<span class="fb-sys">NES</span><div><div class="fb-mark">${esc(initials(name))}</div><div class="fb-name">${esc(name)}</div></div>`;
  return fb;
}

function makeCard(rom, { wide = false } = {}) {
  const name = prettyName(rom.file);
  const meta = rom.meta || {};
  const card = el('button', `card${wide ? ' wide' : ''}`);
  card.title = name;
  const art = el('div', 'card-art');
  art.append(fallbackArt(rom));
  const img = el('img');
  img.alt = '';
  art.append(img);
  art.insertAdjacentHTML('beforeend',
    `<div class="card-hover"><span class="card-play">${iconSvg('play', 22)}</span></div>`
    + `<span class="card-fav${meta.favorite ? ' on' : ''}" title="Favorite">${iconSvg('star', 15)}</span>`);
  card.append(art);

  const sub = wide ? `${fmtAgo(meta.lastPlayed)} · ${fmtDur(meta.playtime)}` : (region(rom.file) || 'NES');
  card.insertAdjacentHTML('beforeend',
    `<div class="card-meta"><div class="card-name">${esc(name)}</div><div class="card-sub">${esc(sub)}</div>`
    + `<span class="list-col">${esc(fmtDur(meta.playtime))}</span><span class="list-col">${esc(fmtAgo(meta.lastPlayed))}</span>`
    + `<span class="list-col">${esc(fmtSize(rom.size))}</span></div>`);

  card._loadArt = async () => {
    if (wide && rom.snap) {
      art.classList.add('snap');
      setImg(img, rom.snap);
      return;
    }
    setImg(img, await artFor(rom));
  };
  artObserver.observe(card);

  card.addEventListener('click', (e) => {
    if (e.target.closest('.card-fav')) {
      e.stopPropagation();
      toggleFavorite(rom);
      return;
    }
    if (e.target.closest('.card-play')) play(rom);
    else openDrawer(rom);
  });
  card.addEventListener('dblclick', () => play(rom));
  card.addEventListener('keydown', (e) => { if (e.key === 'Enter' && e.ctrlKey) play(rom); });
  card.addEventListener('contextmenu', (e) => { e.preventDefault(); openContextMenu(e.clientX, e.clientY, rom); });
  return card;
}

async function toggleFavorite(rom) {
  rom.meta = await api.updateGame(rom.id, { favorite: !(rom.meta && rom.meta.favorite) });
  toast(rom.meta.favorite ? 'Added to favorites' : 'Removed from favorites', { icon: 'star', sub: prettyName(rom.file) });
  renderAll();
  if (state.drawerRom && state.drawerRom.id === rom.id) fillDrawer(rom);
}

// ---------- navigation ----------

function go(page) {
  if (state.current) {
    quitGame().then(() => go(page));
    return;
  }
  if (['library', 'favorites', 'recent'].includes(page)) {
    state.libMode = page;
    page = 'library';
  }
  state.page = page;
  for (const p of $$('.page')) p.classList.toggle('active', p.dataset.page === page);
  for (const n of $$('.nav-item[data-page]')) {
    const target = page === 'library' ? state.libMode : page;
    n.classList.toggle('active', n.dataset.page === target);
  }
  closeDrawer();
  renderPage();
  $('#content').scrollTop = 0;
}

$$('.nav-item[data-page]').forEach((n) => n.addEventListener('click', () => go(n.dataset.page)));
$$('[data-goto]').forEach((b) => b.addEventListener('click', () => go(b.dataset.goto)));
$('#side-collapse').addEventListener('click', () => setSetting('sidebarCollapsed', !state.settings.sidebarCollapsed));

function renderPage() {
  if (state.page === 'home') renderHome();
  else if (state.page === 'library') renderLibrary();
  else if (state.page === 'settings') renderSettings();
}

function renderAll() {
  $('#nav-count').textContent = state.roms.length || '';
  renderPage();
}

// ---------- home ----------

function recentRoms() {
  return state.roms.filter((r) => r.meta.lastPlayed).sort((a, b) => b.meta.lastPlayed - a.meta.lastPlayed);
}

function renderHome() {
  const recent = recentRoms();
  const last = recent[0];
  const hero = $('#hero');
  hero.className = 'hero';
  hero.removeAttribute('style');
  hero.replaceChildren();
  if (last) {
    const name = prettyName(last.file);
    const bg = el('div', 'hero-bg');
    hero.append(bg);
    if (last.snap) {
      bg.style.backgroundImage = `url("${last.snap}")`;
      const shot = el('img', 'hero-shot');
      shot.src = last.snap;
      hero.append(shot);
    } else {
      artFor(last).then((u) => { if (u) bg.style.backgroundImage = `url("${u}")`; });
    }
    const c = el('div', 'hero-content');
    c.innerHTML = `<span class="hero-kicker">${iconSvg('clock', 14)} Continue playing</span>`
      + `<h1>${esc(name)}</h1>`
      + `<p>Last played ${esc(fmtAgo(last.meta.lastPlayed).toLowerCase())} · ${esc(fmtDur(last.meta.playtime))} played</p>`
      + `<div class="hero-actions"><button class="btn primary big" data-a="play">${iconSvg('play', 18)}<span>Continue</span></button>`
      + `<button class="btn glass big" data-a="info">${iconSvg('info', 18)}<span>Details</span></button></div>`;
    c.querySelector('[data-a=play]').addEventListener('click', () => play(last));
    c.querySelector('[data-a=info]').addEventListener('click', () => openDrawer(last));
    hero.append(c);
  } else {
    hero.classList.add('welcome');
    // Collage of library covers behind the welcome text.
    const collage = el('div', 'hero-collage');
    for (const r of state.roms.slice(0, 8)) {
      const img = el('img');
      img.alt = '';
      artFor(r).then((u) => setImg(img, u));
      collage.append(img);
    }
    hero.append(collage);
    const c = el('div', 'hero-content');
    c.innerHTML = `<span class="hero-kicker">${iconSvg('gamepad', 14)} Welcome</span>`
      + `<h1>Your retro library, sharp as ever.</h1>`
      + `<p>${state.roms.length ? `${state.roms.length} games ready. Pick one to start, and it will show up here next time.` : 'Add a folder of .nes ROMs to get started.'}</p>`
      + `<div class="hero-actions">${state.roms.length
        ? `<button class="btn primary big" data-a="lib">${iconSvg('grid', 18)}<span>Browse library</span></button>`
        : `<button class="btn primary big" data-a="add">${iconSvg('folder', 18)}<span>Add ROM folder</span></button>`}</div>`;
    const b = c.querySelector('[data-a]');
    b.addEventListener('click', () => (b.dataset.a === 'lib' ? go('library') : commands.addFolder()));
    hero.append(c);
  }

  const shelf = (id, list, opts) => {
    const box = $(`#${id}`);
    box.hidden = list.length === 0;
    const row = $('.row-scroll', box);
    row.classList.toggle('wide', !!(opts && opts.wide));
    row.replaceChildren(...list.map((r) => makeCard(r, opts)));
    row.scrollLeft = 0;
    updateShelf(box);
  };
  shelf('shelf-recent', recent.slice(1, 12), { wide: true });
  shelf('shelf-fav', state.roms.filter((r) => r.meta.favorite));
  shelf('shelf-all', [...state.roms].sort((a, b) => prettyName(a.file).localeCompare(prettyName(b.file))).slice(0, 20));
}

// Arrow buttons + edge fades for the horizontal home rows.
function updateShelf(box) {
  const row = $('.row-scroll', box);
  const left = row.scrollLeft > 8;
  const right = row.scrollLeft + row.clientWidth < row.scrollWidth - 8;
  row.classList.toggle('more-left', left);
  row.classList.toggle('more-right', right);
  const [prev, next] = $$('.shelf-arrow', box);
  prev.disabled = !left;
  next.disabled = !right;
  prev.hidden = next.hidden = !left && !right;
}

for (const box of $$('.shelf')) {
  const row = $('.row-scroll', box);
  row.addEventListener('scroll', () => updateShelf(box), { passive: true });
  for (const b of $$('.shelf-arrow', box)) {
    b.addEventListener('click', () => row.scrollBy({ left: Number(b.dataset.dir) * row.clientWidth * 0.8 }));
  }
}
window.addEventListener('resize', () => $$('.shelf').forEach(updateShelf));

// ---------- library ----------

function renderLibrary() {
  const s = state.settings;
  const mode = state.libMode;
  const q = $('#search').value.trim().toLowerCase();
  let list = state.roms;
  if (mode === 'favorites') list = list.filter((r) => r.meta.favorite);
  if (mode === 'recent') list = list.filter((r) => r.meta.lastPlayed);
  if (q) list = list.filter((r) => prettyName(r.file).toLowerCase().includes(q));

  const sort = mode === 'recent' ? 'recent' : s.librarySort;
  const byName = (a, b) => prettyName(a.file).localeCompare(prettyName(b.file));
  list = [...list].sort(
    sort === 'recent' ? (a, b) => (b.meta.lastPlayed || 0) - (a.meta.lastPlayed || 0) || byName(a, b)
      : sort === 'playtime' ? (a, b) => (b.meta.playtime || 0) - (a.meta.playtime || 0) || byName(a, b)
        : byName);

  $('#lib-title').textContent = { library: 'Library', favorites: 'Favorites', recent: 'Recently Played' }[mode];
  $('#lib-sub').textContent = `${list.length} ${list.length === 1 ? 'game' : 'games'}${q ? ` matching “${q}”` : ''}`;
  $('#sort').hidden = mode === 'recent';

  const grid = $('#lib-grid');
  grid.classList.toggle('list', s.libraryView === 'list');
  grid.replaceChildren(...list.map((r) => makeCard(r)));

  const empty = list.length === 0;
  $('#lib-empty').hidden = !empty;
  $('#lib-empty-msg').textContent = q ? 'No games match your search.'
    : mode === 'favorites' ? 'Star a game to pin it here.'
      : mode === 'recent' ? 'Games you play will show up here.'
        : 'Add a folder that contains .nes ROM files.';
  $('.empty-actions', $('#lib-empty')).hidden = mode !== 'library' || !!q;
}

$('#search').addEventListener('input', renderLibrary);
$('#sort').addEventListener('change', (e) => { state.settings.librarySort = e.target.value; persist(); renderLibrary(); });
$('#card-size').addEventListener('input', (e) => { state.settings.cardSize = Number(e.target.value); applySettings(); });
$('#card-size').addEventListener('change', persist);
$$('#view-toggle button').forEach((b) => b.addEventListener('click', () => { setSetting('libraryView', b.dataset.view); renderLibrary(); }));

async function refreshRoms() {
  state.roms = await api.listRoms();
  renderAll();
}

// ---------- drawer ----------

function openDrawer(rom) {
  state.drawerRom = rom;
  fillDrawer(rom);
  $('#drawer').classList.add('open');
  $('#drawer').setAttribute('aria-hidden', 'false');
  $('#drawer-scrim').hidden = false;
  setTimeout(() => $('#d-play').focus(), 50);
}

function closeDrawer() {
  state.drawerRom = null;
  $('#drawer').classList.remove('open');
  $('#drawer').setAttribute('aria-hidden', 'true');
  $('#drawer-scrim').hidden = true;
}

async function fillDrawer(rom) {
  const meta = rom.meta || {};
  $('#d-title').textContent = prettyName(rom.file);
  $('#d-sub').textContent = ['Nintendo Entertainment System', region(rom.file)].filter(Boolean).join(' · ');
  $('#d-time').textContent = fmtDur(meta.playtime);
  $('#d-last').textContent = fmtAgo(meta.lastPlayed);
  $('#d-size').textContent = fmtSize(rom.size);
  $('#d-fav').classList.toggle('on', !!meta.favorite);

  const img = $('#d-art');
  img.removeAttribute('src');
  img.className = '';
  const art = await artFor(rom);
  if (state.drawerRom !== rom) return;
  if (art) img.src = art;
  else if (rom.snap) { img.src = rom.snap; img.className = 'snap'; }

  const slots = await api.listSlots(rom.id);
  if (state.drawerRom !== rom) return;
  $('#d-play span').textContent = slots[0].time && state.settings.autoResume ? 'Continue' : 'Play';
  renderSlots($('#d-slots'), rom, slots, 'load', (slot) => { closeDrawer(); play(rom, { slot }); });
}

$('#drawer-close').addEventListener('click', closeDrawer);
$('#drawer-scrim').addEventListener('click', closeDrawer);
$('#d-play').addEventListener('click', () => { const r = state.drawerRom; closeDrawer(); play(r); });
$('#d-fav').addEventListener('click', () => toggleFavorite(state.drawerRom));
$('#d-folder').addEventListener('click', () => api.showInFolder(state.drawerRom.path));
$('#d-cover').addEventListener('click', async () => {
  const rom = state.drawerRom;
  const url = await api.setCustomArt(rom.id);
  if (!url) return;
  rom.cover = url;
  fillDrawer(rom);
  renderAll();
  toast('Cover updated', { icon: 'image' });
});

// ---------- save slots ----------

function renderSlots(box, rom, slots, mode, onPick) {
  box.replaceChildren();
  for (const s of slots) {
    if (s.slot === 0 && (mode === 'save' || !s.time)) continue;
    const b = el('button', `slot${s.slot === 0 ? ' auto' : ''}${mode === 'save' && s.slot === state.slot ? ' current' : ''}`);
    b.disabled = mode === 'load' && !s.time;
    b.innerHTML = `<div class="slot-shot">${s.thumb ? `<img src="${esc(s.thumb)}" alt="">` : iconSvg(mode === 'save' ? 'plus' : 'save', 22)}</div>`
      + `<div class="slot-info"><b>${s.slot === 0 ? 'Resume point' : `Slot ${s.slot}`}</b>`
      + `<span>${s.time ? new Date(s.time).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' }) : 'Empty'}</span></div>`;
    if (s.time) {
      const del = el('span', 'slot-del', iconSvg('trash', 14));
      del.title = 'Delete';
      del.addEventListener('click', async (e) => {
        e.stopPropagation();
        await api.deleteSlot(rom.id, s.slot);
        renderSlots(box, rom, await api.listSlots(rom.id), mode, onPick);
      });
      b.append(del);
    }
    b.addEventListener('click', () => onPick(s.slot));
    box.append(b);
  }
}

function waitReply(cmd, path, ms = 2000) {
  return new Promise((resolve) => {
    const key = `${cmd}|${path}`;
    const timer = setTimeout(() => { replyWaiters.delete(key); resolve(false); }, ms);
    replyWaiters.set(key, (ok) => { clearTimeout(timer); resolve(ok); });
  });
}

api.onCoreReply(({ ok, cmd, path }) => {
  const key = `${cmd}|${path}`;
  const fn = replyWaiters.get(key);
  if (fn) { replyWaiters.delete(key); fn(ok); }
});

async function saveSlot(slot, { quiet = false } = {}) {
  const rom = state.current;
  if (!rom || !state.gotFrame) return false;
  const path = await api.slotPath(rom.id, slot);
  const thumb = canvas.toDataURL('image/png');
  const done = waitReply('save', path);
  api.coreCmd(`save ${path}`);
  const ok = await done;
  if (ok) await api.writeSlotThumb(rom.id, slot, thumb);
  if (!quiet) {
    if (ok) toast(`Saved to slot ${slot}`, { img: thumb, sub: new Date().toLocaleTimeString() });
    else toast('Save failed', { icon: 'x' });
  }
  return ok;
}

async function loadSlot(slot, { quiet = false } = {}) {
  const rom = state.current;
  if (!rom) return false;
  const slots = await api.listSlots(rom.id);
  if (!slots[slot] || !slots[slot].time) {
    if (!quiet) toast(`Slot ${slot} is empty`, { icon: 'save' });
    return false;
  }
  const path = await api.slotPath(rom.id, slot);
  const done = waitReply('load', path);
  api.coreCmd(`load ${path}`);
  const ok = await done;
  if (!quiet || !ok) {
    toast(ok ? (slot === 0 ? 'Resumed where you left off' : `Loaded slot ${slot}`) : 'Could not load that save',
      { img: ok ? slots[slot].thumb : null, icon: ok ? 'upload' : 'x' });
  }
  return ok;
}

function changeSlot(delta) {
  state.slot = ((state.slot - 1 + delta + SLOTS) % SLOTS) + 1;
  toast(`Save slot ${state.slot}`, { icon: 'save', ms: 1200 });
}

// ---------- game ----------

const canvas = $('#screen');
const ctx = canvas.getContext('2d', { alpha: false });
const image = ctx.createImageData(NES_W, NES_H);

function layoutScreen() {
  if (!state.settings) return;
  const s = state.settings;
  const stage = $('#stage');
  const dpr = window.devicePixelRatio || 1;
  const par = s.aspect === 'par' ? 8 / 7 : 1;
  const availW = (stage.clientWidth || window.innerWidth) * dpr;
  const availH = (stage.clientHeight || window.innerHeight) * dpr;
  let scale = Math.min(availW / (NES_W * par), availH / NES_H);
  if (s.integerScale) scale = Math.max(1, Math.floor(scale));
  const w = (NES_W * par * scale) / dpr;
  const h = (NES_H * scale) / dpr;
  canvas.style.width = `${w}px`;
  canvas.style.height = `${h}px`;
  canvas.classList.toggle('smooth', s.filter === 'bilinear');
  const sl = $('#scanlines');
  sl.hidden = !s.scanlines;
  sl.style.width = `${w}px`;
  sl.style.height = `${h}px`;
  sl.style.background = `linear-gradient(to bottom, transparent 50%, rgba(0,0,0,.32) 50%) 0 0 / 100% ${h / NES_H}px`;
}

async function play(rom, { slot = null } = {}) {
  if (state.current) await quitGame();
  closeDrawer();
  closeMenus();
  const res = await api.startCore(rom.path, { mute: state.settings.mute });
  if (!res.ok) {
    alertModal('Could not start the emulator', res.error);
    return;
  }
  state.current = rom;
  state.paused = false;
  state.pauseView = null;
  state.sessionSeconds = 0;
  state.gotFrame = false;
  state.ff = false;
  lastMask = -1;
  latestFrame = null;
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, NES_W, NES_H);

  state.settings.recent = [rom.path, ...state.settings.recent.filter((p) => p !== rom.path)].slice(0, 12);
  persist();
  rom.meta = await api.updateGame(rom.id, { lastPlayed: Date.now() });

  $('#game').hidden = false;
  $('#pause').hidden = true;
  $('#osd-ff').hidden = true;
  document.body.classList.add('playing');
  document.body.classList.remove('paused');
  $('#tb-title').textContent = prettyName(rom.file);
  layoutScreen();

  if (slot == null && state.settings.autoResume) {
    const slots = await api.listSlots(rom.id);
    if (slots[0].time) slot = 0;
  }
  if (slot != null) await loadSlot(slot);
  else toast(prettyName(rom.file), { icon: 'gamepad', sub: 'Esc for menu · F5 save · F8 load' });
}

async function quitGame() {
  const rom = state.current;
  if (!rom) return;
  if (state.ff) setFastForward(false);
  if (state.gotFrame) {
    const shot = canvas.toDataURL('image/png');
    if (state.settings.autoResume) await saveSlot(0, { quiet: true });
    rom.snap = await api.writeSnap(rom.id, shot);
  }
  rom.meta = await api.updateGame(rom.id, {
    playtime: (rom.meta.playtime || 0) + state.sessionSeconds,
    lastPlayed: Date.now(),
  });
  api.stopCore();
  state.current = null;
  quitGameView();
  renderAll();
}

function quitGameView() {
  state.paused = false;
  $('#game').hidden = true;
  $('#pause').hidden = true;
  document.body.classList.remove('playing', 'paused', 'show-titlebar');
  $('#tb-title').textContent = '';
  api.setFullscreen(false);
}

function setPaused(p) {
  if (!state.current || state.paused === p) return;
  state.paused = p;
  api.coreCmd(p ? 'pause' : 'resume');
  if (p && state.ff) setFastForward(false);
  document.body.classList.toggle('paused', p);
  $('#pause').hidden = !p;
  if (p) {
    const rom = state.current;
    $('#p-title').textContent = prettyName(rom.file);
    $('#p-time').textContent = `${fmtDur((rom.meta.playtime || 0) + state.sessionSeconds)} played`;
    const img = $('#p-art');
    img.className = '';
    artFor(rom).then((u) => {
      if (u) img.src = u;
      else { img.src = canvas.toDataURL(); img.className = 'snap'; }
    });
    showPauseView(null);
    $('.pause-menu button[data-act=resume]').focus();
  }
}

async function showPauseView(view) {
  state.pauseView = view;
  for (const b of $$('.pause-menu button')) b.classList.toggle('active', b.dataset.act === (view || 'resume'));
  $('#pause-slots').hidden = view !== 'save' && view !== 'load';
  $('#pause-settings').hidden = view !== 'settings';
  if (view === 'save' || view === 'load') {
    const rom = state.current;
    $('#slots-title').textContent = view === 'save' ? 'Save State' : 'Load State';
    $('#slots-hint').textContent = view === 'save' ? 'Pick a slot. Existing saves are overwritten.' : 'Pick a save to jump back to.';
    renderSlots($('#p-slots'), rom, await api.listSlots(rom.id), view, async (slot) => {
      if (view === 'save') {
        state.slot = slot;
        await saveSlot(slot);
        showPauseView('save');
      } else if (await loadSlot(slot)) {
        setPaused(false);
      }
    });
  }
}

$('.pause-menu').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-act]');
  if (!b) return;
  const act = b.dataset.act;
  if (act === 'resume') setPaused(false);
  else if (act === 'save' || act === 'load' || act === 'settings') showPauseView(act);
  else if (act === 'reset') commands.reset();
  else if (act === 'screenshot') commands.screenshot();
  else if (act === 'fullscreen') api.toggleFullscreen();
  else if (act === 'quit') quitGame();
});

function setFastForward(on) {
  if (!state.current || state.paused || state.ff === on) return;
  state.ff = on;
  api.coreCmd(`speed ${on ? state.settings.ffSpeed : 1}`);
  $('#osd-ff').hidden = !on;
}

api.onFrame((buf) => {
  latestFrame = buf;
  framesThisSecond++;
  state.gotFrame = true;
});

api.onCoreExit(({ code, stderr }) => {
  if (!state.current) return;
  state.current = null;
  quitGameView();
  alertModal(`The emulator stopped (code ${code})`, stderr.trim() || 'No details were reported.');
  renderAll();
});

function draw() {
  if (latestFrame) {
    image.data.set(latestFrame);
    ctx.putImageData(image, 0, 0);
    latestFrame = null;
  }
  if (state.current) pollInput();
  requestAnimationFrame(draw);
}

setInterval(() => {
  const playing = state.current && !state.paused;
  if (playing) state.sessionSeconds++;
  $('#osd-fps').textContent = playing && state.settings.showFps ? `${framesThisSecond} FPS` : '';
  framesThisSecond = 0;
}, 1000);

// Fullscreen chrome + cursor auto-hide.
api.onFullscreen((on) => {
  document.body.classList.toggle('fullscreen', on);
  setTimeout(layoutScreen, 50);
});
let cursorTimer = 0;
window.addEventListener('mousemove', (e) => {
  document.body.classList.add('cursor');
  clearTimeout(cursorTimer);
  cursorTimer = setTimeout(() => document.body.classList.remove('cursor'), 2000);
  document.body.classList.toggle('show-titlebar', e.clientY < 48 || !!$('.menu'));
});
window.addEventListener('resize', () => layoutScreen());

// ---------- input ----------

let padMenuLatch = false;

function pollInput() {
  let mask = 0;
  if (!state.paused) {
    for (const [btn, code] of Object.entries(state.settings.keys)) {
      if (keysDown.has(code)) mask |= 1 << BIT[btn];
    }
  }
  for (const gp of navigator.getGamepads()) {
    if (!gp) continue;
    const pressed = (i) => gp.buttons[i] && gp.buttons[i].pressed;
    const menu = pressed(16) || (pressed(8) && pressed(9));
    if (menu && !padMenuLatch) setPaused(!state.paused);
    padMenuLatch = menu;
    if (state.paused || menu) continue;
    for (const [btn, idxs] of Object.entries(PAD_BUTTONS)) {
      if (idxs.some(pressed)) mask |= 1 << BIT[btn];
    }
    const [x = 0, y = 0] = gp.axes;
    if (x < -0.5) mask |= 1 << BIT.left;
    if (x > 0.5) mask |= 1 << BIT.right;
    if (y < -0.5) mask |= 1 << BIT.up;
    if (y > 0.5) mask |= 1 << BIT.down;
  }
  if (mask !== lastMask) {
    api.coreCmd(`pad ${mask}`);
    lastMask = mask;
  }
}

// ---------- commands & shortcuts ----------

const commands = {
  async openRom() {
    const p = await api.openRomFile();
    if (!p) return;
    play(state.roms.find((r) => r.path === p) || await api.describeRom(p));
  },
  async addFolder() {
    const s = await api.addRomFolder();
    if (!s) return;
    state.settings.romDirs = s.romDirs;
    await refreshRoms();
    toast('Folder added', { icon: 'folder', sub: `${state.roms.length} games in library` });
  },
  closeGame: () => quitGame(),
  quit: async () => { await quitGame(); api.quit(); },
  togglePause: () => setPaused(!state.paused),
  reset() {
    if (!state.current) return;
    api.coreCmd('reset');
    setPaused(false);
    toast('Reset', { icon: 'reset', ms: 1200 });
  },
  save: () => saveSlot(state.slot),
  load: async () => { if (await loadSlot(state.slot)) setPaused(false); },
  nextSlot: () => changeSlot(1),
  prevSlot: () => changeSlot(-1),
  async screenshot() {
    if (!state.current || !state.gotFrame) return;
    const shot = canvas.toDataURL('image/png');
    const out = await api.saveScreenshot(state.current.id, shot);
    toast('Screenshot saved', { img: shot, sub: out.split(/[\\/]/).slice(-2).join('/') });
  },
  fullscreen: () => api.toggleFullscreen(),
  toggleSidebar: () => setSetting('sidebarCollapsed', !state.settings.sidebarCollapsed),
  settings: (sec) => { if (state.current) { if (!state.paused) setPaused(true); showPauseView('settings'); return; } go('settings'); if (sec) showSettingsSection(sec); },
  openData: () => api.openPath(state.info.userData),
  about: () => { go('settings'); showSettingsSection('about'); },
};

// [code, ctrl, action, label]
const SHORTCUTS = [
  ['KeyO', true, () => commands.openRom(), 'Open ROM'],
  ['KeyQ', true, () => commands.quit(), 'Quit'],
  ['KeyR', true, () => commands.reset(), 'Reset game'],
  ['KeyB', true, () => commands.toggleSidebar(), 'Toggle sidebar'],
  ['Comma', true, () => commands.settings(), 'Settings'],
  ['Digit1', true, () => go('home'), 'Home'],
  ['Digit2', true, () => go('library'), 'Library'],
  ['Digit3', true, () => go('favorites'), 'Favorites'],
  ['Digit4', true, () => go('recent'), 'Recently played'],
  ['KeyF', true, () => { if (!state.current) { go(state.page === 'library' ? state.libMode : 'library'); $('#search').focus(); } }, 'Search'],
  ['F5', false, () => commands.save(), 'Save state'],
  ['F8', false, () => commands.load(), 'Load state'],
  ['F6', false, () => commands.prevSlot(), 'Previous slot'],
  ['F7', false, () => commands.nextSlot(), 'Next slot'],
  ['F12', false, () => commands.screenshot(), 'Screenshot'],
  ['F11', false, () => commands.fullscreen(), 'Fullscreen'],
];
const HOTKEY_HELP = [
  ['Esc', 'Pause menu'], ['Tab (hold)', 'Fast-forward'], ['F5 / F8', 'Save / load state'], ['F6 / F7', 'Change slot'],
  ['F11', 'Fullscreen'], ['F12', 'Screenshot'], ['Ctrl R', 'Reset'], ['Ctrl O', 'Open ROM'],
  ['Ctrl B', 'Toggle sidebar'], ['Ctrl F', 'Search'], ['Ctrl 1–4', 'Switch page'], ['Alt', 'Menu bar'],
];

let listeningFor = null;
let altAlone = false;

window.addEventListener('keydown', (e) => {
  altAlone = e.key === 'Alt';
  if (listeningFor) {
    e.preventDefault();
    if (e.code !== 'Escape') state.settings.keys[listeningFor] = e.code;
    listeningFor = null;
    persist();
    renderPadMap();
    return;
  }
  if (!$('#modal').hidden) {
    if (e.key === 'Escape' || e.key === 'Enter') { e.preventDefault(); $('#modal').hidden = true; }
    return;
  }
  if (openMenu && handleMenuKey(e)) return;

  const sc = SHORTCUTS.find(([code, ctrl]) => code === e.code && ctrl === (e.ctrlKey || e.metaKey));
  if (sc && !(e.target.matches('input[type=search]') && !e.ctrlKey)) {
    e.preventDefault();
    sc[2]();
    return;
  }
  if (e.code === 'Escape') {
    if (state.current) { if (state.paused && state.pauseView) showPauseView(null); else setPaused(!state.paused); }
    else if (state.drawerRom) closeDrawer();
    else if (document.activeElement === $('#search')) $('#search').blur();
    return;
  }
  if (state.current && !state.paused) {
    if (e.code === 'Tab') { e.preventDefault(); setFastForward(true); return; }
    keysDown.add(e.code);
    if (Object.values(state.settings.keys).includes(e.code)) e.preventDefault();
  }
});
window.addEventListener('keyup', (e) => {
  keysDown.delete(e.code);
  if (e.code === 'Tab') setFastForward(false);
  if (e.key === 'Alt' && altAlone) {
    e.preventDefault();
    if (openMenu) closeMenus(); else openMenuAt(0);
  }
  altAlone = false;
});
window.addEventListener('blur', () => { keysDown.clear(); setFastForward(false); });

// ---------- menu bar ----------

function menuDefs() {
  const s = state.settings;
  const g = !!state.current;
  const check = (on) => ({ checked: on });
  const recent = state.settings.recent
    .map((p) => state.roms.find((r) => r.path === p)).filter(Boolean).slice(0, 6)
    .map((r) => ({ label: prettyName(r.file), act: () => play(r) }));
  const pads = navigator.getGamepads().filter(Boolean);
  return [
    { label: 'File', items: [
      { label: 'Open ROM…', key: 'Ctrl+O', act: commands.openRom },
      { label: 'Add ROM Folder…', act: commands.addFolder },
      '-',
      { head: 'Recent' },
      ...(recent.length ? recent : [{ label: 'No recent games', disabled: true }]),
      '-',
      { label: 'Close Game', disabled: !g, act: commands.closeGame },
      { label: 'Exit', key: 'Ctrl+Q', act: commands.quit },
    ] },
    { label: 'Emulation', items: [
      { label: state.paused ? 'Resume' : 'Pause', key: 'Esc', disabled: !g, act: commands.togglePause },
      { label: 'Reset', key: 'Ctrl+R', disabled: !g, act: commands.reset },
      '-',
      { label: `Save State (slot ${state.slot})`, key: 'F5', disabled: !g, act: commands.save },
      { label: `Load State (slot ${state.slot})`, key: 'F8', disabled: !g, act: commands.load },
      { label: 'Previous Slot', key: 'F6', act: commands.prevSlot },
      { label: 'Next Slot', key: 'F7', act: commands.nextSlot },
      '-',
      { label: 'Fast-forward', key: 'Hold Tab', disabled: true },
      { label: 'Take Screenshot', key: 'F12', disabled: !g, act: commands.screenshot },
    ] },
    { label: 'Video', items: [
      { label: 'Sharp Pixels', ...check(s.filter === 'nearest'), act: () => setSetting('filter', 'nearest') },
      { label: 'Smooth', ...check(s.filter === 'bilinear'), act: () => setSetting('filter', 'bilinear') },
      '-',
      { label: '8:7 CRT Aspect', ...check(s.aspect === 'par'), act: () => setSetting('aspect', 'par') },
      { label: 'Square Pixels', ...check(s.aspect === 'square'), act: () => setSetting('aspect', 'square') },
      '-',
      { label: 'Integer Scaling', ...check(s.integerScale), act: () => setSetting('integerScale', !s.integerScale) },
      { label: 'Scanlines', ...check(s.scanlines), act: () => setSetting('scanlines', !s.scanlines) },
      { label: 'Show FPS', ...check(s.showFps), act: () => setSetting('showFps', !s.showFps) },
      '-',
      { label: 'Fullscreen', key: 'F11', act: commands.fullscreen },
    ] },
    { label: 'Input', items: [
      { label: 'Configure Controls…', act: () => commands.settings('input') },
      '-',
      { head: 'Gamepads' },
      ...(pads.length ? pads.map((p) => ({ label: p.id.replace(/\s*\(.*\)$/, ''), disabled: true })) : [{ label: 'None connected (press a button)', disabled: true }]),
    ] },
    { label: 'View', items: [
      { label: 'Home', key: 'Ctrl+1', act: () => go('home') },
      { label: 'Library', key: 'Ctrl+2', act: () => go('library') },
      { label: 'Favorites', key: 'Ctrl+3', act: () => go('favorites') },
      { label: 'Recently Played', key: 'Ctrl+4', act: () => go('recent') },
      '-',
      { label: 'Grid View', ...check(s.libraryView === 'grid'), act: () => { setSetting('libraryView', 'grid'); renderLibrary(); } },
      { label: 'List View', ...check(s.libraryView === 'list'), act: () => { setSetting('libraryView', 'list'); renderLibrary(); } },
      { label: 'Show Sidebar', key: 'Ctrl+B', ...check(!s.sidebarCollapsed), act: commands.toggleSidebar },
      '-',
      { label: 'Settings', key: 'Ctrl+,', act: () => commands.settings() },
    ] },
    { label: 'Help', items: [
      { label: 'Controls & Hotkeys', act: () => commands.settings('input') },
      { label: 'Open Data Folder', act: commands.openData },
      '-',
      { label: 'About RetroNNetScaler', act: commands.about },
    ] },
  ];
}

let openMenu = null; // { index, el, items, hot }

function buildMenubar() {
  const bar = $('#menubar');
  bar.replaceChildren(...menuDefs().map((m, i) => {
    const b = el('button', 'mb-item', esc(m.label));
    b.addEventListener('mousedown', (e) => { e.preventDefault(); if (openMenu && openMenu.index === i) closeMenus(); else openMenuAt(i); });
    b.addEventListener('mouseenter', () => { if (openMenu && openMenu.index !== i) openMenuAt(i); });
    return b;
  }));
}

function renderMenu(items, x, y) {
  const m = el('div', 'menu');
  m.setAttribute('role', 'menu');
  const actionable = [];
  for (const it of items) {
    if (it === '-') { m.append(el('hr')); continue; }
    if (it.head) { m.append(el('div', 'mi-head', esc(it.head))); continue; }
    const b = el('button');
    b.disabled = !!it.disabled;
    b.innerHTML = `<span class="mi-check">${it.checked ? iconSvg('check', 14) : ''}</span>`
      + `<span class="mi-label">${esc(it.label)}</span>${it.key ? `<kbd>${esc(it.key)}</kbd>` : ''}`;
    b.addEventListener('click', () => { closeMenus(); if (it.act) it.act(); });
    b.addEventListener('mouseenter', () => setHot(actionable.indexOf(b)));
    if (!it.disabled) actionable.push(b);
    m.append(b);
  }
  $('#menus').append(m);
  const r = m.getBoundingClientRect();
  m.style.left = `${Math.max(6, Math.min(x, window.innerWidth - r.width - 6))}px`;
  m.style.top = `${Math.max(6, Math.min(y, window.innerHeight - r.height - 6))}px`;
  return { el: m, items: actionable, hot: -1 };
}

function setHot(i) {
  if (!openMenu) return;
  openMenu.items.forEach((b, j) => b.classList.toggle('hot', j === i));
  openMenu.hot = i;
}

function openMenuAt(index) {
  closeMenus();
  const defs = menuDefs();
  const btn = $('#menubar').children[index];
  const r = btn.getBoundingClientRect();
  btn.classList.add('open');
  openMenu = { index, ...renderMenu(defs[index].items, r.left, r.bottom + 4) };
  document.body.classList.add('show-titlebar');
}

function openContextMenu(x, y, rom) {
  closeMenus();
  const fav = rom.meta && rom.meta.favorite;
  openMenu = { index: -1, ...renderMenu([
    { label: 'Play', act: () => play(rom) },
    { label: 'Details', act: () => openDrawer(rom) },
    '-',
    { label: fav ? 'Remove from Favorites' : 'Add to Favorites', act: () => toggleFavorite(rom) },
    { label: 'Set Custom Cover…', act: async () => { const u = await api.setCustomArt(rom.id); if (u) { rom.cover = u; renderAll(); } } },
    ...(rom.cover ? [{ label: 'Remove Custom Cover', act: async () => { await api.clearCustomArt(rom.id); rom.cover = null; renderAll(); } }] : []),
    '-',
    { label: 'Show in Folder', act: () => api.showInFolder(rom.path) },
  ], x, y) };
}

function closeMenus() {
  $('#menus').replaceChildren();
  $$('.mb-item.open').forEach((b) => b.classList.remove('open'));
  openMenu = null;
}

function handleMenuKey(e) {
  const n = openMenu.items.length;
  const moves = { ArrowDown: 1, ArrowUp: -1 };
  if (e.key in moves) {
    e.preventDefault();
    setHot(n ? (openMenu.hot + moves[e.key] + n) % n : -1);
  } else if ((e.key === 'ArrowLeft' || e.key === 'ArrowRight') && openMenu.index >= 0) {
    e.preventDefault();
    const count = $('#menubar').children.length;
    openMenuAt((openMenu.index + (e.key === 'ArrowRight' ? 1 : -1) + count) % count);
  } else if (e.key === 'Enter' || e.key === ' ') {
    e.preventDefault();
    if (openMenu.hot >= 0) openMenu.items[openMenu.hot].click();
  } else if (e.key === 'Escape') {
    e.preventDefault();
    closeMenus();
  } else {
    return false;
  }
  return true;
}

window.addEventListener('mousedown', (e) => {
  if (openMenu && !e.target.closest('.menu') && !e.target.closest('.mb-item')) closeMenus();
});
window.addEventListener('blur', closeMenus);

// ---------- settings page ----------

function showSettingsSection(sec) {
  $$('.settings-nav button').forEach((b) => b.classList.toggle('active', b.dataset.sec === sec));
  $$('.sec').forEach((s) => { s.hidden = s.dataset.sec !== sec; });
}
$$('.settings-nav button').forEach((b) => b.addEventListener('click', () => showSettingsSection(b.dataset.sec)));

function renderPadMap() {
  const box = $('#pad-map');
  box.replaceChildren();
  for (const [btn, label, glyph] of PAD_LABELS) {
    const row = el('div', 'bind');
    const isAB = btn === 'a' || btn === 'b';
    row.innerHTML = `<span class="bind-btn"><span class="bind-glyph${isAB ? ' ab' : ''}">${esc(glyph)}</span>${esc(label)}</span>`;
    const kbd = el('kbd', listeningFor === btn ? 'listening' : '');
    kbd.tabIndex = 0;
    kbd.textContent = listeningFor === btn ? 'Press a key…' : keyLabel(state.settings.keys[btn]);
    kbd.addEventListener('click', () => { listeningFor = btn; renderPadMap(); });
    kbd.addEventListener('keydown', (e) => { if (e.key === 'Enter' && listeningFor !== btn) { e.stopPropagation(); listeningFor = btn; renderPadMap(); } });
    row.append(kbd);
    box.append(row);
  }
}

function renderSettings() {
  syncSettingControls();
  renderPadMap();
  $('#hotkeys').innerHTML = HOTKEY_HELP.map(([k, d]) => `<div><span>${esc(d)}</span><kbd>${esc(k)}</kbd></div>`).join('');
  $('#dirs').replaceChildren(...state.settings.romDirs.map((d) => {
    const row = el('div', 'dir', `${iconSvg('folder', 16)}<span title="${esc(d)}">${esc(d)}</span>`);
    const rm = el('button', '', iconSvg('x', 16));
    rm.title = 'Remove folder';
    rm.addEventListener('click', async () => {
      state.settings.romDirs = (await api.removeRomFolder(d)).romDirs;
      await refreshRoms();
    });
    row.append(rm);
    return row;
  }));
  $('#about-ver').textContent = `Version ${state.info.version} · ${state.info.platform}`;
}

$('#keys-reset').addEventListener('click', () => { state.settings.keys = { ...DEFAULT_KEYS }; persist(); renderPadMap(); });
$('#recent-clear').addEventListener('click', async () => {
  state.settings.recent = [];
  persist();
  for (const r of state.roms) if (r.meta.lastPlayed) r.meta = await api.updateGame(r.id, { lastPlayed: 0 });
  renderAll();
  toast('Recently played cleared', { icon: 'trash' });
});
$('#open-data').addEventListener('click', commands.openData);
$$('[data-cmd]').forEach((b) => b.addEventListener('click', () => commands[b.dataset.cmd]()));

api.onClosing(async () => {
  await quitGame();
  api.quit();
});

// ---------- boot ----------

(async () => {
  hydrateIcons();
  state.info = await api.appInfo();
  state.settings = await api.getSettings();
  document.body.classList.toggle('mac', state.info.platform === 'darwin');
  buildMenubar();
  syncSettingControls();
  applySettings();
  await refreshRoms();
  go('home');
  requestAnimationFrame(draw);
})();
