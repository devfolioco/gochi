// daemon.ts — the long-lived process that owns the serial port.
//
// Listens on a Unix domain socket (~/.tamagotchi/daemon.sock) and speaks
// plain HTTP over it so the request handlers are identical in shape to
// the TCP HTTP server. The HTTP server (server.ts) is a thin TCP→UDS
// reverse-proxy on top of this.
//
// Why not just merge daemon + HTTP server? Two reasons: (1) the HTTP
// listener is opt-in via `tamagotchi server enable/disable`, but the
// daemon must always run if the device is to be usable; (2) keeping the
// kernel-facing serial code in one process means there's a single owner
// of /dev/cu.usbmodem* with no possibility of contention.

import { chmodSync } from "node:fs";
import { createServer, type IncomingMessage, type ServerResponse } from "node:http";

import { watchDevices } from "./discovery";
import {
  DAEMON_SOCKET,
  clearStaleSocket,
  ensureDaemonDir,
} from "./ipc";
import { SerialTransport } from "./transport";

export const DAEMON_VERSION = "0.1.0";

// Device wraps the current SerialTransport (if any) and listens to the
// discovery watcher for attach/detach. Only one port is connected at a
// time — first-attached wins. Multi-device support is a future thing.
class Device {
  private transport: SerialTransport | null = null;
  private path: string | null = null;
  private connecting = false;
  // When stopped, the daemon releases the serial port and refuses to
  // open any new ones. Used for arduino-cli flashing — see
  // `tamagotchi stop` and the firmware Makefile.
  private stopped = false;

  isConnected(): boolean {
    return !!this.transport?.isOpen();
  }
  port(): string | null {
    return this.path;
  }
  isStopped(): boolean {
    return this.stopped;
  }

  async stop(): Promise<void> {
    if (this.stopped) return;
    this.stopped = true;
    if (this.transport) {
      try {
        await this.transport.close();
      } catch {}
    }
    this.transport = null;
    this.path = null;
    log("stopped (serial port released)");
  }

  start(): void {
    if (!this.stopped) return;
    this.stopped = false;
    log("started (will reconnect on next discovery tick)");
  }

  async attach(path: string): Promise<void> {
    if (this.stopped || this.connecting || this.isConnected()) return;
    this.connecting = true;
    try {
      const t = new SerialTransport(path);
      await t.open();
      t.on("close", () => {
        log(`disconnected (${this.path})`);
        this.transport = null;
        this.path = null;
      });
      t.on("error", (e: Error) => log("serial error:", e.message));
      this.transport = t;
      this.path = path;
      log(`connected to ${path}`);
    } catch (e: any) {
      log(`attach failed for ${path}:`, e?.message || e);
      this.transport = null;
      this.path = null;
    } finally {
      this.connecting = false;
    }
  }

  detach(path: string): void {
    if (this.path !== path) return;
    try {
      void this.transport?.close();
    } catch {}
    this.transport = null;
    this.path = null;
  }

  async send(line: string): Promise<string | null> {
    if (!this.isConnected()) return null;
    try {
      return await this.transport!.send(line);
    } catch (e: any) {
      log("send failed:", e?.message || e);
      return null;
    }
  }
}

function log(...args: unknown[]): void {
  const ts = new Date().toISOString();
  console.log(`[${ts}]`, ...args);
}

function readBody(req: IncomingMessage): Promise<string> {
  return new Promise((resolve, reject) => {
    let body = "";
    req.on("data", (c) => (body += c));
    req.on("end", () => resolve(body));
    req.on("error", reject);
  });
}

async function readJson(req: IncomingMessage): Promise<Record<string, unknown>> {
  try {
    const raw = await readBody(req);
    return raw ? (JSON.parse(raw) as Record<string, unknown>) : {};
  } catch {
    return {};
  }
}

function send(res: ServerResponse, data: unknown, status = 200): void {
  res.statusCode = status;
  res.setHeader("Content-Type", "application/json");
  res.end(JSON.stringify(data, null, 2) + "\n");
}

