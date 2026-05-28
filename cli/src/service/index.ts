// service/index.ts — platform dispatcher for service management.
//
// Each backend (darwin / linux / windows) implements the same five-call
// surface. Pick at runtime instead of import-time so importing this
// module never touches launchctl / systemctl / schtasks.exe directly.

import * as darwin from "./darwin";
import * as linux from "./linux";
import * as windows from "./windows";

type ServiceBackend = {
  setup(): void | Promise<void>;
  enableHttp(): void | Promise<void>;
  disableHttp(): void | Promise<void>;
  daemonStatus(): Promise<void>;
  serverStatus(): Promise<void>;
};

function backend(): ServiceBackend {
  switch (process.platform) {
    case "darwin":
      return darwin;
    case "linux":
      return linux;
    case "win32":
      return windows;
    default:
      console.error(
        `gochi: unsupported platform '${process.platform}'.\n` +
          "Run `gochi daemon run` (and optionally `gochi server run`) in a terminal to use the CLI manually.",
      );
      process.exit(1);
  }
}

export const setup = (): void | Promise<void> => backend().setup();
export const enableHttp = (): void | Promise<void> => backend().enableHttp();
export const disableHttp = (): void | Promise<void> => backend().disableHttp();
export const daemonStatus = (): Promise<void> => backend().daemonStatus();
export const serverStatus = (): Promise<void> => backend().serverStatus();
