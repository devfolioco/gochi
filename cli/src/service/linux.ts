// service/linux.ts — systemd user units for the daemon and HTTP frontend.
//
// We drop two .service files into ~/.config/systemd/user/ and drive
// them with `systemctl --user`. Restart=always mirrors launchd's
// KeepAlive=true.
//
// Logs land in the user journal: `journalctl --user -u tamagotchi-daemon`.
// We don't try to redirect to ~/.tamagotchi/*.log because journald is
// the idiomatic Linux choice and most distros already rotate it.
//
// Caveat: by default systemd's per-user manager stops when the user
// logs out, which would kill the daemon. `loginctl enable-linger`
// keeps it alive across logout, but that requires sudo, so we surface
// a hint after setup rather than running it ourselves.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, unlinkSync, writeFileSync } from "node:fs";
import { homedir, userInfo } from "node:os";
import { join } from "node:path";

import { DAEMON_SOCKET, ensureDaemonDir } from "../ipc";
import { SERVER_PORT } from "../server";

import { daemonHealth, httpUp, resolveExecutionPaths } from "./common";

const SYSTEMD_DIR = join(homedir(), ".config", "systemd", "user");
const DAEMON_UNIT = "tamagotchi-daemon.service";
const HTTP_UNIT = "tamagotchi-http.service";
const DAEMON_UNIT_PATH = join(SYSTEMD_DIR, DAEMON_UNIT);
const HTTP_UNIT_PATH = join(SYSTEMD_DIR, HTTP_UNIT);

// systemd's ExecStart line uses POSIX-ish quoting: a single double-quoted
// token survives spaces, and \" / \\ escape inside it.
function shellQuote(s: string): string {
  return `"${s.replace(/\\/g, "\\\\").replace(/"/g, '\\"')}"`;
}

function unitFor(description: string, args: string[]): string {
  const cmd = args.map(shellQuote).join(" ");
  return `[Unit]
Description=${description}
After=default.target

[Service]
Type=simple
ExecStart=${cmd}
Restart=always
RestartSec=2

[Install]
WantedBy=default.target
`;
}

function systemctl(args: string[], opts: { check?: boolean } = {}): { stdout: string; stderr: string; status: number } {
  const r = spawnSync("systemctl", ["--user", ...args], { encoding: "utf8" });
  if (opts.check && r.status !== 0) {
    console.error(`systemctl --user ${args.join(" ")} failed:\n${r.stderr || r.stdout}`);
    process.exit(1);
  }
  return { stdout: r.stdout || "", stderr: r.stderr || "", status: r.status ?? -1 };
}

function ensureSystemdAvailable(): void {
  // Minimal containers (docker without `--init` + a session bus, WSL1,
  // ancient distros) don't have a per-user systemd. We catch that early
  // with a useful message instead of letting `enable` cryptically fail.
  const r = spawnSync("systemctl", ["--user", "show", "--property=Version"], { encoding: "utf8", timeout: 2000 });
  if (r.status !== 0 && /Failed to connect|No such file/i.test(r.stderr || "")) {
    console.error(
      "systemd --user is not available on this system.\n" +
      "Run `gochi daemon run` (and optionally `gochi server run`) in a terminal to use the CLI manually.",
    );
    process.exit(1);
  }
}

function installUnit(unit: string, unitPath: string, body: string): void {
  mkdirSync(SYSTEMD_DIR, { recursive: true });
  writeFileSync(unitPath, body);
  systemctl(["daemon-reload"], { check: true });
  systemctl(["enable", "--now", unit], { check: true });
}

function removeUnit(unit: string, unitPath: string): void {
  spawnSync("systemctl", ["--user", "disable", "--now", unit], { stdio: "ignore" });
  if (existsSync(unitPath)) {
    try { unlinkSync(unitPath); } catch {}
  }
  systemctl(["daemon-reload"]);
}

