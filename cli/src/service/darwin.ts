// service/darwin.ts — macOS launchd lifecycle for the daemon and HTTP frontend.
//
// Two LaunchAgents, each with its own plist:
//
//   com.tamagotchi.daemon  — long-lived port owner; serves the UDS.
//                            Installed by `gochi setup`. Always on.
//   com.tamagotchi.http    — TCP :7474 reverse-proxy to the daemon.
//                            Installed by `setup` (ON by default) and by
//                            `server enable`; removed by `server disable`.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, unlinkSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";

import {
  DAEMON_LOG_ERR,
  DAEMON_LOG_OUT,
  DAEMON_SOCKET,
  HTTP_LOG_ERR,
  HTTP_LOG_OUT,
  ensureDaemonDir,
} from "../ipc";
import { SERVER_PORT } from "../server";

import { type Paths, daemonHealth, httpUp, resolveExecutionPaths } from "./common";

const DAEMON_LABEL = "com.tamagotchi.daemon";
const HTTP_LABEL = "com.tamagotchi.http";
const LEGACY_LABEL = "com.tamagotchi.server"; // pre-split, daemon+HTTP-in-one
const PLIST_DIR = join(homedir(), "Library", "LaunchAgents");
const DAEMON_PLIST = join(PLIST_DIR, `${DAEMON_LABEL}.plist`);
const HTTP_PLIST = join(PLIST_DIR, `${HTTP_LABEL}.plist`);
const LEGACY_PLIST = join(PLIST_DIR, `${LEGACY_LABEL}.plist`);

function uid(): number {
  const f = (process as unknown as { getuid?: () => number }).getuid;
  return f ? f.call(process) : 0;
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
  console.log("  gochi server disable");
}

export function enableHttp(): void {
  ensureDaemonDir();
  if (!existsSync(DAEMON_PLIST)) {
    console.error("daemon isn't installed yet. Run `gochi setup` first.");
    process.exit(1);
  }
  const paths = resolveExecutionPaths();
  installHttp(paths);
  console.log(`HTTP frontend enabled at http://localhost:${SERVER_PORT}.`);
}

export function disableHttp(): void {
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
  const up = await httpUp(SERVER_PORT);

  console.log(
    JSON.stringify(
      {
        enabled: httpInstalled,
        plist: httpInstalled ? HTTP_PLIST : null,
        launchd: launchdLoaded ? "loaded" : "not loaded",
        http: up ? "up" : "down",
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
  const { reachable, json } = await daemonHealth();

  console.log(
    JSON.stringify(
      {
        installed,
        plist: installed ? DAEMON_PLIST : null,
        launchd: launchdLoaded ? "loaded" : "not loaded",
        socket: DAEMON_SOCKET,
        socketPresent,
        socketReachable: reachable,
        health: json,
      },
      null,
      2,
    ),
  );
}
