<script setup lang="ts">
import { computed, ref, watch } from 'vue';

import type { ISerialConsoleTransport, TSerialConsoleStatus } from 'vuesp-components/components';
import { SerialConsole, VSelect } from 'vuesp-components/components';

import type { IListItem } from '@/types';

import { KEY, CMD } from '@/generated/protocol';
import { useConnection } from '@/composables/useConnection';
import { useLocale } from '@/composables/useLocale';

const { $t, i18n } = useLocale();

interface IConsoleLabels {
  usb: string;
  noPort: string;
}

const { main, onSend } = useConnection((send) => {
  send(KEY.RS232, { command: CMD.GET });
});

const rs = computed(() => main.value.rs232);

const baud = ref(9600);
const parity = ref(0);
const stopBits = ref(1);

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

const selectName = <T,>(items: IListItem<T>[], value: T) => items.find((i) => i.value === value)?.name ?? String(value);

watch(
  () => main.value.rs232,
  (value) => {
    baud.value = value.baud;
    parity.value = value.parity;
    stopBits.value = value.stopBits;
  },
  { immediate: true },
);

const applyConfig = (open: boolean) => {
  onSend(KEY.RS232, {
    command: CMD.SET,
    enabled: open ? 1 : 0,
    baud: baud.value,
    dataBits: 8,
    parity: parity.value,
    stopBits: stopBits.value,
    flags: rs.value.flags,
    rxCount: rs.value.rxCount,
    txCount: rs.value.txCount,
    rxLen: 0,
    txLen: 0,
    rx: Array.from({ length: 128 }, () => 0),
    tx: Array.from({ length: 128 }, () => 0),
  });
};

const onBaud = ({ value }: IListItem<number>) => {
  baud.value = value;
  applyConfig(rs.value.status === 1);
};
const onParity = ({ value }: IListItem<number>) => {
  parity.value = value;
  applyConfig(rs.value.status === 1);
};
const onStop = ({ value }: IListItem<number>) => {
  stopBits.value = value;
  applyConfig(rs.value.status === 1);
};

const listeners = new Set<(bytes: Uint8Array) => void>();
let lastRxCount = 0;

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
    lastRxCount = count;
    for (const listener of listeners) listener(new Uint8Array(bytes));
  },
);

const transport: ISerialConsoleTransport = {
  start: () => {
    applyConfig(true);
  },
  stop: () => {
    applyConfig(false);
  },
  send: async (value) => {
    if (rs.value.status !== 1) return false;

    const bytes = Array.from(value.slice(0, 128));
    if (!bytes.length) return false;

    onSend(KEY.RS232, {
      ...rs.value,
      command: CMD.WRITE,
      rx: Array.from({ length: 128 }, () => 0),
      rxLen: 0,
      tx: Array.from({ length: 128 }, (_, index) => bytes[index] ?? 0),
      txLen: bytes.length,
    });
    return true;
  },
  subscribe: (listener) => {
    listeners.add(listener);
    return () => listeners.delete(listener);
  },
};

const consoleLocale = computed<'ru' | 'en'>(() => (i18n.state.locale === 'ru' ? 'ru' : 'en'));

const consoleStatus = computed<TSerialConsoleStatus>(() => {
  if (rs.value.status === 1) return 'connected';
  if (rs.value.status === 2) return 'error';
  return 'disconnected';
});

const parityChar = computed(() => {
  if (rs.value.parity === 1) return 'E';
  if (rs.value.parity === 2) return 'O';
  return 'N';
});

const consolePortLabel = computed(() => `${rs.value.baud} ${rs.value.dataBits || 8}${parityChar.value}${rs.value.stopBits}`);
const consoleLabels: IConsoleLabels = { usb: 'RS232', noPort: 'RS232' };
</script>

<template>
  <div class="flex flex-col gap-4">
    <SerialConsole
      :status="consoleStatus"
      :locale="consoleLocale"
      :port-label="consolePortLabel"
      :labels="consoleLabels"
      :transport="transport"
      initial-line-ending="crlf"
      storage-key="vuesp.kc868-a6:serial"
    >
      <template #header-actions>
        <div class="w-40 shrink-0">
          <VSelect
            hide-message
            :model-value="selectName(baudItems, baud)"
            :items="baudItems"
            :label="$t('rs232.baud')"
            :disabled="rs.status === 1"
            @change="onBaud"
          />
        </div>
      </template>
      <template #settings>
        <div class="grid grid-cols-1 gap-4 sm:grid-cols-3">
          <VSelect
            hide-message
            :model-value="selectName(parityItems, parity)"
            :items="parityItems"
            :label="$t('rs232.parity')"
            :disabled="rs.status === 1"
            @change="onParity"
          />
          <VSelect
            hide-message
            :model-value="selectName(stopItems, stopBits)"
            :items="stopItems"
            :label="$t('rs232.stopBits')"
            :disabled="rs.status === 1"
            @change="onStop"
          />
        </div>
      </template>
    </SerialConsole>
  </div>
</template>