function unitActive(unit: string): boolean {
  return spawnSync("systemctl", ["--user", "is-active", "--quiet", unit]).status === 0;
}

function lingerHint(): void {
  // Skip the hint when lingering is already on. Best-effort — `loginctl`
  // missing is fine, just don't say anything.
  try {
    const user = userInfo().username;
    const r = spawnSync("loginctl", ["show-user", user, "--property=Linger"], { encoding: "utf8" });
    if (!r.stdout.includes("Linger=yes")) {
      console.log("");
      console.log("Tip: services stop when you log out. Enable lingering so they survive reboots / SSH disconnects:");
      console.log(`  sudo loginctl enable-linger ${user}`);
    }
  } catch {
    // no loginctl
  }
}

export function setup(): void {
  ensureSystemdAvailable();
  ensureDaemonDir();
  const paths = resolveExecutionPaths();
  installUnit(
    DAEMON_UNIT,
    DAEMON_UNIT_PATH,
    unitFor(
      "Tamagotchi daemon (owns USB serial)",
      [paths.nodePath, paths.tsxPath, paths.cliEntry, "daemon", "run"],
    ),
  );
  installUnit(
    HTTP_UNIT,
    HTTP_UNIT_PATH,
    unitFor(
      "Tamagotchi HTTP frontend",
      [paths.nodePath, paths.tsxPath, paths.cliEntry, "server", "run"],
    ),
  );
  console.log("Tamagotchi installed and running:");
  console.log(`  daemon:        ${DAEMON_UNIT_PATH}`);
  console.log(`  HTTP frontend: ${HTTP_UNIT_PATH}  (http://localhost:${SERVER_PORT})`);
  console.log("");
  console.log("Logs:  journalctl --user -u tamagotchi-daemon -f");
  console.log("Both start automatically at login. To turn off HTTP:");
  console.log("  gochi server disable");
  lingerHint();
}

export function enableHttp(): void {
  ensureSystemdAvailable();
  ensureDaemonDir();
  if (!existsSync(DAEMON_UNIT_PATH)) {
    console.error("daemon isn't installed yet. Run `gochi setup` first.");
    process.exit(1);
  }
  const paths = resolveExecutionPaths();
  installUnit(
    HTTP_UNIT,
    HTTP_UNIT_PATH,
    unitFor(
      "Tamagotchi HTTP frontend",
      [paths.nodePath, paths.tsxPath, paths.cliEntry, "server", "run"],
    ),
  );
  console.log(`HTTP frontend enabled at http://localhost:${SERVER_PORT}.`);
}

export function disableHttp(): void {
  if (!existsSync(HTTP_UNIT_PATH)) {
    console.log("HTTP frontend is already disabled.");
    return;
  }
  removeUnit(HTTP_UNIT, HTTP_UNIT_PATH);
  console.log("HTTP frontend disabled.");
}

export async function daemonStatus(): Promise<void> {
  const installed = existsSync(DAEMON_UNIT_PATH);
  const active = installed && unitActive(DAEMON_UNIT);
  const socketPresent = existsSync(DAEMON_SOCKET);
  const { reachable, json } = await daemonHealth();

  console.log(JSON.stringify({
    installed,
    unit: installed ? DAEMON_UNIT_PATH : null,
    service: active ? "active" : "inactive",
    socket: DAEMON_SOCKET,
    socketPresent,
    socketReachable: reachable,
    health: json,
  }, null, 2));
}

export async function serverStatus(): Promise<void> {
  const installed = existsSync(HTTP_UNIT_PATH);
  const active = installed && unitActive(HTTP_UNIT);
  const up = await httpUp(SERVER_PORT);

  console.log(JSON.stringify({
    enabled: installed,
    unit: installed ? HTTP_UNIT_PATH : null,
    service: active ? "active" : "inactive",
    http: up ? "up" : "down",
    port: SERVER_PORT,
  }, null, 2));
}
