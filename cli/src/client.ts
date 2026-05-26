// client.ts — the CLI's transport to the daemon.
//
// Default: HTTP over the daemon's Unix domain socket. Fast, local, and
// works even with the TCP HTTP frontend disabled.
// Back-compat: if TAMAGOTCHI_URL is set, talk to that TCP endpoint
// instead — useful for hitting a remote daemon or for scripts that have
// always pointed at http://localhost:7474.

import { request as httpRequest } from "node:http";
import { existsSync } from "node:fs";

import { DAEMON_SOCKET } from "./ipc";

const TCP_URL = process.env.TAMAGOTCHI_URL;
const USE_TCP = !!TCP_URL;

export type Result = {
  ok: boolean;
  connected?: boolean;
  response?: unknown;
  message?: string;
  [k: string]: unknown;
};

function tcpCall(method: "GET" | "POST", path: string, body?: unknown): Promise<Result> {
  return new Promise((resolve, reject) => {
    const url = new URL(path, TCP_URL!);
    const payload = body !== undefined ? JSON.stringify(body) : undefined;
    const req = httpRequest(
      {
        hostname: url.hostname,
        port: url.port || 80,
        method,
        path: url.pathname + url.search,
        headers: payload
          ? { "Content-Type": "application/json", "Content-Length": Buffer.byteLength(payload) }
          : undefined,
      },
      (res) => {
        let buf = "";
        res.setEncoding("utf8");
        res.on("data", (c) => (buf += c));
        res.on("end", () => {
          try {
            resolve(buf ? (JSON.parse(buf) as Result) : ({ ok: false } as Result));
          } catch (e) {
            reject(e);
          }
        });
      },
    );
    req.on("error", reject);
    if (payload) req.write(payload);
    req.end();
  });
}

function udsCall(method: "GET" | "POST", path: string, body?: unknown): Promise<Result> {
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
      },
      (res) => {
        let buf = "";
        res.setEncoding("utf8");
        res.on("data", (c) => (buf += c));
        res.on("end", () => {
          try {
            resolve(buf ? (JSON.parse(buf) as Result) : ({ ok: false } as Result));
          } catch (e) {
            reject(e);
          }
        });
      },
    );
    req.on("error", reject);
    if (payload) req.write(payload);
    req.end();
  });
}

async function call(method: "GET" | "POST", path: string, body?: unknown): Promise<Result> {
  try {
    return USE_TCP ? await tcpCall(method, path, body) : await udsCall(method, path, body);
  } catch (e: any) {
    const msg = e?.message || String(e);
    const isConnRefused =
      /ECONNREFUSED|ENOENT|Connection refused|fetch failed|Unable to connect|socket hang up/i.test(
        msg,
      );
    if (isConnRefused) {
      if (USE_TCP) {
        console.error(
          `Could not reach the Tamagotchi HTTP frontend at ${TCP_URL}.\n` +
            "Is it enabled? Try: tamagotchi server enable",
        );
      } else if (!existsSync(DAEMON_SOCKET) || /ENOENT/i.test(msg)) {
        console.error(
          "Tamagotchi daemon isn't running.\n" +
            "Run `tamagotchi setup` once to install it (auto-starts at login on macOS).",
        );
      } else {
        console.error(
          "Tamagotchi daemon socket exists but isn't responding.\n" +
            "Try: tamagotchi daemon status",
        );
      }
      process.exit(1);
    }
    throw e;
  }
}

export const health = () => call("GET", "/health");
export const face = (name: string) => call("POST", "/face", { name });
export const text = (s: string) => call("POST", "/text", { text: s });
export const mood = (name: string) => call("POST", "/mood", { name });
// `data` is a base64-encoded 128x64 1bpp bitmap (1024 bytes raw, MSB-first).
export const image = (data: string) => call("POST", "/image", { data });
export const state = () => call("GET", "/state");
export const fps = () => call("GET", "/fps");
export const faces = () => call("GET", "/faces");
export const ping = () => call("POST", "/ping");
export const stop = () => call("POST", "/stop");
export const start = () => call("POST", "/start");
