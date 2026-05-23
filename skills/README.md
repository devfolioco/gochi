# Tamagotchi skills

This folder ships an [agentskills.io](https://agentskills.io/)-compliant `SKILL.md` that teaches an AI coding agent how to drive the physical tamagotchi over its locally installed CLI or its background HTTP server (`http://localhost:7474`).

- Source: [`skills/tamagotchi/SKILL.md`](https://github.com/prathamVaidya/tamagotchi/blob/main/skills/tamagotchi/SKILL.md)
- Triggers on explicit commands ("tamagotchi face happy", "set mood playful", "show text on the tamagotchi") and optionally on task outcomes (build pass → happy, fail → sad, long wait → sleepy).

Install the [`tamagotchi` CLI](../cli) first so the skill has something to talk to. The skill itself follows the open `SKILL.md` spec — a single markdown file with YAML frontmatter — so it works in every agent that implements the standard.

## Quick install (recommended)

Use [`npx skills`](https://github.com/vercel-labs/skills) — the universal installer maintained by Vercel Labs. It auto-detects which agents you have installed and drops the skill into the right place for each.

```sh
# project-local: writes into ./.<agent>/skills/ for every agent it finds
npx skills add prathamVaidya/tamagotchi

# global: writes into ~/.<agent>/skills/ so every project sees it
npx skills add prathamVaidya/tamagotchi -g

# target a specific agent (or several)
npx skills add prathamVaidya/tamagotchi -a claude-code
npx skills add prathamVaidya/tamagotchi -a claude-code -a openclaw -a hermes-agent -g
```

That's the whole install. Restart the agent (or open a new session) and the `tamagotchi` skill is discovered automatically. Update later with `npx skills update tamagotchi`, list with `npx skills list`, remove with `npx skills remove tamagotchi`.

## Agent paths

`npx skills` writes to the conventional location for each agent. If you'd rather wire it up by hand (symlink or copy `skills/tamagotchi/` yourself), these are the targets it would use:

| Agent        | `--agent` slug | Project path        | Global path                  |
| ------------ | -------------- | ------------------- | ---------------------------- |
| Claude Code  | `claude-code`  | `.claude/skills/`   | `~/.claude/skills/`          |
| OpenClaw     | `openclaw`     | `skills/`           | `~/.openclaw/skills/`        |
| Hermes Agent | `hermes-agent` | `.hermes/skills/`   | `~/.hermes/skills/`          |

For other agents (Codex, Cursor, opencode, Cline, Gemini CLI, etc. — 50+ supported), see the full table in [`vercel-labs/skills`](https://github.com/vercel-labs/skills#supported-agents).

## Manual install

If you'd rather not run `npx`, clone the repo and symlink the skill folder yourself.

```sh
git clone https://github.com/prathamVaidya/tamagotchi.git ~/src/tamagotchi

# Claude Code (global)
mkdir -p ~/.claude/skills
ln -s ~/src/tamagotchi/skills/tamagotchi ~/.claude/skills/tamagotchi

# OpenClaw (global)
mkdir -p ~/.openclaw/skills
ln -s ~/src/tamagotchi/skills/tamagotchi ~/.openclaw/skills/tamagotchi

# Hermes Agent (global)
mkdir -p ~/.hermes/skills
ln -s ~/src/tamagotchi/skills/tamagotchi ~/.hermes/skills/tamagotchi
```

A symlink means `git pull` updates the skill for every agent at once. Use `cp -r` instead if your shell or agent doesn't follow symlinks.

## Verifying

After install, the skill is `user_invocable`, so most agents expose it as `/tamagotchi`. You can also just ask in plain English — "make the tamagotchi happy", "show 'hello' on the device", "ping the tamagotchi" — and the agent should pick the skill up on its own.

End-to-end smoke test:

```sh
tamagotchi health             # server + device status, should print connected:true
tamagotchi face happy         # device should show the happy face
tamagotchi face neutral       # reset
```

## Troubleshooting

- **Agent can't find the skill.** Make sure `SKILL.md` is all caps and lives in a folder named `tamagotchi`. Some runtimes are strict about both. Re-run `npx skills list` to see what got installed where.
- **Skill loads but commands fail.** Run `tamagotchi health` yourself — the CLI server probably isn't running. Start it with `tamagotchi server start` (or `tamagotchi server install` the first time).
- **Server is up but `connected: false`.** Plug the device in. The server reconnects automatically; no restart needed.

## Related

- [Agent Skills specification (agentskills.io)](https://agentskills.io)
- [`vercel-labs/skills` — the `npx skills` CLI](https://github.com/vercel-labs/skills)
- [skills.sh — directory of published skills](https://skills.sh)