export async function runDaemon(): Promise<void> {
  ensureDaemonDir();
  await clearStaleSocket();

  const device = new Device();

  // Subscribe to hotplug events. attach() races against connecting; we
  // ignore further attaches while one device is already connected.
  const watcher = watchDevices((e) => {
    if (e.type === "attach") void device.attach(e.path);
    else device.detach(e.path);
  });

  const sendCmd = async (res: ServerResponse, line: string): Promise<void> => {
    if (!device.isConnected()) {
      return send(res, {
        ok: true,
        connected: false,
        message: "device offline; request ignored",
      });
    }
    const response = await device.send(line);
    if (response === null) {
      return send(res, {
        ok: false,
        connected: device.isConnected(),
        message: "no response from device",
      });
    }
    send(res, { ok: true, connected: true, response });
  };

  const server = createServer(async (req: IncomingMessage, res: ServerResponse) => {
    const url = req.url || "/";
    const path = url.split("?")[0];
    const method = req.method || "GET";

    try {
      if (method === "GET" && path === "/health") {
        return send(res, {
          ok: true,
          connected: device.isConnected(),
          stopped: device.isStopped(),
          port: device.port(),
          version: DAEMON_VERSION,
        });
      }

      if (method === "POST" && path === "/stop") {
        await device.stop();
        return send(res, { ok: true, stopped: true });
      }
      if (method === "POST" && path === "/start") {
        device.start();
        return send(res, { ok: true, stopped: false });
      }

      if (method === "POST" && path === "/face") {
        const body = await readJson(req);
        const name = body?.name;
        if (typeof name !== "string" || !name) {
          return send(res, { ok: false, connected: device.isConnected(), message: "missing name" });
        }
        return sendCmd(res, `SHOW face ${name}`);
      }
      if (method === "POST" && path === "/text") {
        const body = await readJson(req);
        const text = (body?.text ?? "") as string;
        if (typeof text !== "string") {
          return send(res, { ok: false, connected: device.isConnected(), message: "invalid text" });
        }
        return sendCmd(res, `SHOW text ${text}`);
      }
      if (method === "POST" && path === "/image") {
        const body = await readJson(req);
        const data = body?.data;
        if (typeof data !== "string" || !data) {
          return send(res, { ok: false, connected: device.isConnected(), message: "missing data" });
        }
        // The firmware's line buffer is 1536 bytes including the verb
        // prefix; reject anything that can't fit so we don't silently
        // truncate. 1368 chars is base64 for exactly 1024 raw bytes.
        if (data.length > 1500) {
          return send(res, {
            ok: false,
            connected: device.isConnected(),
            message: `image payload too large: ${data.length} chars (max 1500)`,
          });
        }
        return sendCmd(res, `SHOW image ${data}`);
      }
      if (method === "POST" && path === "/mood") {
        const body = await readJson(req);
        const name = body?.name;
        if (typeof name !== "string" || !name) {
          return send(res, { ok: false, connected: device.isConnected(), message: "missing name" });
        }
        return sendCmd(res, `SET mood ${name}`);
      }
      if (method === "GET" && path === "/state") return sendCmd(res, "GET state");
      if (method === "GET" && path === "/fps") return sendCmd(res, "GET fps");
      if (method === "GET" && path === "/faces") return sendCmd(res, "LIST faces");
      if (method === "POST" && path === "/ping") return sendCmd(res, "PING");

      send(res, { ok: false, message: "not found" }, 404);
    } catch (e: any) {
      send(res, { ok: false, connected: device.isConnected(), message: e?.message || String(e) });
    }
  });

  const cleanup = () => {
    try {
      watcher.stop();
    } catch {}
    try {
      server.close();
    } catch {}
    process.exit(0);
  };
  process.on("SIGINT", cleanup);
  process.on("SIGTERM", cleanup);

  server.listen(DAEMON_SOCKET, () => {
    // Lock the socket to the owning user; nobody else on the box should
    // be able to drive the device.
    if (process.platform !== "win32") {
      try {
        chmodSync(DAEMON_SOCKET, 0o600);
      } catch {}
    }
    log(`Tamagotchi daemon listening on ${DAEMON_SOCKET}`);
  });
}
