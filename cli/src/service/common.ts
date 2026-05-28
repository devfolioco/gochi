// service/common.ts — helpers shared by every platform backend.
//
// The interesting bit is `resolveExecutionPaths()`: every backend has
// to bake an absolute, scheduler-friendly command line into a plist /
// unit / task. We derive it from `process.execPath` (the running Node)
// and `import.meta.url` (this file's URL) instead of shelling out to
// `which`, so it works identically on macOS, Linux, and Windows.

import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";

import { daemonRequest } from "../ipc";

export type Paths = {
  nodePath: string;   // absolute path to node
  tsxPath: string;    // absolute path to tsx's CLI entry
  cliEntry: string;   // absolute path to src/cli.ts
};

export function resolveExecutionPaths(): Paths {
  const nodePath = process.execPath;
  // common.ts lives at src/service/common.ts; cli.ts is at src/cli.ts.
  const cliEntry = fileURLToPath(new URL("../cli.ts", import.meta.url));
  const require = createRequire(import.meta.url);
  let tsxPath: string;
  try {
    tsxPath = require.resolve("tsx/cli");
  } catch {
    console.error(
      "Could not resolve `tsx/cli`. Reinstall the CLI (npm i -g gochi).",
    );
    process.exit(1);
  }
  return { nodePath, tsxPath, cliEntry };
}

// Ask the daemon for /health over its UDS (or named pipe on Windows).
// Returns reachable=false if the daemon isn't running.
export async function daemonHealth(): Promise<{ reachable: boolean; json: unknown }> {
  try {
    const r = await daemonRequest("GET", "/health");
    return { reachable: r.ok, json: r.json };
  } catch {
    return { reachable: false, json: null };
  }
}

// Hit the HTTP frontend's /health on TCP. Used by `server status`.
export async function httpUp(port: number): Promise<boolean> {
  try {
    const res = await fetch(`http://localhost:${port}/health`, {
      signal: AbortSignal.timeout(500),
    });
    return res.ok;
  } catch {
    return false;
  }
}
