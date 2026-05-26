// service.ts — macOS launchd lifecycle for the daemon and HTTP frontend.
//
// Two LaunchAgents, each with its own plist:
//
//   com.tamagotchi.daemon  — long-lived port owner; serves the UDS.
//                            Installed by `tamagotchi setup`. Always on.
//   com.tamagotchi.http    — TCP :7474 reverse-proxy to the daemon.
//                            Installed by `setup` (ON by default) and by
//                            `server enable`; removed by `server disable`.
//
// On non-macOS we can't write LaunchAgents. The CLI still works in
// foreground mode: run `tamagotchi daemon run` (and optionally
// `tamagotchi server run`) in a terminal yourself.

import { execSync, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, realpathSync, unlinkSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";

import { SERVER_PORT } from "./server";
import {
  DAEMON_LOG_ERR,
  DAEMON_LOG_OUT,
  DAEMON_SOCKET,
  HTTP_LOG_ERR,
  HTTP_LOG_OUT,
  daemonRequest,
  ensureDaemonDir,
} from "./ipc";

const DAEMON_LABEL = "com.tamagotchi.daemon";
const HTTP_LABEL = "com.tamagotchi.http";
const LEGACY_LABEL = "com.tamagotchi.server"; // pre-split, daemon+HTTP-in-one
const PLIST_DIR = join(homedir(), "Library", "LaunchAgents");
const DAEMON_PLIST = join(PLIST_DIR, `${DAEMON_LABEL}.plist`);
const HTTP_PLIST = join(PLIST_DIR, `${HTTP_LABEL}.plist`);
const LEGACY_PLIST = join(PLIST_DIR, `${LEGACY_LABEL}.plist`);

function whichBin(name: string): string | null {
  try {
    const p = execSync(`command -v ${name}`, { encoding: "utf8", shell: "/bin/sh" }).trim();
    return p || null;
  } catch {
    return null;
  }
}

function uid(): number {
  const f = (process as unknown as { getuid?: () => number }).getuid;
  return f ? f.call(process) : 0;
}

function requireMac(action: string): void {
  if (process.platform !== "darwin") {
    console.error(
      `${action}: only macOS (launchd) is supported right now.\n` +
        "On Linux/Windows, run `tamagotchi daemon run` (and optionally `tamagotchi server run`) in a terminal.",
    );
    process.exit(1);
  }
}

// We need three absolute paths every plist needs: node, tsx's CLI, and
// the cli.ts entry. Resolved once from the global `tamagotchi` symlink.
type Paths = { nodePath: string; tsxPath: string; cliEntry: string };

function resolveExecutionPaths(): Paths {
  const tamagotchiSymlink = whichBin("tamagotchi");
  const nodePath = whichBin("node");
  if (!tamagotchiSymlink) {
    console.error(
      "`tamagotchi` is not on your PATH. Install the CLI globally first (`npm i -g @0xpv/tamagotchi`).",
    );
    process.exit(1);
  }
  if (!nodePath) {
    console.error("`node` is not on your PATH. Install Node 18+ first.");
    process.exit(1);
  }
  // The global bin is a symlink to ./bin/tamagotchi.js; walk it back to
  // cli/ so we can find tsx and src/cli.ts. We resolve tsx via Node's
  // module resolution because npm hoisting can move it around.
  const wrapperPath = realpathSync(tamagotchiSymlink);
  const cliRoot = join(wrapperPath, "..", "..");
  let tsxPath: string;
  try {
    // dynamic require so this file stays importable for the build path
    const { createRequire } = require("node:module") as typeof import("node:module");
    const req = createRequire(join(cliRoot, "package.json"));
    tsxPath = req.resolve("tsx/cli");
  } catch {
    // Fallback: best-effort path inside the package's own node_modules.
    tsxPath = join(cliRoot, "node_modules", "tsx", "dist", "cli.mjs");
  }
  const cliEntry = join(cliRoot, "src", "cli.ts");
  return { nodePath, tsxPath, cliEntry };
}

function plistFor(label: string, args: string[], stdout: string, stderr: string): string {
  const argv = args
    .map((a) => `    <string>${a.replace(/&/g, "&amp;").replace(/</g, "&lt;")}</string>`)
    .join("\n");
  return `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>${label}</string>
  <key>ProgramArguments</key>
  <array>
${argv}
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>ProcessType</key><string>Background</string>
  <key>StandardOutPath</key><string>${stdout}</string>
  <key>StandardErrorPath</key><string>${stderr}</string>
</dict>
</plist>
`;
}

function writePlist(label: string, plistPath: string, args: string[], out: string, err: string): void {
  mkdirSync(PLIST_DIR, { recursive: true });
  writeFileSync(plistPath, plistFor(label, args, out, err));
}

function bootstrap(label: string, plistPath: string): void {
  // bootout the old version (modern + legacy), then bootstrap. If
  // bootstrap fails, fall back to legacy `load`.
  spawnSync("launchctl", ["bootout", `gui/${uid()}/${label}`], { stdio: "ignore" });
  spawnSync("launchctl", ["unload", plistPath], { stdio: "ignore" });
  const r = spawnSync("launchctl", ["bootstrap", `gui/${uid()}`, plistPath], { encoding: "utf8" });
  if (r.status !== 0) {
    const r2 = spawnSync("launchctl", ["load", plistPath], { encoding: "utf8" });
    if (r2.status !== 0) {
      console.error("launchctl failed:", r.stderr || r2.stderr);
      process.exit(1);
    }
  }
}

function teardown(label: string, plistPath: string): void {
  spawnSync("launchctl", ["bootout", `gui/${uid()}/${label}`], { stdio: "ignore" });
  spawnSync("launchctl", ["unload", plistPath], { stdio: "ignore" });
  if (existsSync(plistPath)) {
    try {
      unlinkSync(plistPath);
    } catch {}
  }
}

function installDaemon(paths: Paths): void {
  writePlist(
    DAEMON_LABEL,
    DAEMON_PLIST,
    [paths.nodePath, paths.tsxPath, paths.cliEntry, "daemon", "run"],
    DAEMON_LOG_OUT,
    DAEMON_LOG_ERR,
  );
  bootstrap(DAEMON_LABEL, DAEMON_PLIST);
}

function installHttp(paths: Paths): void {
  writePlist(
    HTTP_LABEL,
    HTTP_PLIST,
    [paths.nodePath, paths.tsxPath, paths.cliEntry, "server", "run"],
    HTTP_LOG_OUT,
    HTTP_LOG_ERR,
  );
  bootstrap(HTTP_LABEL, HTTP_PLIST);
}

// ---------------------------------------------------------------------
// Public API used by cli.ts
// ---------------------------------------------------------------------

// Tear down the pre-split LaunchAgent if it's still resident. The old
// process held :7474 and the serial port itself, which would block the
// new HTTP frontend from binding and contend with the new daemon for the
// device.
function removeLegacy(): void {
  if (!existsSync(LEGACY_PLIST)) return;
  console.log(`migrating: removing legacy ${LEGACY_LABEL} agent…`);
  teardown(LEGACY_LABEL, LEGACY_PLIST);
}

export function setup(): void {
  requireMac("setup");
  ensureDaemonDir();
  removeLegacy();
  const paths = resolveExecutionPaths();
  installDaemon(paths);
  installHttp(paths);
  console.log("Tamagotchi installed and running:");
  console.log(`  daemon:        ${DAEMON_PLIST}`);
  console.log(`  HTTP frontend: ${HTTP_PLIST}  (http://localhost:${SERVER_PORT})`);
  console.log("");
  console.log("Both start automatically at login. To turn off HTTP:");
  console.log("  tamagotchi server disable");
}

export function enableHttp(): void {
  requireMac("server enable");
  ensureDaemonDir();
  if (!existsSync(DAEMON_PLIST)) {
    console.error("daemon isn't installed yet. Run `tamagotchi setup` first.");
    process.exit(1);
  }
  const paths = resolveExecutionPaths();
  installHttp(paths);
  console.log(`HTTP frontend enabled at http://localhost:${SERVER_PORT}.`);
}

export function disableHttp(): void {
  requireMac("server disable");
  if (!existsSync(HTTP_PLIST)) {
    console.log("HTTP frontend is already disabled.");
    return;
  }
  teardown(HTTP_LABEL, HTTP_PLIST);
  console.log("HTTP frontend disabled.");
}

export async function serverStatus(): Promise<void> {
  const httpInstalled = existsSync(HTTP_PLIST);
  const list = spawnSync("launchctl", ["list", HTTP_LABEL], { encoding: "utf8" });
  const launchdLoaded = list.status === 0;

  let httpUp = false;
  try {
    const res = await fetch(`http://localhost:${SERVER_PORT}/health`, {
      signal: AbortSignal.timeout(500),
    });
    if (res.ok) httpUp = true;
  } catch {
    // not up
  }

  console.log(
    JSON.stringify(
      {
        enabled: httpInstalled,
        plist: httpInstalled ? HTTP_PLIST : null,
        launchd: launchdLoaded ? "loaded" : "not loaded",
        http: httpUp ? "up" : "down",
        port: SERVER_PORT,
      },
      null,
      2,
    ),
  );
}

export async function daemonStatus(): Promise<void> {
  const installed = existsSync(DAEMON_PLIST);
  const list = spawnSync("launchctl", ["list", DAEMON_LABEL], { encoding: "utf8" });
  const launchdLoaded = list.status === 0;
  const socketPresent = existsSync(DAEMON_SOCKET);

  let socketReachable = false;
  let health: unknown = null;
  try {
    const r = await daemonRequest("GET", "/health");
    if (r.ok) {
      socketReachable = true;
      health = r.json;
    }
  } catch {
    // unreachable
  }

  console.log(
    JSON.stringify(
      {
        installed,
        plist: installed ? DAEMON_PLIST : null,
        launchd: launchdLoaded ? "loaded" : "not loaded",
        socket: DAEMON_SOCKET,
        socketPresent,
        socketReachable,
        health,
      },
      null,
      2,
    ),
  );
}
