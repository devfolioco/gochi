// cli.ts — CLI entry point (invoked by ../bin/gochi.js via tsx).
//
// User-facing surface:
//   gochi setup                  one-time install (daemon + HTTP frontend)
//   gochi face|mood|text|image   drive the pet (talks to daemon via UDS)
//   gochi ping|health            liveness / status
//   gochi get|list               queries
//   gochi server enable          turn HTTP frontend on (default after setup)
//   gochi server disable         turn HTTP frontend off
//   gochi server status          HTTP frontend state
//   gochi daemon status          daemon state
//
// Internal (invoked by launchd plists / systemd units / Task Scheduler):
//   gochi daemon run             foreground daemon process
//   gochi server run             foreground HTTP frontend process

import { select } from "@inquirer/prompts";
import { Command } from "commander";

import * as client from "./client";
import { runDaemon } from "./daemon";
import { fileToFrameBase64 } from "./image";
import { runServer } from "./server";
import * as service from "./service";
import { runTest } from "./test";

const VERSION = "0.1.0";

// Kept in sync with the firmware's expression / mood registries.
const FACES = [
  "neutral", "happy", "sad", "sleepy", "excited", "surprised",
  "angry",   "blink", "love", "horny",  "shy",     "dead",
];
const MOODS = ["content", "playful", "grumpy", "sleepy", "affectionate"];

const program = new Command();
program
  .name("gochi")
  .description("Drive the Tamagotchi desk pet over USB serial.")
  .version(VERSION, "-v, --version");

// --- One-time setup ----------------------------------------------------

program
  .command("setup")
  .description("install the daemon (and HTTP frontend) so the pet is always reachable")
  .action(() => service.setup());

// stop / start release and reacquire the serial port. The daemon keeps
// running; only the device link is dropped. Used before `make flash` and
// any other tool that needs exclusive access to /dev/cu.usbmodem*.
program
  .command("stop")
  .description("release the serial port (e.g. before flashing firmware)")
  .action(async () => print(await client.stop()));

program
  .command("start")
  .description("reacquire the serial port after a `stop`")
  .action(async () => print(await client.start()));

// --- Pet commands ------------------------------------------------------

program
  .command("face")
  .description("show a face expression (omit name to pick from a menu)")
  .argument("[name]", "face name (e.g. happy)")
  .action(async (name?: string) => {
    const chosen = name ?? (await pickFrom("Pick a face:", FACES));
    print(await client.face(chosen));
  });

program
  .command("mood")
  .description("set the pet's mood (omit name to pick from a menu)")
  .argument("[name]", "mood name (e.g. playful)")
  .action(async (name?: string) => {
    const chosen = name ?? (await pickFrom("Pick a mood:", MOODS));
    print(await client.mood(chosen));
  });

program
  .command("text")
  .description("show a line of text (long text scrolls)")
  .argument("<text...>", "the text to show")
  .action(async (parts: string[]) => print(await client.text(parts.join(" "))));

program
  .command("image")
  .description("show a PNG/JPG on the OLED (auto-resized to 128x64, 1-bit)")
  .argument("<path>", "path to a PNG or JPG file")
  .option("--no-dither", "use a plain brightness threshold instead of Floyd–Steinberg")
  .option("-t, --threshold <n>", "threshold cutoff 0..255 (only with --no-dither)", "128")
  .option("--invert", "swap on/off pixels")
  .option("--bg <color>", "letterbox fill: black|white", "black")
  .action(async (path: string, opts: { dither: boolean; threshold: string; invert?: boolean; bg: string }) => {
    const t = Number(opts.threshold);
    if (!Number.isFinite(t) || t < 0 || t > 255) {
      console.error("threshold must be a number 0..255");
      process.exit(1);
    }
    if (opts.bg !== "black" && opts.bg !== "white") {
      console.error("bg must be 'black' or 'white'");
      process.exit(1);
    }
    let base64: string;
    try {
      base64 = await fileToFrameBase64(path, {
        dither: opts.dither,
        threshold: t,
        invert: opts.invert,
        background: opts.bg,
      });
    } catch (e: any) {
      console.error(`could not read or decode "${path}": ${e?.message || e}`);
      process.exit(1);
    }
    print(await client.image(base64));
  });

// --- Queries -----------------------------------------------------------

const get = program.command("get");
get.command("state").description("current view + expression")
  .action(async () => print(await client.state()));
get.command("fps").description("current display frame rate")
  .action(async () => print(await client.fps()));

const list = program.command("list");
list.command("faces").description("list all expression names")
  .action(async () => print(await client.faces()));

program.command("ping").description("liveness check (PONG)")
  .action(async () => print(await client.ping()));

// --- Self-test ---------------------------------------------------------

program
  .command("test")
  .description("interactive hardware self-test (asks y/n per component)")
  .argument("[component]", "serial | oled | buzzer | all (omit to pick from a menu)")
  .action(async (component?: string) => {
    await runTest(component);
  });

program.command("health").description("daemon + device status")
  .action(async () => print(await client.health()));

// --- Daemon management -------------------------------------------------

const daemon = program.command("daemon").description("manage the long-lived port-owning daemon");
daemon.command("run")
  .description("run the daemon in the foreground (used by launchd)")
  .action(() => runDaemon());
daemon.command("status")
  .description("show daemon launchd + socket status")
  .action(() => service.daemonStatus());

// --- HTTP frontend management -----------------------------------------

const server = program.command("server").description("manage the optional HTTP frontend");
server.command("enable")
  .description("turn on the TCP HTTP listener (auto-started at login)")
  .action(() => service.enableHttp());
server.command("disable")
  .description("turn off the TCP HTTP listener")
  .action(() => service.disableHttp());
server.command("status")
  .description("show HTTP frontend status")
  .action(() => service.serverStatus());
server.command("run")
  .description("run the HTTP frontend in the foreground (used by launchd)")
  .action(() => runServer());

program.parseAsync(process.argv).catch((e) => {
  // Inquirer throws this on Ctrl-C — exit quietly instead of stack-tracing.
  if (e?.name === "ExitPromptError") {
    process.exit(130);
  }
  console.error(e?.stack || e);
  process.exit(1);
});

// --- Helpers -----------------------------------------------------------

async function pickFrom(message: string, options: string[]): Promise<string> {
  return await select({
    message,
    choices: options.map((value) => ({ name: value, value })),
  });
}

function print(result: unknown): void {
  if (typeof result === "string") console.log(result);
  else console.log(JSON.stringify(result, null, 2));
}
