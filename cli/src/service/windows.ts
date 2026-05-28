// service/windows.ts — Windows Task Scheduler tasks for the daemon
// and HTTP frontend.
//
// We register two scheduled tasks ('Tamagotchi\Daemon', 'Tamagotchi\Http')
// that fire on the current user's logon and auto-restart on failure.
// That mirrors launchd's KeepAlive=true and systemd's Restart=always.
//
// Logs land in Task Scheduler's history (Task Scheduler > Show All
// Running Tasks > History) rather than ~/.tamagotchi/*.log: doing
// stdout redirection in Task Scheduler means wrapping every action in
// `cmd.exe /c "..."` and fighting CreateProcess quoting rules. The
// daemon itself logs to stdout, which Task Scheduler captures, so the
// information isn't lost.

import { spawnSync } from "node:child_process";
import { unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { DAEMON_SOCKET, ensureDaemonDir } from "../ipc";
import { SERVER_PORT } from "../server";

import { daemonHealth, httpUp, resolveExecutionPaths } from "./common";

const DAEMON_TASK = "Tamagotchi\\Daemon";
const HTTP_TASK = "Tamagotchi\\Http";

function xmlEscape(s: string): string {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function joinArgs(args: string[]): string {
  // <Arguments> is one command line that CreateProcess will tokenize.
  // Wrap args that contain whitespace in double-quotes; embedded "
  // gets escaped \" the way CommandLineToArgvW expects.
  return args.map((a) => /\s/.test(a) ? `"${a.replace(/"/g, '\\"')}"` : a).join(" ");
}

function principalUser(): string {
  // Task Scheduler wants DOMAIN\User or .\User. process.env.USERDOMAIN
  // is the local machine name on standalone boxes and the AD domain on
  // joined boxes — either is valid. Fall back to .\<user> just in case.
  const u = process.env.USERNAME;
  if (!u) return ".\\user";
  const dom = process.env.USERDOMAIN || ".";
  return `${dom}\\${u}`;
}

function taskXml(command: string, args: string[]): string {
  const cmd = xmlEscape(command);
  const argLine = xmlEscape(joinArgs(args));
  const user = xmlEscape(principalUser());
  return `<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.4" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Author>tamagotchi</Author>
    <Description>Tamagotchi background service.</Description>
  </RegistrationInfo>
  <Triggers>
    <LogonTrigger>
      <Enabled>true</Enabled>
      <UserId>${user}</UserId>
    </LogonTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author">
      <UserId>${user}</UserId>
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>LeastPrivilege</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <AllowHardTerminate>false</AllowHardTerminate>
    <StartWhenAvailable>true</StartWhenAvailable>
    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>
    <IdleSettings>
      <StopOnIdleEnd>false</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <AllowStartOnDemand>true</AllowStartOnDemand>
    <Enabled>true</Enabled>
    <Hidden>false</Hidden>
    <RunOnlyIfIdle>false</RunOnlyIfIdle>
    <DisallowStartOnRemoteAppSession>false</DisallowStartOnRemoteAppSession>
    <UseUnifiedSchedulingEngine>true</UseUnifiedSchedulingEngine>
    <WakeToRun>false</WakeToRun>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
    <RestartOnFailure>
      <Interval>PT5S</Interval>
      <Count>10000</Count>
    </RestartOnFailure>
  </Settings>
  <Actions Context="Author">
    <Exec>
      <Command>${cmd}</Command>
      <Arguments>${argLine}</Arguments>
    </Exec>
  </Actions>
</Task>
`;
}

function writeTaskXmlFile(args: string[]): string {
  const xmlPath = join(tmpdir(), `tamagotchi-task-${Date.now()}-${Math.floor(Math.random() * 1e6)}.xml`);
  const [command, ...rest] = args;
  // schtasks.exe insists on UTF-16LE with BOM for /XML.
  const body = "﻿" + taskXml(command, rest);
  writeFileSync(xmlPath, Buffer.from(body, "utf16le"));
  return xmlPath;
}

function schtasks(args: string[], opts: { check?: boolean } = {}): { stdout: string; stderr: string; status: number } {
  const r = spawnSync("schtasks.exe", args, { encoding: "utf8" });
  if (opts.check && r.status !== 0) {
    console.error(`schtasks ${args.join(" ")} failed:\n${r.stderr || r.stdout}`);
    process.exit(1);
  }
  return { stdout: r.stdout || "", stderr: r.stderr || "", status: r.status ?? -1 };
}

function installTask(taskName: string, args: string[]): void {
  const xmlPath = writeTaskXmlFile(args);
  try {
    schtasks(["/Create", "/TN", taskName, "/XML", xmlPath, "/F"], { check: true });
    // Kick it off now so the user doesn't have to sign out and back in.
    schtasks(["/Run", "/TN", taskName]);
  } finally {
    try { unlinkSync(xmlPath); } catch {}
  }
}

function removeTask(taskName: string): void {
  schtasks(["/End", "/TN", taskName]);
  schtasks(["/Delete", "/TN", taskName, "/F"]);
}

function taskExists(taskName: string): boolean {
  return schtasks(["/Query", "/TN", taskName]).status === 0;
}

function taskRunning(taskName: string): boolean {
  // CSV: "TaskName","Next Run Time","Status"
  const r = schtasks(["/Query", "/TN", taskName, "/FO", "CSV", "/NH"]);
  if (r.status !== 0) return false;
  const line = (r.stdout || "").trim().split(/\r?\n/).pop() || "";
  return /"Running"\s*$/i.test(line);
}

export function setup(): void {
  ensureDaemonDir();
  const paths = resolveExecutionPaths();
  installTask(DAEMON_TASK, [paths.nodePath, paths.tsxPath, paths.cliEntry, "daemon", "run"]);
  installTask(HTTP_TASK, [paths.nodePath, paths.tsxPath, paths.cliEntry, "server", "run"]);
  console.log("Tamagotchi installed and running:");
  console.log(`  daemon task:        ${DAEMON_TASK}`);
  console.log(`  HTTP frontend task: ${HTTP_TASK}  (http://localhost:${SERVER_PORT})`);
  console.log("");
  console.log("Both auto-start at sign-in. To turn off HTTP:");
  console.log("  gochi server disable");
}

export function enableHttp(): void {
  ensureDaemonDir();
  if (!taskExists(DAEMON_TASK)) {
    console.error("daemon isn't installed yet. Run `gochi setup` first.");
    process.exit(1);
  }
  const paths = resolveExecutionPaths();
  installTask(HTTP_TASK, [paths.nodePath, paths.tsxPath, paths.cliEntry, "server", "run"]);
  console.log(`HTTP frontend enabled at http://localhost:${SERVER_PORT}.`);
}

export function disableHttp(): void {
  if (!taskExists(HTTP_TASK)) {
    console.log("HTTP frontend is already disabled.");
    return;
  }
  removeTask(HTTP_TASK);
  console.log("HTTP frontend disabled.");
}

export async function daemonStatus(): Promise<void> {
  const installed = taskExists(DAEMON_TASK);
  const running = installed && taskRunning(DAEMON_TASK);
  // existsSync isn't reliable for named pipes on Windows; lean on
  // socketReachable from the actual request instead.
  const { reachable, json } = await daemonHealth();

  console.log(JSON.stringify({
    installed,
    task: installed ? DAEMON_TASK : null,
    scheduledTask: running ? "running" : "not running",
    socket: DAEMON_SOCKET,
    socketReachable: reachable,
    health: json,
  }, null, 2));
}

export async function serverStatus(): Promise<void> {
  const installed = taskExists(HTTP_TASK);
  const running = installed && taskRunning(HTTP_TASK);
  const up = await httpUp(SERVER_PORT);

  console.log(JSON.stringify({
    enabled: installed,
    task: installed ? HTTP_TASK : null,
    scheduledTask: running ? "running" : "not running",
    http: up ? "up" : "down",
    port: SERVER_PORT,
  }, null, 2));
}
