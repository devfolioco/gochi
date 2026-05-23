# Tamagotchi skills

This folder contains an [agentskills.io](https://agentskills.io/)-compliant `SKILL.md` that teaches an AI coding agent how to drive the physical tamagotchi over its locally installed CLI or its background HTTP server (`http://localhost:7474`).

- Source: [`skills/tamagotchi/SKILL.md`](https://github.com/prathamVaidya/tamagotchi/blob/main/skills/tamagotchi/SKILL.md)
- Triggers on explicit commands ("tamagotchi face happy", "set mood playful", "show text on the tamagotchi") and optionally on task outcomes (build pass → happy, fail → sad, long wait → sleepy).

The format is the open `SKILL.md` standard, so the same file works in any compatible runtime — install it once into your agent's skills directory and it's discovered automatically. Install the [`tamagotchi` CLI](../cli) first so the skill has something to talk to.

## Claude Code

Claude Code loads skills from `~/.claude/skills/` (user-wide) or `.claude/skills/` (project-local).

**User-wide install (recommended):**

```sh
# clone the repo somewhere stable
git clone https://github.com/prathamVaidya/tamagotchi.git ~/src/tamagotchi

# symlink the skill so updates flow through with `git pull`
mkdir -p ~/.claude/skills
ln -s ~/src/tamagotchi/skills/tamagotchi ~/.claude/skills/tamagotchi
```

**Project-local install** (only this repo's Claude sessions see it):

```sh
mkdir -p .claude/skills
ln -s "$PWD/skills/tamagotchi" .claude/skills/tamagotchi
```

Restart Claude Code (or open a new session). Verify with `/tamagotchi` — the skill is `user_invocable`, so it appears as a slash command. Otherwise, just ask Claude "make the tamagotchi happy" and it should pick up the skill on its own.

## opencode

[opencode](https://opencode.ai/docs/skills/) has first-class `SKILL.md` support and auto-discovers skills from several locations. The simplest are:

- `~/.config/opencode/skills/*/SKILL.md` — global
- `.opencode/skills/*/SKILL.md` — project-local

```sh
git clone https://github.com/prathamVaidya/tamagotchi.git ~/src/tamagotchi

mkdir -p ~/.config/opencode/skills
ln -s ~/src/tamagotchi/skills/tamagotchi ~/.config/opencode/skills/tamagotchi
```

opencode also reads `~/.claude/skills/*/SKILL.md` and `~/.agents/skills/*/SKILL.md`, so if you've already installed the skill for Claude Code you don't need to do anything else — opencode will see it too.

Confirm with `opencode` → the agent should list `tamagotchi` among available skills.

## Hermes Agent

[Hermes Agent](https://hermes-agent.nousresearch.com/docs/) expects skills under `~/.hermes/skills/`, optionally nested by category. The `SKILL.md` here works without modification (Hermes follows the same agentskills.io standard).

```sh
git clone https://github.com/prathamVaidya/tamagotchi.git ~/src/tamagotchi

mkdir -p ~/.hermes/skills
ln -s ~/src/tamagotchi/skills/tamagotchi ~/.hermes/skills/tamagotchi
```

Hermes picks the skill up on next launch. You can verify by asking it to ping or show a face — it should reach for the skill and run the `tamagotchi` CLI or curl the local server.

## Other agents

Any agent that supports the [agentskills.io](https://agentskills.io/) open standard can load this skill — point its skills directory at `skills/tamagotchi/` (symlink, copy, or git submodule). The skill itself is just `SKILL.md` with YAML frontmatter; there's no runtime or build step.

## Updating

If you symlinked, `git pull` in your clone updates every agent at once. If you copied the file, re-copy after each release. The skill is small and rarely changes, so symlinks are usually the right call.

## Troubleshooting

- **The agent can't find the skill.** Check `SKILL.md` is spelled in all caps and the parent folder is named `tamagotchi`. Some runtimes are strict about both.
- **Skill loads but commands fail.** Run `tamagotchi health` yourself — the CLI server probably isn't running. Start it with `tamagotchi server start` (or `tamagotchi server install` the first time).
- **Server is up but `connected: false`.** Plug the device in. The server reconnects automatically; no restart needed.
