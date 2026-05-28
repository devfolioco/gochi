// test.ts — interactive hardware self-test driven from the CLI.
//
// `gochi test` (or `gochi test <component>`) walks one of the host-
// observable components through a quick check, asks a y/n question, and
// drops a tailored troubleshooting list on 'no'. Everything talks to the
// firmware over the existing serial protocol via the daemon — no new
// firmware commands needed.
//
// Buttons are intentionally not covered here: the firmware has no
// host-readable button state, so a CLI-side test can't observe a press.
// (Flash a dedicated on-device sketch for that.)
//
// What gets tested:
//   serial  — `PING` round-trip via the daemon (proves the host can talk
//             to the board at all)
//   oled    — `SHOW text "Hello"`, then ask if the user sees it
//   buzzer  — two `SHOW face` calls back-to-back to guarantee a face
//             transition (the firmware only jingles on a *change*), then
//             ask if the user heard the tone

import { select } from "@inquirer/prompts";

import * as client from "./client";
import type { Result } from "./client";

type Component = "serial" | "oled" | "buzzer";
const ORDER: readonly Component[] = ["serial", "oled", "buzzer"] as const;

const TIPS: Record<Component, string[]> = {
  serial: [
    "Is the board plugged in? `ls /dev/cu.usbmodem*` should list it.",
    "Is the daemon running? `gochi daemon status` — if not, run `gochi setup`.",
    "Did a recent `make flash` leave the port released? Try `gochi start`.",
    "Hit the RESET button on the board and try again.",
  ],
  oled: [
    "Check SDA wired to GPIO5 and SCL wired to GPIO6.",
    "Check VCC -> 3V3 and GND -> GND on the OLED module.",
    "Confirm the panel is SSD1306 128x64 at I2C address 0x3C.",
    "Reseat the I2C jumpers — flaky dupont wires are the #1 cause.",
    "Reflash the firmware (`make flash`) — if the OLED never inits at boot, no text command will rescue it.",
  ],
  buzzer: [
    "Check the buzzer signal pin is on GPIO10 and the other pin is on GND.",
    "Make sure it is a *passive* piezo — active buzzers ignore tone signals.",
    "Try swapping the two buzzer leads (some modules are polarity-sensitive).",
    "In a quiet room the tone is faint; hold the buzzer close to your ear.",
    "The firmware only jingles on a face *change* in Desktop Mode — the test forces a transition, so a silent buzzer here means hardware.",
  ],
};

// Public entry — called from cli.ts. `which` undefined → menu; "all" →
// run every test in order.
export async function runTest(which?: string): Promise<void> {
  const target = which ?? (await pickComponent());

  if (target === "all") {
    let serialOk = await runOne("serial");
    if (!serialOk) {
      console.log("\nSkipping OLED + buzzer tests — without a serial link they can't run.");
      summary(1, 0);
      process.exit(1);
    }
    const oledOk = await runOne("oled");
    const buzzerOk = await runOne("buzzer");
    const fails = [serialOk, oledOk, buzzerOk].filter((x) => !x).length;
    const passes = 3 - fails;
    summary(fails, passes);
    process.exit(fails === 0 ? 0 : 1);
  }

  if (!isComponent(target)) {
    console.error(`Unknown component "${target}". Valid: ${ORDER.join(", ")}, all.`);
    process.exit(2);
  }

  const ok = await runOne(target);
  process.exit(ok ? 0 : 1);
}

async function pickComponent(): Promise<Component | "all"> {
  return await select({
    message: "Which component do you want to test?",
    choices: [
      { name: "All  — run every test in order", value: "all" as const },
      { name: "Serial link  — daemon ↔ board (PING)", value: "serial" as const },
      { name: "OLED display  — show 'Hello'", value: "oled" as const },
      { name: "Buzzer  — play a tone", value: "buzzer" as const },
    ],
  });
}

