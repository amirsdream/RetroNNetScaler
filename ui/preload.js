const { contextBridge, ipcRenderer } = require('electron');

const invoke = (ch) => (...args) => ipcRenderer.invoke(ch, ...args);
const send = (ch) => (...args) => ipcRenderer.send(ch, ...args);
const on = (ch) => (fn) => ipcRenderer.on(ch, (_e, data) => fn(data));

contextBridge.exposeInMainWorld('retro', {
  appInfo: invoke('app:info'),
  getSettings: invoke('settings:get'),
  setSettings: invoke('settings:set'),

  listRoms: invoke('roms:list'),
  describeRom: invoke('roms:describe'),
  addRomFolder: invoke('roms:addFolder'),
  removeRomFolder: invoke('roms:removeFolder'),
  openRomFile: invoke('roms:openFile'),
  updateGame: invoke('lib:update'),

  getArt: invoke('art:get'),
  setCustomArt: invoke('art:setCustom'),
  clearCustomArt: invoke('art:clearCustom'),
  writeSnap: invoke('snap:write'),

  listSlots: invoke('saves:list'),
  slotPath: invoke('saves:path'),
  writeSlotThumb: invoke('saves:thumb'),
  deleteSlot: invoke('saves:delete'),
  saveScreenshot: invoke('screenshot:save'),
  showInFolder: send('shell:show'),
  openPath: send('shell:open'),

  startCore: invoke('core:start'),
  coreCmd: send('core:cmd'),
  stopCore: send('core:stop'),
  onFrame: on('core:frame'),
  onCoreReply: on('core:reply'),
  onCoreExit: on('core:exit'),

  toggleFullscreen: send('win:toggleFullscreen'),
  setFullscreen: send('win:setFullscreen'),
  onFullscreen: on('win:fullscreen'),
  quit: send('app:quit'),
  onClosing: on('app:closing'),
});
