<script setup lang="ts">
import type { IListItem } from '@/types';

import { computed, nextTick, ref, watch } from 'vue';

import { KEY, CMD } from '@/generated/protocol';
import { useConnection } from '@/composables/useConnection';
import { useLocale } from '@/composables/useLocale';

import { VButton, VCard, VCheckbox, VSelect, VTextField } from 'vuesp-components/components';

const { $t } = useLocale();

interface ILogLine {
  id: number;
  dir: 'rx' | 'tx';
  text: string;
  hex: string;
}

const { main, onSend } = useConnection((send) => {
  send(KEY.RS232, { command: CMD.GET });
});

const rs = computed(() => main.value.rs232);

const baud = ref(9600);
const parity = ref(0);
const stopBits = ref(1);
const enabled = ref(true);
const dirty = ref(false);

const baudItems: IListItem<number>[] = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200].map((value) => ({
  name: String(value),
  value,
}));
const parityItems: IListItem<number>[] = [
  { name: $t('rs232.parityNone'), value: 0 },
  { name: $t('rs232.parityEven'), value: 1 },
  { name: $t('rs232.parityOdd'), value: 2 },
];
const stopItems: IListItem<number>[] = [
  { name: '1', value: 1 },
  { name: '2', value: 2 },
];
const lineEndingItems: IListItem<string>[] = [
  { name: $t('rs232.lineNone'), value: 'none' },
  { name: $t('rs232.lineCr'), value: 'cr' },
  { name: $t('rs232.lineLf'), value: 'lf' },
  { name: $t('rs232.lineCrlf'), value: 'crlf' },
];

const selectName = <T,>(items: IListItem<T>[], value: T) => items.find((i) => i.value === value)?.name ?? String(value);

const onLineEnding = ({ value }: IListItem<string>) => {
  lineEnding.value = value;
};

const lines = ref<ILogLine[]>([]);
const input = ref('');
const hexMode = ref(false);
const lineEnding = ref('none');
const sendError = ref('');
const logEl = ref<HTMLElement | null>(null);
let lineId = 0;
let lastRxCount = 0;

watch(
  () => main.value.rs232,
  (value) => {
    if (!dirty.value) {
      baud.value = value.baud;
      parity.value = value.parity;
      stopBits.value = value.stopBits;
      enabled.value = Boolean(value.enabled);
    }
  },
  { immediate: true },
);

watch(
  () => rs.value.rxCount,
  (count) => {
    const value = rs.value;
    if (!value.rxLen) {
      lastRxCount = count;
      return;
    }
    if (count < lastRxCount) lastRxCount = 0;
    if (count <= lastRxCount) return;

    const bytes = (value.rx || []).slice(0, value.rxLen);
    lines.value.push({ id: ++lineId, dir: 'rx', text: bytesToText(bytes), hex: bytesToHex(bytes) });
    if (lines.value.length > 500) lines.value.splice(0, lines.value.length - 500);
    lastRxCount = count;
    scroll();
  },
);

const bytesToHex = (bytes: number[]) => bytes.map((v) => v.toString(16).padStart(2, '0')).join(' ');

const bytesToText = (bytes: number[]) => {
  const text = new TextDecoder().decode(new Uint8Array(bytes));
  return text.replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f]/g, '.');
};

const scroll = () => {
  nextTick(() => {
    if (logEl.value) logEl.value.scrollTop = logEl.value.scrollHeight;
  });
};

const statusText = computed(() => {
  if (rs.value.status === 1) return $t('rs232.opened');
  if (rs.value.status === 2) return $t('rs232.error');
  return $t('rs232.closed');
});

const onBaud = ({ value }: IListItem<number>) => {
  baud.value = value;
  dirty.value = true;
};
const onParity = ({ value }: IListItem<number>) => {
  parity.value = value;
  dirty.value = true;
};
const onStop = ({ value }: IListItem<number>) => {
  stopBits.value = value;
  dirty.value = true;
};
const onEnabled = (value: boolean) => {
  enabled.value = value;
  dirty.value = true;
};

const onApply = () => {
  onSend(KEY.RS232, {
    command: CMD.SET,
    enabled: enabled.value ? 1 : 0,
    baud: baud.value,
    dataBits: 8,
    parity: parity.value,
    stopBits: stopBits.value,
    flags: rs.value.flags,
    rxCount: rs.value.rxCount,
    txCount: rs.value.txCount,
    rxLen: 0,
    txLen: 0,
    rx: [],
    tx: [],
  });
  dirty.value = false;
};

const onReset = () => {
  baud.value = 9600;
  parity.value = 0;
  stopBits.value = 1;
  enabled.value = true;
  dirty.value = true;
  onApply();
};

const buildPayload = () => {
  let bytes: number[] = [];
  if (hexMode.value) {
    const clean = input.value.replace(/[^0-9a-fA-F]/g, '');
    for (let i = 0; i + 1 < clean.length; i += 2) bytes.push(parseInt(clean.slice(i, i + 2), 16));
  } else {
    bytes = Array.from(new TextEncoder().encode(input.value));
  }
  if (lineEnding.value === 'cr' || lineEnding.value === 'crlf') bytes.push(13);
  if (lineEnding.value === 'lf' || lineEnding.value === 'crlf') bytes.push(10);
  return bytes.slice(0, 128);
};

