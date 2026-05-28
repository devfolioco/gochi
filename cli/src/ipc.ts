// ipc.ts — shared IPC layer between the CLI (or HTTP server) and the daemon.
//
// The daemon owns the serial port and exposes the command surface over a
// Unix domain socket at ~/.tamagotchi/daemon.sock. We deliberately speak
// plain HTTP over that socket so the daemon's request handlers are
// identical in shape to the HTTP-server's, and the HTTP frontend becomes
// a thin reverse-proxy.
//
// On Windows, Node maps `socketPath` onto a named pipe; we use
// \\.\pipe\tamagotchi-daemon there. The launchd auto-start is macOS-only
// regardless, but the runtime works cross-platform.

import { request as httpRequest } from "node:http";
import { existsSync, mkdirSync, unlinkSync } from "node:fs";
import { homedir, platform } from "node:os";
import { join } from "node:path";

export const DAEMON_DIR = join(homedir(), ".tamagotchi");

export const DAEMON_SOCKET =
  platform() === "win32"
    ? "\\\\.\\pipe\\tamagotchi-daemon"
    : join(DAEMON_DIR, "daemon.sock");

export const DAEMON_PID_FILE = join(DAEMON_DIR, "daemon.pid");
export const DAEMON_LOG_OUT = join(DAEMON_DIR, "daemon.out.log");
export const DAEMON_LOG_ERR = join(DAEMON_DIR, "daemon.err.log");
export const HTTP_LOG_OUT = join(DAEMON_DIR, "http.out.log");
export const HTTP_LOG_ERR = join(DAEMON_DIR, "http.err.log");

export function ensureDaemonDir(): void {
  if (!existsSync(DAEMON_DIR)) mkdirSync(DAEMON_DIR, { recursive: true, mode: 0o700 });
}

// Stale-socket cleanup: if a previous daemon crashed without removing the
// socket file, listen() will EADDRINUSE. We try to connect first — if no
// one answers, the file is stale and safe to unlink.
export async function clearStaleSocket(): Promise<void> {
  if (platform() === "win32") return;
  if (!existsSync(DAEMON_SOCKET)) return;
  const reachable = await new Promise<boolean>((resolve) => {
    const req = httpRequest(
      { socketPath: DAEMON_SOCKET, path: "/health", method: "GET", timeout: 250 },
      (res) => {
        res.resume();
        resolve(true);
      },
    );
    req.on("error", () => resolve(false));
    req.on("timeout", () => {
      req.destroy();
      resolve(false);
    });
    req.end();
  });
  if (!reachable) {
    try {
      unlinkSync(DAEMON_SOCKET);
    } catch {}
  }
}

export type IpcResult = {
  ok: boolean;
  status: number;
  json: any;
};

// One-shot HTTP request to the daemon over its Unix socket. The HTTP
// frontend uses this for proxying; the CLI uses it as its default
// transport (with a TCP fallback when GOCHI_URL is set).
export function daemonRequest(
  method: "GET" | "POST",
  path: string,
  body?: unknown,
): Promise<IpcResult> {
  return new Promise((resolve, reject) => {
    const payload = body !== undefined ? JSON.stringify(body) : undefined;
    const req = httpRequest(
      {
        socketPath: DAEMON_SOCKET,
        method,
        path,
        headers: payload
          ? { "Content-Type": "application/json", "Content-Length": Buffer.byteLength(payload) }
          : undefined,
        timeout: 5000,
      },
      (res) => {
        let buf = "";
        res.setEncoding("utf8");
        res.on("data", (c) => (buf += c));
        res.on("end", () => {
          let json: any = null;
          try {
            json = buf ? JSON.parse(buf) : null;
          } catch {
            // leave null; the caller will see ok=false
          }
          resolve({ ok: res.statusCode! < 400, status: res.statusCode || 0, json });
        });
      },
    );
    req.on("error", reject);
    req.on("timeout", () => {
      req.destroy(new Error("daemon request timed out"));
    });
    if (payload) req.write(payload);
    req.end();
  });
}
