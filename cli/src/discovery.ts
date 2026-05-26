// discovery.ts — hotplug-style watcher for connected Tamagotchi devices.
//
// SerialPort.list() is a cheap metadata query — it asks the OS for the
// current set of serial devices without opening any of them. We poll it
// every ~1.5s and compare against the last-seen set to detect attach /
// detach. Crucially we *never* open a non-Espressif port: doing so on
// arbitrary USB-serial devices (Arduinos, other ESPs) would toggle
// DTR/RTS and reset whatever's plugged in, which the old probe-every-
// port code did every 5 seconds.

import { SerialPort } from "serialport";

// USB VID for Espressif's native CDC (ESP32-S2/S3/C3 with CDCOnBoot).
const ESPRESSIF_VID = "303a";
const POLL_MS = 1500;

export type DiscoveryEvent =
  | { type: "attach"; path: string }
  | { type: "detach"; path: string };

export type DiscoveryHandle = { stop: () => void };

function normalize(p: string): string {
  // On macOS SerialPort.list() returns /dev/tty.* (blocking, waits for
  // carrier on open); we want /dev/cu.* (non-blocking call-up device).
  if (process.platform === "darwin" && p.startsWith("/dev/tty.")) {
    return p.replace("/dev/tty.", "/dev/cu.");
  }
  return p;
}

async function listEspressifPaths(): Promise<Set<string>> {
  const ports = await SerialPort.list();
  const out = new Set<string>();
  for (const p of ports) {
    if ((p.vendorId || "").toLowerCase() !== ESPRESSIF_VID) continue;
    if (!p.path) continue;
    out.add(normalize(p.path));
  }
  return out;
}

// Subscribe to attach/detach events for Espressif USB-serial devices.
// The callback is invoked synchronously for the initial snapshot
// (everything already plugged in shows up as "attach") and then on
// every change.
export function watchDevices(
  onEvent: (e: DiscoveryEvent) => void,
): DiscoveryHandle {
  let known = new Set<string>();
  let stopped = false;

  const tick = async (): Promise<void> => {
    if (stopped) return;
    let live: Set<string>;
    try {
      live = await listEspressifPaths();
    } catch (e: any) {
      // SerialPort.list() can transiently fail; just try again next tick.
      console.error(`[discovery] list failed: ${e?.message || e}`);
      return;
    }
    for (const p of live) if (!known.has(p)) onEvent({ type: "attach", path: p });
    for (const p of known) if (!live.has(p)) onEvent({ type: "detach", path: p });
    known = live;
  };

  // Kick the first poll immediately so the daemon doesn't sit idle for
  // 1.5s on startup when the device is already attached.
  void tick();
  const timer = setInterval(tick, POLL_MS);

  return {
    stop: () => {
      stopped = true;
      clearInterval(timer);
    },
  };
}