const onSendText = () => {
  sendError.value = '';
  if (rs.value.status !== 1) {
    sendError.value = $t('rs232.sendError');
    return;
  }
  const bytes = buildPayload();
  if (!bytes.length) return;

  const tx = Array.from({ length: 128 }, (_, i) => bytes[i] ?? 0);
  onSend(KEY.RS232, {
    ...rs.value,
    command: CMD.WRITE,
    rx: [],
    rxLen: 0,
    tx,
    txLen: bytes.length,
  });

  lines.value.push({ id: ++lineId, dir: 'tx', text: hexMode.value ? '' : input.value, hex: bytesToHex(bytes) });
  if (lines.value.length > 500) lines.value.splice(0, lines.value.length - 500);
  input.value = '';
  scroll();
};

const onClear = () => {
  lines.value = [];
  sendError.value = '';
};
</script>

<template>
  <div class="flex flex-col gap-4">
    <VCard variant="glass" padding="lg" class="relative z-20 overflow-visible">
      <div class="flex flex-wrap items-start justify-between gap-4">
        <div>
          <h3 class="text-base font-semibold">{{ $t('rs232.settings') }}</h3>
          <p class="mt-1 text-xs text-gray-dark-25 dark:text-gray-light-10">{{ $t('rs232.subtitle') }}</p>
        </div>
        <div class="flex flex-wrap items-center gap-3 text-sm">
          <span
            class="rounded-full px-3 py-1 text-xs font-semibold"
            :class="rs.status === 1 ? 'bg-emerald-500/10 text-emerald-500' : rs.status === 2 ? 'bg-red-500/10 text-red-500' : 'bg-amber-500/10 text-amber-500'"
          >
            {{ statusText }}
          </span>
          <span class="font-mono text-xs text-gray-dark-25 dark:text-gray-light-10">{{ $t('rs232.rx') }} {{ rs.rxCount }} · {{ $t('rs232.tx') }} {{ rs.txCount }}</span>
        </div>
      </div>

      <div class="mt-4 grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
        <VSelect :model-value="selectName(baudItems, baud)" :items="baudItems" :label="$t('rs232.baud')" @change="onBaud" />
        <VSelect :model-value="selectName(parityItems, parity)" :items="parityItems" :label="$t('rs232.parity')" @change="onParity" />
        <VSelect :model-value="selectName(stopItems, stopBits)" :items="stopItems" :label="$t('rs232.stopBits')" @change="onStop" />
      </div>

      <div class="mt-4 flex flex-wrap items-center gap-4">
        <VCheckbox :model-value="enabled" @update:model-value="onEnabled">{{ $t('rs232.enabled') }}</VCheckbox>
        <VButton color="blue" @click="onApply">{{ $t('rs232.apply') }}</VButton>
        <VButton variant="outline" color="gray" @click="onReset">{{ $t('rs232.reset') }}</VButton>
        <span v-if="dirty" class="text-xs text-amber-600 dark:text-amber-400">{{ $t('rs232.unsaved') }}</span>
      </div>
    </VCard>

    <VCard variant="glass" padding="lg" class="relative z-0">
      <div class="flex flex-wrap items-center justify-between gap-4">
        <h3 class="text-base font-semibold">{{ $t('rs232.terminal') }}</h3>
        <VButton variant="ghost" color="gray" @click="onClear">{{ $t('rs232.clear') }}</VButton>
      </div>

      <div ref="logEl" class="mt-4 h-80 overflow-auto rounded-lg bg-black/40 p-4 font-mono text-xs leading-6">
        <p v-if="!lines.length" class="text-gray-dark-25 dark:text-gray-light-10">{{ $t('rs232.empty') }}</p>
        <div v-for="line in lines" :key="line.id" class="break-all">
          <span :class="line.dir === 'rx' ? 'text-emerald-400' : 'text-sky-400'">{{ line.dir === 'rx' ? 'RX' : 'TX' }}</span>
          <span class="ml-2">{{ hexMode ? line.hex : line.text || line.hex }}</span>
        </div>
      </div>

      <div class="mt-4 flex flex-col gap-4">
        <VTextField :model-value="input" :label="$t('rs232.input')" @update:model-value="(v) => (input = v)" @enter="onSendText" />

        <div class="flex flex-wrap items-center gap-4">
          <div class="w-48">
            <VSelect :model-value="selectName(lineEndingItems, lineEnding)" :items="lineEndingItems" :label="$t('rs232.lineEnding')" @change="onLineEnding" />
          </div>
          <div class="flex h-12 items-center">
            <VCheckbox :model-value="hexMode" @update:model-value="(v) => (hexMode = Boolean(v))">{{ $t('rs232.hex') }}</VCheckbox>
          </div>
          <VButton class="ml-auto" color="blue" :disabled="rs.status !== 1" @click="onSendText">{{ $t('rs232.send') }}</VButton>
        </div>
      </div>

      <p v-if="sendError" class="mt-3 text-xs text-red-500">{{ sendError }}</p>
      <p class="mt-3 text-xs text-gray-dark-25 dark:text-gray-light-10">{{ $t('rs232.hint') }}</p>
    </VCard>
  </div>
</template>
