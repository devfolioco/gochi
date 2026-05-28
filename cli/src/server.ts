// server.ts — opt-in TCP HTTP frontend for the daemon.
//
// This is a thin reverse-proxy: it listens on :7474 and forwards every
// request to the daemon's Unix domain socket. The daemon is the single
// owner of the serial port; this process just opens the network door.
// Enabled / disabled via `gochi server enable|disable`.

import { createServer, request as httpRequest } from "node:http";
import type { IncomingMessage, ServerResponse } from "node:http";

import { DAEMON_SOCKET } from "./ipc";

export const SERVER_PORT = Number(process.env.GOCHI_PORT) || 7474;
export const SERVER_VERSION = "0.1.0";

function log(...args: unknown[]): void {
  const ts = new Date().toISOString();
  console.log(`[${ts}]`, ...args);
}

function sendJson(res: ServerResponse, data: unknown, status = 200): void {
  res.statusCode = status;
  res.setHeader("Content-Type", "application/json");
  res.end(JSON.stringify(data, null, 2) + "\n");
}

export async function runServer(): Promise<void> {
  const proxy = createServer((req: IncomingMessage, res: ServerResponse) => {
    const upstream = httpRequest(
      {
        socketPath: DAEMON_SOCKET,
        method: req.method,
        path: req.url,
        headers: req.headers,
        timeout: 5000,
      },
      (uRes) => {
        res.statusCode = uRes.statusCode || 502;
        for (const [k, v] of Object.entries(uRes.headers)) {
          if (v !== undefined) res.setHeader(k, v as any);
        }
        uRes.pipe(res);
      },
    );
    upstream.on("error", (e: any) => {
      log("daemon unreachable:", e?.message || e);
      sendJson(
        res,
        {
          ok: false,
          connected: false,
          message:
            "daemon not reachable. Run `gochi setup` (one-time) if you haven't.",
        },
        502,
      );
    });
    upstream.on("timeout", () => {
      upstream.destroy();
      sendJson(res, { ok: false, message: "daemon timed out" }, 504);
    });
    req.pipe(upstream);
  });

  proxy.listen(SERVER_PORT, () => {
    log(`Tamagotchi HTTP frontend on http://localhost:${SERVER_PORT} → ${DAEMON_SOCKET}`);
  });
}
