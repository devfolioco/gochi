# How to set up the gochi dev environment

End-to-end software setup, before you touch any wires. By the end you'll
have:

- The repo cloned and built (firmware + CLI).
- `arduino-cli` set up with the ESP32-C3 toolchain pinned to the same
  versions the project uses.
- The `gochi` CLI runnable from the source tree, with the daemon ready
  to install once you have hardware.

Once everything in this guide works, move on to **[HOW-TO-BUILD.md](HOW-TO-BUILD.md)**
to wire the actual hardware.

The guide covers four hosts:

- [macOS](#macos)
- [Linux (Ubuntu / Debian / Fedora)](#linux)
- [Windows (native)](#windows-native)
- [WSL 2 (Ubuntu on Windows)](#wsl-2)

The shared steps — clone, build, verify — come after the
platform-specific bits.

---

## Prerequisites everywhere

Whatever host you're on, you'll need:

- **Git** — to clone the repo.
- **`arduino-cli` 1.0+** — builds and flashes the firmware. The
  project pins ESP32 core `3.3.8` and U8g2 `2.36.18` in
  `firmware/sketch.yaml`, so the install commands below are deterministic.
- **A JavaScript runtime for the CLI**. Two options:
  - **Bun** (preferred — what the project's `bun.lock` is generated against).
  - **Node 18+** with **npm** (also works; you can replace `bun` with `npm`
    in the commands below).
- **`clang-format`** — used by `make format` and `make format-check`.
  Not strictly required to build the firmware, but the Makefile lists
  those targets and the pre-commit / CI workflow assumes it's there, so
  install it now to avoid the "clang-format: command not found" surprise.
- **About 1 GB free disk** — the ESP32 core unpacks to ~500 MB.
- **A USB cable that carries data** (not power-only) once you get to
  flashing.

`make` is optional but the Makefile wraps common commands, so most steps
below assume it's installed (it's pre-installed on macOS / Linux / WSL;
on native Windows you can either install `make` or run the raw
`arduino-cli` commands the Makefile would have).

---

## macOS

```sh
# Xcode Command Line Tools (gives you git, make, clang)
xcode-select --install

# Homebrew, if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Toolchain
brew install arduino-cli
brew install oven-sh/bun/bun     # or: brew install node
brew install clang-format        # used by `make format` / `make format-check`
```

Skip ahead to [**Shared setup**](#shared-setup-clone--install--build).

---

## Linux

Tested on Ubuntu 22.04+ and Fedora 39+. Other distros work the same
way; substitute your package manager.

```sh
# Ubuntu / Debian
sudo apt update
sudo apt install -y git make curl build-essential clang-format

# Fedora
sudo dnf install -y git make curl gcc-c++ clang-tools-extra
```

(`clang-format` is a stand-alone package on Debian/Ubuntu but lives
inside `clang-tools-extra` on Fedora — both give you the same binary
on your `PATH`.)

### `arduino-cli`

Official install script — drops a single static binary into `~/bin/`:

```sh
mkdir -p ~/bin
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
  | BINDIR=~/bin sh
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc      # or ~/.zshrc
exec $SHELL
```

### Bun

```sh
curl -fsSL https://bun.sh/install | bash
exec $SHELL
```

(Or use Node.js: `sudo apt install nodejs npm` / `sudo dnf install nodejs`,
ensure Node ≥ 18 with `node --version`.)

### USB serial permissions

By default Linux only lets `root` open `/dev/ttyACM*` / `/dev/ttyUSB*`.
Add yourself to the `dialout` group (or `uucp` on Fedora/Arch) and log
out and back in:

```sh
sudo usermod -aG dialout $USER       # Ubuntu / Debian
sudo usermod -aG uucp    $USER       # Fedora / Arch
# Log out + back in (the group only takes effect on a new session)
```

If you skip this you'll get `permission denied` on `arduino-cli upload`
later.

Skip ahead to [**Shared setup**](#shared-setup-clone--install--build).

---

## Windows (native)

You can develop natively on Windows without WSL — the CLI's daemon
service backend supports Windows Task Scheduler. The downside is a few
of the Makefile niceties don't work (Windows ships without `make` by
default) and the `make flash` wrappers don't apply.

### Tools

Easiest: install via [winget](https://learn.microsoft.com/en-us/windows/package-manager/winget/)
(pre-installed on Windows 11):

```powershell
winget install --id Git.Git
winget install --id ArduinoSA.CLI
winget install --id Oven-sh.Bun       # or: winget install OpenJS.NodeJS.LTS
winget install --id LLVM.LLVM         # provides clang-format
```

If `winget` isn't available, download the installers manually:
- Git for Windows: <https://git-scm.com/download/win>
- arduino-cli: <https://github.com/arduino/arduino-cli/releases> (pick the
  Windows zip, extract `arduino-cli.exe` into a folder on your `PATH`)
- Bun: <https://bun.sh/install>
- LLVM (includes `clang-format.exe`): <https://releases.llvm.org/>

### USB driver

Plug in an ESP32-C3 SuperMini. Modern Windows 10/11 has the USB-CDC
class driver built in, so the board should appear as a COM port within
a few seconds. Find it in Device Manager → **Ports (COM & LPT)** —
something like `USB Serial Device (COM7)`. Remember that COM number;
you'll pass it to `arduino-cli` later.

If the device shows up as `Unknown device` instead, install the generic
USB-CDC driver from Espressif:
<https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/usb-serial-jtag-console.html>

### Optional: install `make`

If you want the Makefile commands (`make build`, `make flash`):

```powershell
winget install GnuWin32.Make
```

The Makefile recipes use POSIX-shell syntax (`if`, `trap`, command
substitution), so they only run cleanly under **Git Bash** or
**MSYS2** — both ship a `sh.exe` that `make` picks up automatically.
From plain `cmd.exe` or PowerShell the `flash` / `upload` recipes
fail with quoting errors; either open a Git Bash shell for those
commands, or run the raw `arduino-cli` commands shown below.

### Finding your COM port

The Makefile can't glob `/dev/cu.usbmodem*` on Windows, so port
auto-detection is disabled — you'll pass `PORT=COMx` explicitly to
every `make flash` (and the raw `arduino-cli upload` below). Two
ways to find the right COM number:

- **Device Manager** → expand **Ports (COM & LPT)** → look for
  `USB Serial Device (COMn)` that appears when you plug the board in.
- **`make ports`** (from Git Bash) or **`arduino-cli --config-file firmware/arduino-cli.yaml board list`**
  (from any shell) — the SuperMini shows up with VID `303a`
  (Espressif).

### Raw flash command (no `make` needed)

If you skipped `make`, the `make build` and `make flash` targets
boil down to two `arduino-cli` calls. Run them from the repo root,
substituting your COM port:

```powershell
# build
arduino-cli --config-file firmware/arduino-cli.yaml compile `
  --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc `
  --build-path firmware/build firmware

# upload
arduino-cli --config-file firmware/arduino-cli.yaml upload `
  --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc `
  --port COM7 `
  --input-dir firmware/build firmware
```

(Backticks are PowerShell line-continuations; in `cmd.exe` use `^`
or put it all on one line.)

> ⚠️ **"Access is denied" on upload.** If `arduino-cli upload` fails
> with `Access is denied` or `the port is busy`, something else has
> the COM port open. Common culprits:
> - The **`gochi` daemon** is holding it. Stop it before flashing:
>   `gochi stop` (or, brute force: `Stop-Process -Name gochi -Force -ErrorAction SilentlyContinue`).
>   `make flash` on macOS/Linux already does this automatically — on
>   Windows you have to do it yourself.
> - **Arduino IDE Serial Monitor**, **VS Code's serial extension**,
>   **PuTTY**, or any other terminal attached to the same COM port.
>   Close them.
> - If nothing obvious is holding it, unplug the USB for ~3 s and
>   plug it back in to drop any zombie handles. Then re-run upload.
> - As a last resort, **force download mode**: hold the **BOOT**
>   button, tap **RESET**, release **BOOT**, then re-run upload
>   within a couple of seconds.

Skip ahead to [**Shared setup**](#shared-setup-clone--install--build).

---

## WSL 2

WSL 2 is a clean way to develop on Windows: you get a full Linux
toolchain, and the file system is Linux-native. **One catch**: WSL 2
has no USB access out of the box, so flashing the firmware requires
attaching the COM port from Windows. The dev-only steps (clone, build,
CLI) work without USB; only `make flash` and `make monitor` need it.

### Install the Linux side

Inside your WSL Ubuntu shell, follow the [Linux instructions above](#linux).
You don't need the `dialout` group step — WSL handles permissions on
attached USB devices via `udev` rules that come with `usbipd`.

### USB passthrough

Install [usbipd-win](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)
on the Windows side (PowerShell as Administrator):

```powershell
winget install --interactive --exact dorssel.usbipd-win
```

Plug in the SuperMini. Then in PowerShell:

```powershell
usbipd list                    # find the BUSID of "USB Serial Device"
usbipd bind --busid 1-4        # one-time: marks the device shareable
usbipd attach --wsl --busid 1-4
```

Inside WSL the board now appears as `/dev/ttyACM0`. `arduino-cli upload`
and the `gochi` CLI work normally from this point.

> You'll need to re-run `usbipd attach` after every reboot or every time
> you unplug and replug the board. A simple alias in PowerShell helps:
> `function Attach-Gochi { usbipd attach --wsl --busid 1-4 }`.

Skip ahead to [**Shared setup**](#shared-setup-clone--install--build).

---

## Shared setup: clone → install → build

Now that the platform-specific tools are in place, the rest is the same
everywhere.

### 1. Clone the repo

```sh
git clone https://github.com/devfolioco/gochi.git
cd gochi
```

### 2. Install the ESP32 toolchain

The project pins the core version inside `firmware/arduino-cli.yaml` and
`firmware/sketch.yaml`, so this install is reproducible — same versions
every time, on any host.

```sh
arduino-cli --config-file firmware/arduino-cli.yaml core update-index
arduino-cli --config-file firmware/arduino-cli.yaml core install esp32:esp32@3.3.8
```

This downloads ~500 MB of toolchain and unpacks it under
`~/Library/Arduino15` (macOS) / `~/.arduino15` (Linux / WSL) /
`%LOCALAPPDATA%\Arduino15` (Windows).

### 3. Build the firmware

```sh
make build
```

…or, if you don't have `make`:

```sh
arduino-cli --config-file firmware/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc \
  --build-path firmware/build firmware
```

**Expected output ends with:**

```
Sketch uses 358xxx bytes (27%) of program storage space. Maximum is 1310720 bytes.
Global variables use 20xxx bytes (6%) of dynamic memory, leaving 307xxx bytes for local variables. Maximum is 327680 bytes.
```

If this works, your **firmware build setup is good**. Compilation
itself doesn't need any hardware connected.

### 4. Install CLI dependencies

```sh
cd cli
bun install            # or: npm install
```

### 5. Verify the CLI runs

```sh
bun src/cli.ts --version
```

Expected: prints the version from `cli/package.json` (e.g. `0.1.0`).

```sh
bun src/cli.ts --help
```

Expected: the full command list (`setup`, `face`, `mood`, `text`,
`image`, `i2c`, `test`, etc.).

### 6. Optional: TypeScript typecheck

```sh
bunx tsc --noEmit
```

Expected: exits 0 with no output. This confirms `@types/node` and the
project's `tsconfig.json` are wired up correctly — useful before
your editor starts squawking.

```sh
cd ..                  # back to repo root
```

### 7. Sanity check: everything compiles together

From the repo root:

```sh
make build                 # firmware compiles
( cd cli && bun src/cli.ts --version )   # CLI runs
```

If both produce output without errors, **your dev setup is ready**.

---

## What you don't need yet

You haven't done these steps and don't need to until you have hardware:

- `gochi setup` — installs the long-running daemon as a launchd /
  systemd / Task Scheduler service. Only useful once you have a board
  to talk to.
- `make flash` — uploads firmware to the board.
- `make monitor` — opens the serial monitor.

Those all need a USB-connected board. They're covered in
[**HOW-TO-BUILD.md**](HOW-TO-BUILD.md), step by step.

---

## Common dev-setup issues

| Symptom | Fix |
|---------|-----|
| `arduino-cli: command not found` | `~/bin` (Linux) or Bun's install dir isn't on `PATH`. Open a fresh shell, or re-run the install script's `PATH` line. |
| `clang-format: command not found` on `make format` | Install it. Linux: `sudo apt install clang-format` (Debian/Ubuntu) or `sudo dnf install clang-tools-extra` (Fedora). macOS: `brew install clang-format`. Windows: `winget install LLVM.LLVM`. |
| `make build` says `command not found` for `make` | Install `make` (Linux/macOS: usually pre-installed; Windows: `winget install GnuWin32.Make` or use the raw `arduino-cli` command in step 3). |
| `Error: Platform 'esp32:esp32@3.3.8' not found` | Re-run step 2 — the `core update-index` step is what makes 3.3.8 visible. |
| `bun: command not found` after install | Either re-open the shell or `source ~/.bashrc` (Bun appends a `PATH` export to your rc file). |
| `tsc` reports `Cannot find module 'node:path'` | You skipped the `bun install` / `npm install` step — `@types/node` is a dev dependency. |
| `arduino-cli upload` says `permission denied` on Linux | You're not in the `dialout` group, or you didn't log out and back in after `usermod`. |
| `arduino-cli upload` says `Access is denied` / `the port is busy` on Windows | Something else has the COM port open. Run `gochi stop` (or `Stop-Process -Name gochi -Force`), close Arduino IDE's Serial Monitor / PuTTY / VS Code serial extensions, then retry. If still stuck, unplug USB for ~3 s. Last resort: hold BOOT, tap RESET, release BOOT, then upload. |
| `make flash` on Windows fails with quoting errors | The recipe uses POSIX shell syntax — run it from **Git Bash** or **MSYS2**, not `cmd.exe` / PowerShell. Alternative: use the raw `arduino-cli upload` command from the [Windows section](#windows-native). |
| `make flash` on Windows says `no port` | Auto-detection is disabled on Windows. Pass it explicitly: `make flash PORT=COM7` (find it via `make ports` or Device Manager). |
| WSL doesn't see `/dev/ttyACM0` | `usbipd attach --wsl --busid <id>` from Windows PowerShell — and re-run it after every reboot / replug. |
| Windows shows "Unknown device" in Device Manager | The ESP32-C3 native USB-CDC driver isn't loaded. On Windows 10 1903+ it should be automatic; older builds need the Espressif driver. |

For anything else, `arduino-cli` and `bun --help` both have good
built-in help, and the error messages from `arduino-cli compile` are
usually accurate (it's the upload step that gets cryptic).

---

## Next: wire the hardware

Once everything above works, you have a working dev environment but no
pet to drive. Move to **[HOW-TO-BUILD.md](HOW-TO-BUILD.md)** — it walks
through wiring the OLED, buzzer, and IMU onto a breadboard one component
at a time, with a `gochi` test command to verify each before moving on.
