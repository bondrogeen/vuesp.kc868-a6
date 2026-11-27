import { readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { extname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { WebSocketServer } from 'ws';
import VuespStructModule from 'vuesp-struct';

const VuespStruct = VuespStructModule.default ?? VuespStructModule;

const DEFAULT_ROOT = fileURLToPath(new URL('../..', import.meta.url));

const parseFirmwareVersion = async (root) => {
  const text = await readOptional(join(root, '..', 'platformio.ini'), '');
  const match = /firmware_version\s*=\s*'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}'/.exec(text);
  return match ? [Number(match[1]), Number(match[2]), Number(match[3])] : [4, 0, 0];
};

const buildDeviceSample = (fields, now) => {
  const names = new Set(fields.map((field) => field.name));
  const isShinwa = names.has('cell1');
  const sample = {
    key: 0,
    command: 0,
    analog: 512,
    now,
    list: 2,
    message: 'text',
  };

  if (names.has('voltage')) sample.voltage = isShinwa ? 52800 : 5280;
  if (names.has('current')) sample.current = -1240;
  if (names.has('cycle')) sample.cycle = 128;
  if (names.has('rsoc')) sample.rsoc = 87;
  if (names.has('SOC')) sample.SOC = 87;
  if (names.has('capacity')) sample.capacity = 120;
  if (names.has('fccRaw')) sample.fccRaw = 120000;
  if (names.has('dccRaw')) sample.dccRaw = 104400;
  if (names.has('balanceCapacity')) sample.balanceCapacity = 110000;
  if (names.has('rateCapacity')) sample.rateCapacity = 120000;

  if (names.has('ntc')) sample.ntc = 24;
  if (names.has('ntc1')) sample.ntc1 = 2450;
  if (names.has('ntc2')) sample.ntc2 = 2520;
  if (names.has('ntc3')) sample.ntc3 = 2480;
  if (names.has('cellLow')) sample.cellLow = 3290;
  if (names.has('cellHigh')) sample.cellHigh = 3350;
  if (names.has('cellDiff')) sample.cellDiff = 60;
  if (names.has('cellAvg')) sample.cellAvg = 3320;

  if (names.has('dateDay')) sample.dateDay = 14;
  if (names.has('dateMonth')) sample.dateMonth = 7;
  if (names.has('dateYear')) sample.dateYear = 25;

  if (names.has('lastTime')) sample.lastTime = 0;
  if (names.has('status1')) sample.status1 = 0;
  if (names.has('status3')) sample.status3 = 6;
  if (names.has('cellCount')) sample.cellCount = 4;
  if (names.has('tempCount')) sample.tempCount = 4;
  if (names.has('series')) sample.series = 4;
  if (names.has('temp')) sample.temp = 24;
  if (names.has('fet')) sample.fet = 3;
  if (names.has('version')) sample.version = isShinwa ? 'V1.0' : 1;

  if (names.has('cellVoltage')) {
    sample.cellVoltage = Array.from({ length: 14 }, (_, index) => 3300 + ((index * 7) % 41) - 20);
  }
  for (let index = 1; index <= 4; index += 1) {
    if (names.has(`cell${index}`)) sample[`cell${index}`] = 3300 + ((index * 9) % 33) - 16;
    if (names.has(`temp${index}`)) sample[`temp${index}`] = 240 + index * 3;
  }

  return sample;
};

const COMMAND = {
  GPIO_GET: 1,
  GPIO_SET: 2,
  GPIO_GET_ALL: 3,
  GPIO_CHANGE: 4,
  GPIO_SAVE: 254,
};

// vuesp-struct packs fixed-length arrays, so mock payloads must be padded up to
// the size declared in protocol.h.
const padTo = (items, size, empty) => Array.from({ length: size }, (_, index) => items[index] ?? empty(index));

const modbusRole = Number(process.env.MOCK_MODBUS_ROLE ?? 2);

const modbusState = {
  role: modbusRole,
  address: 1,
  baud: 9600,
  parity: 0,
  stopBits: 1,
};

const modbusStatus = (now) => ({
  role: modbusState.role,
  command: 0,
  address: modbusState.address,
  baud: modbusState.baud,
  dataBits: 8,
  parity: modbusState.parity,
  stopBits: modbusState.stopBits,
  status: modbusState.role === 0 ? 0 : 1,
  lastFunction: 3,
  lastAddress: 0,
  lastError: 0,
  requests: 1284,
  errors: 3,
  varCount: 2,
  updated: now,
  point: 0,
  ok: 1,
  value: 23.4,
  text: '23.4',
  vars: padTo(
    [
      { var: 0, quality: 1, flags: 0, value: 23.4, text: '23.4' },
      { var: 1, quality: 1, flags: 0, value: 51.2, text: '51.2' },
    ],
    32,
    (index) => ({ var: index, quality: 0, flags: 0, value: 0, text: '' })
  ),
});

const modbusProfile = {
  command: 0,
  version: 1,
  deviceCount: 2,
  pointCount: 6,
  varCount: 2,
  reserved: 0,
  devices: padTo(
    [
      { id: 'meter', name: 'Energy meter', address: 1, enabled: 1, interval: 1000, timeout: 300, retries: 2 },
      { id: 'th', name: 'Temp sensor', address: 2, enabled: 1, interval: 5000, timeout: 300, retries: 1 },
    ],
    8,
    () => ({ id: '', name: '', address: 0, enabled: 0, interval: 0, timeout: 0, retries: 0 })
  ),
  points: padTo(
    [
      { device: 0, id: 'volt', name: 'Voltage', fc: 3, address: 0, type: 1, flags: 4, registers: 1, decimals: 1, scale: 1, offset: 0, unit: 'V', var: 0 },
      { device: 0, id: 'power', name: 'Power', fc: 3, address: 1, type: 1, flags: 4, registers: 1, decimals: 0, scale: 1, offset: 0, unit: 'W', var: 1 },
      { device: 1, id: 'temp', name: 'Temperature', fc: 4, address: 0, type: 1, flags: 4, registers: 1, decimals: 1, scale: 0.1, offset: 0, unit: '°C', var: 255 },
      { device: 1, id: 'hum', name: 'Humidity', fc: 4, address: 1, type: 1, flags: 4, registers: 1, decimals: 0, scale: 1, offset: 0, unit: '%', var: 255 },
      { device: 255, id: 'relay', name: 'Relay mask', fc: 3, address: 100, type: 2, flags: 0, registers: 1, decimals: 0, scale: 1, offset: 0, unit: '', var: 0 },
      { device: 255, id: 'ain', name: 'Analog input', fc: 3, address: 102, type: 1, flags: 4, registers: 1, decimals: 0, scale: 1, offset: 0, unit: '', var: 1 },
    ],
    64,
    () => ({ device: 0, id: '', name: '', fc: 0, address: 0, type: 0, flags: 0, registers: 0, decimals: 0, scale: 0, offset: 0, unit: '', var: 0 })
  ),
  vars: padTo(
    [
      { id: 'volt', name: 'Voltage', type: 1, flags: 0, unit: 'V', decimals: 1, point: 0, source: '' },
      { id: 'power', name: 'Power', type: 1, flags: 0, unit: 'W', decimals: 0, point: 1, source: '' },
    ],
    32,
    () => ({ id: '', name: '', type: 0, flags: 0, unit: '', decimals: 0, point: 0, source: '' })
  ),
};

const modbusRaw = {
  command: 0,
  slave: 1,
  function: 3,
  address: 0,
  length: 10,
  autoCrc: 1,
  request: padTo([1, 3, 0, 0, 0, 10], 64, () => 0),
  requestLen: 6,
  response: padTo([1, 3, 20, 0, 234, 0, 12, 0, 45, 0, 78], 64, () => 0),
  responseLen: 11,
  status: 0,
  reserved: 0,
  elapsed: 23,
};

const LIST_ALL = 0xff;

const readOptional = async (path, fallback) => {
  try {
    return await readFile(path, 'utf8');
  } catch {
    return fallback;
  }
};

const buildPort = (gpio, overrides = {}) => ({
  gpio,
  value: 0,
  mode: 1,
  interrupt: 0,
  list: LIST_ALL,
  disabled: 0,
  isButton: 0,
  command: 0,
  isPressed: 0,
  count: 0,
  empty: 0,
  valueOld: 0,
  pressStart: 0,
  ...overrides,
});

export const startMockDevice = async ({ port = 4173, silent = false, root = DEFAULT_ROOT } = {}) => {
  const defaultJsonText = await readFile(join(root, 'public/default.json'), 'utf8');
  const defaultJson = JSON.parse(defaultJsonText);
  const dashboardJson = await readOptional(join(root, 'json/dashboard.json'), '[]');
  const scriptsTxt = await readOptional(join(root, 'public/scripts.txt'), '');
  const firmwareVersion = await parseFirmwareVersion(root);
  const deviceStruct = defaultJson?.struct?.structs?.Device;
  const deviceFields = Array.isArray(deviceStruct) ? deviceStruct : (deviceStruct?.fields ?? []);

  const struct = new VuespStruct();
  struct.init(defaultJson.struct);
  const keyNames = Object.fromEntries(
    Object.entries(defaultJson?.struct?.enums?.KEY ?? {}).map(([name, id]) => [id, name])
  );

  const now = Math.floor(Date.now() / 1000);
  const deviceState = {
    info: {
      firmware: firmwareVersion,
      totalBytes: 1024 * 1024,
      usedBytes: 214 * 1024,
      id: 0xda129b,
      uptime: 4 * 3600 + 45 * 60 + 19,
      name: 'VUESP-DA129B',
      board: 1,
    },
    settings: {
      wifiDhcp: 1,
      wifiMode: 1,
      authMode: 0,
      version: 1,
      device: 0,
      discovery: 0,
      discoveryInterval: 5,
      discoveryPort: 0,
      wifiIp: [192, 168, 1, 100],
      wifiSubnet: [255, 255, 255, 0],
      wifiGateway: [192, 168, 1, 1],
      wifiDns: [192, 168, 1, 1],
      wifiSsid: 'Home',
      wifiPass: '',
      authLogin: 'admin',
      authPass: '',
    },
    device: buildDeviceSample(deviceFields, now),
    buffer: {
      empty: 0,
      head: 0,
      tail: 0,
      count: 128,
      data: Array.from({ length: 256 }, (_, index) => Math.round(24 + 4 * Math.sin(index / 9))),
    },
    dallas: [
      { comm: 0, comm2: 0, address: [0x28, 0xaa, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66], temp: 23.4 },
      { comm: 1, comm2: 0, address: [0x28, 0xbb, 0x11, 0x22, 0x33, 0x44, 0x55, 0x77], temp: 18.7 },
    ],
    ports: new Map([
      [2, buildPort(2, { mode: 2, value: 1, interrupt: 2, isButton: 1 })],
      [4, buildPort(4, { value: 1 })],
      [5, buildPort(5, { value: 1 })],
      [12, buildPort(12, { value: 0 })],
      [13, buildPort(13, { value: 0 })],
      [14, buildPort(14, { mode: 128, value: 0 })],
      [15, buildPort(15, { mode: 0, value: 1, interrupt: 3, isButton: 1 })],
      [16, buildPort(16, { mode: 130, value: 512 })],
      [17, buildPort(17, { mode: 130, value: 23 })],
      [18, buildPort(18, { mode: 131, value: 128 })],
      [19, buildPort(19, { mode: 129, value: 0 })],
    ]),
  };

  const files = {
    '/': [
      { isDir: 1, isFile: 0, size: 0, name: 'www' },
      { isDir: 1, isFile: 0, size: 0, name: 'tmp' },
    ],
    '/www': [
      { isDir: 0, isFile: 1, size: 3600, name: 'index.html' },
      { isDir: 1, isFile: 0, size: 0, name: 'assets' },
      { isDir: 0, isFile: 1, size: 6640, name: 'default.json' },
      { isDir: 0, isFile: 1, size: 1280, name: 'scripts.txt' },
    ],
    '/www/assets': [
      { isDir: 0, isFile: 1, size: 114560, name: 'index.js' },
      { isDir: 0, isFile: 1, size: 13550, name: 'style.css' },
    ],
    '/tmp': [
      { isDir: 0, isFile: 1, size: 2140, name: 'dashboard.json' },
      { isDir: 0, isFile: 1, size: 116, name: 'device.io' },
      { isDir: 0, isFile: 1, size: 120, name: 'gpio.io' },
    ],
  };

  const scanList = [
    { id: 0, isHidden: 0, encryptionType: 4, channel: 1, rssi: -42, ssid: 'Home' },
    { id: 1, isHidden: 0, encryptionType: 4, channel: 6, rssi: -61, ssid: 'Home-2.4G' },
    { id: 2, isHidden: 1, encryptionType: 0, channel: 11, rssi: -78, ssid: '' },
  ];

  const encode = (key, data) => {
    const buffer = data ? struct.set(key, data) : struct.set(key);
    return buffer ? Buffer.from(buffer) : null;
  };

  const send = (socket, key, data) => {
    const payload = encode(key, data);
    if (payload && socket.readyState === socket.OPEN) socket.send(payload);
  };

  const sendPorts = (socket, ports) => {
    for (const port of ports) send(socket, 'PORT', port);
  };

  const server = createServer(async (request, response) => {
    const url = new URL(request.url ?? '/', 'http://mock');
    const sendJson = (status, body) => {
      response.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8' });
      response.end(typeof body === 'string' ? body : JSON.stringify(body));
    };

    if (url.pathname === '/default.json') {
      response.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
      response.end(defaultJsonText);
      return;
    }

    if (url.pathname === '/fs') {
      if (request.method === 'GET') {
        const file = url.searchParams.get('file') ?? '';
        if (file === '/tmp/dashboard.json') {
          response.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
          response.end(dashboardJson);
          return;
        }
        if (file === '/www/scripts.txt') {
          response.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
          response.end(scriptsTxt);
          return;
        }
        if (file === '/tmp/gpio.io') {
          response.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
          response.end('');
          return;
        }
        response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
        response.end('Not found');
        return;
      }
      sendJson(200, { state: 1 });
      return;
    }

    if (['/cmd', '/update'].includes(url.pathname)) {
      sendJson(200, { state: 1 });
      return;
    }

    if (url.pathname === '/' || extname(url.pathname) === '') {
      response.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
      response.end('Vuesp mock device');
      return;
    }

    response.writeHead(404);
    response.end();
  });

  const wss = new WebSocketServer({ server });

  wss.on('connection', (socket) => {
    const alive = setInterval(() => send(socket, 'PING'), 2000);

    socket.on('close', () => clearInterval(alive));
    socket.on('error', () => clearInterval(alive));

    socket.on('message', (raw) => {
      const buffer = Buffer.isBuffer(raw) ? raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength) : raw;
      // Key-only commands (for example PING) are a single byte and cannot be
      // decoded as a full struct.
      let message;
      try {
        message = struct.get(buffer);
      } catch {
        return;
      }
      if (!message?.key) return;

      // vuesp-struct returns the numeric KEY value, while the handlers below
      // are keyed by name.
      const messageKey = keyNames[message.key] ?? message.key;

      switch (messageKey) {
        case 'INFO':
          send(socket, 'INFO', deviceState.info);
          break;
        case 'SETTINGS':
          send(socket, 'SETTINGS', deviceState.settings);
          break;
        case 'DEVICE':
          deviceState.device.now = Math.floor(Date.now() / 1000);
          send(socket, 'DEVICE', deviceState.device);
          break;
        case 'PORT': {
          const object = message.object ?? {};
          const command = object.command ?? 0;
          if (command === COMMAND.GPIO_GET_ALL) {
            sendPorts(socket, [...deviceState.ports.values()]);
            break;
          }
          const port = deviceState.ports.get(object.gpio);
          if (!port) break;
          if (command === COMMAND.GPIO_SET || command === COMMAND.GPIO_CHANGE) {
            Object.assign(port, {
              value: object.value ?? port.value,
              mode: object.mode ?? port.mode,
              interrupt: object.interrupt ?? port.interrupt,
              isButton: object.isButton ?? port.isButton,
            });
          }
          if (command !== COMMAND.GPIO_SAVE) send(socket, 'PORT', port);
          break;
        }
        case 'BUFFER':
          send(socket, 'BUFFER', deviceState.buffer);
          break;
        case 'MODBUS': {
          const object = message.object ?? {};
          // CMD.SET = 2, CMD.SAVE = 254
          if (object.command === 2 || object.command === 254) {
            if (typeof object.role === 'number') modbusState.role = object.role;
            if (typeof object.address === 'number') modbusState.address = object.address;
            if (typeof object.baud === 'number') modbusState.baud = object.baud;
            if (typeof object.parity === 'number') modbusState.parity = object.parity;
            if (typeof object.stopBits === 'number') modbusState.stopBits = object.stopBits;
          }
          send(socket, 'MODBUS', modbusStatus(Math.floor(Date.now() / 1000)));
          break;
        }
        case 'MODBUSPROFILE':
          send(socket, 'MODBUSPROFILE', modbusProfile);
          break;
        case 'MODBUSRAW':
          send(socket, 'MODBUSRAW', modbusRaw);
          break;
        case 'DALLAS':
          for (const sensor of deviceState.dallas) send(socket, 'DALLAS', sensor);
          break;
        case 'DISCOVERY':
          break;
        case 'FILES': {
          const object = message.object ?? {};
          if ((object.command ?? 0) !== 0) break;
          const key = String(object.name ?? '').replace(/\/+$/, '') || '/';
          const list = files[key] ?? [];
          for (const file of list) send(socket, 'FILES', { command: 0, ...file });
          break;
        }
        case 'SCAN':
          for (const network of scanList) send(socket, 'SCAN', network);
          break;
        case 'REBOOT':
          break;
        case 'PING':
          send(socket, 'PING');
          break;
        default:
          break;
      }
    });

    send(socket, 'INFO', deviceState.info);
    send(socket, 'SETTINGS', deviceState.settings);
    send(socket, 'DEVICE', deviceState.device);
    send(socket, 'BUFFER', deviceState.buffer);
    sendPorts(socket, [...deviceState.ports.values()]);
    for (const sensor of deviceState.dallas) send(socket, 'DALLAS', sensor);
    send(socket, 'MODBUS', modbusStatus(Math.floor(Date.now() / 1000)));
    send(socket, 'MODBUSPROFILE', modbusProfile);
    send(socket, 'MODBUSRAW', modbusRaw);
  });

  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(port, '127.0.0.1', resolve);
  });

  if (!silent) {
    console.log(`Mock device listening on http://127.0.0.1:${port} (ws /esp)`);
  }

  return {
    port,
    close: () =>
      new Promise((resolve) => {
        for (const client of wss.clients) client.close();
        wss.close(() => server.close(() => resolve()));
      }),
  };
};

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const port = Number(process.env.MOCK_PORT ?? 4173);
  await startMockDevice({ port });
}