function isComponent(s: string): s is Component {
  return (ORDER as readonly string[]).includes(s);
}

async function runOne(c: Component): Promise<boolean> {
  header(c);
  let ok = false;
  if (c === "serial") ok = await testSerial();
  else if (c === "oled") ok = await testOled();
  else if (c === "buzzer") ok = await testBuzzer();
  if (ok) {
    console.log(`  ✓ PASS — ${c}`);
  } else {
    console.log(`  ✗ FAIL — ${c}`);
    troubleshoot(TIPS[c]);
  }
  return ok;
}

// --- Individual tests --------------------------------------------------

async function testSerial(): Promise<boolean> {
  console.log("  Sending PING via the daemon...");
  let result: Result;
  try {
    result = await client.ping();
  } catch (e: any) {
    console.log(`  Error: ${e?.message || e}`);
    return false;
  }
  if (!result.ok) {
    console.log(`  Daemon responded with failure: ${describe(result)}`);
    return false;
  }
  // The board replies "PONG"; the daemon surfaces that in `response`.
  const reply = typeof result.response === "string" ? result.response : JSON.stringify(result.response);
  console.log(`  Board replied: ${reply || "(empty)"}`);
  return /pong/i.test(reply || "");
}

async function testOled(): Promise<boolean> {
  console.log('  Sending: SHOW text "Hello"');
  const result = await client.text("Hello");
  if (!result.ok) {
    console.log(`  Firmware did not accept the command: ${describe(result)}`);
    return false;
  }
  // Give the display a moment to repaint before the user looks at it.
  await sleep(300);
  return await askYesNo("  Can you see the text 'Hello' on the OLED?");
}

async function testBuzzer(): Promise<boolean> {
  // The firmware's buzzer only jingles on an expression *change* in
  // Desktop Mode (see firmware.ino — `if (expr != jingledExpr)`).
  // Two distinct faces back-to-back guarantees a transition regardless
  // of what the pet was showing before.
  console.log("  Forcing a face transition to trigger a jingle...");
  const a = await client.face("neutral");
  if (!a.ok) {
    console.log(`  Firmware did not accept 'face neutral': ${describe(a)}`);
    return false;
  }
  await sleep(600);
  const b = await client.face("excited");
  if (!b.ok) {
    console.log(`  Firmware did not accept 'face excited': ${describe(b)}`);
    return false;
  }
  // Let the jingle finish before asking — it's ~1 s long.
  await sleep(1200);
  return await askYesNo("  Did you hear a tone from the buzzer?");
}

// --- Pretty-printing helpers ------------------------------------------

function header(c: Component): void {
  const titles: Record<Component, string> = {
    serial: "Serial link (PING)",
    oled: "OLED display (SSD1306, I2C 0x3C — SDA=GPIO5, SCL=GPIO6)",
    buzzer: "Buzzer (passive piezo on GPIO10)",
  };
  console.log("");
  console.log("------------------------------------------------------------");
  console.log(`  ${titles[c]}`);
  console.log("------------------------------------------------------------");
}

function troubleshoot(lines: string[]): void {
  console.log("  Troubleshooting:");
  for (const line of lines) console.log(`    • ${line}`);
}

function summary(fails: number, passes: number): void {
  console.log("");
  console.log("============================================================");
  console.log(`  Done. ${passes} passed, ${fails} failed.`);
  console.log("============================================================");
}

function describe(r: Result): string {
  const msg = r.message ?? (typeof r.response === "string" ? r.response : JSON.stringify(r.response));
  return msg || "(no message)";
}

// `confirm` from @inquirer/prompts would work, but rolling a yes/no
// matches the rest of the CLI's lightweight, non-fancy prompts.
async function askYesNo(question: string): Promise<boolean> {
  const choice = await select({
    message: question,
    choices: [
      { name: "yes", value: true },
      { name: "no", value: false },
    ],
  });
  return choice;
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}
