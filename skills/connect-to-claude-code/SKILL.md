---
name: connect-to-claude-code
description: Use when setting up Claude Code hooks to send live events to a Clawdmeter device, or when a user asks how to wire Clawdmeter to their Claude Code session so Clawd reacts in real time.
---

# Connecting Clawdmeter to Claude Code

Clawdmeter has two HTTP endpoints. They serve different purposes and work together:

| Endpoint | What it does |
|----------|-------------|
| `POST /event` | Feeds the **signal layer** — liveliness-gated corner reactions (noticing, wave, joy) and the conditional attention animation |
| `POST /clawd` | Commands a **director beat directly** — bypasses policy, always fires, used for QA and demos |

For daily hook wiring, use `/event`. Use `/clawd` for direct beat control, QA, or demos.

Two env vars control the connection — set them once in `~/.claude/settings.json`:

| Var | Default | Purpose |
|-----|---------|---------|
| `CLAWDMETER_URL` | `http://clawdmeter.local` | Device address (override if mDNS doesn't work) |
| `CLAWDMETER_TOKEN` | _(empty)_ | Bearer token (omit if no token is set on the device) |

## Table of Contents

- [Hook setup (`POST /event`)](#hook-setup-post-event)
- [Director beats (`POST /clawd`)](#director-beats-post-clawd)
- [Steps](#steps)
- [Troubleshooting](#troubleshooting)

---

## Hook setup (`POST /event`)

Add to `~/.claude/settings.json`:

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -sf ${CLAWDMETER_URL:-http://clawdmeter.local}/clawd -X POST -d 'beat=home' -H \"Authorization: Bearer ${CLAWDMETER_TOKEN}\" -o /dev/null; curl -sf ${CLAWDMETER_URL:-http://clawdmeter.local}/event -X POST -d 't=p' -H \"Authorization: Bearer ${CLAWDMETER_TOKEN}\" -o /dev/null"
          }
        ]
      }
    ],
    "Stop": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -sf ${CLAWDMETER_URL:-http://clawdmeter.local}/clawd -X POST -d 'beat=attention&msg=Your+turn!' -H \"Authorization: Bearer ${CLAWDMETER_TOKEN}\" -o /dev/null"
          }
        ]
      }
    ]
  }
}
```

If no token is set, the `Authorization: Bearer ` header is sent with an empty value —
the device ignores it when no token is configured.

### What each hook does

**`PreToolUse`** — Claude just started using a tool (you replied, Claude is working):
1. `beat=home` via `/clawd` — dismisses any pending attention animation so Clawd returns to his corner
2. `t=p` via `/event` — corner noticing reaction (Clawd points); feeds the signal layer for long-turn wave tracking

**`Stop`** — Claude finished its turn and is waiting for you:
- `beat=attention&msg=Your+turn!` via `/clawd` — the big Clawd walks on and fetches you

This mirrors the upstream desktop app's `alert` / `clear` modes: Clawd appears when Claude is waiting for input and disappears when you reply. A tap on the device or the two-minute timeout also dismisses it.

### Attention animation setting

The **Attention animation** setting at `http://clawdmeter.local` → **Clawd** controls
how long Clawd points and waves before returning to his corner on his own (off, 10 s,
30 s, or 2 min). The Stop hook triggers this regardless of the setting — the setting
only affects how long Clawd stays on screen if you don't tap to dismiss him.

### Session joy

When Claude's 5-hour usage window resets, the device detects it from the usage
data and fires two "jumping happy" acts — no hook needed.

---

## Director beats (`POST /clawd`)

Directly commands a named beat, bypassing the liveliness policy. Useful for demos, QA,
and testing director behaviour. Protected by the same event token as `/event`.

```bash
curl -sf ${CLAWDMETER_URL:-http://clawdmeter.local}/clawd \
  -X POST -d "beat=<name>" \
  -H "Authorization: Bearer ${CLAWDMETER_TOKEN}"
```

Returns **204** on success, **409** if the beat is refused (wrong director state),
**400** if `beat` is missing.

| Beat | Trigger / what happens |
|------|----------------------|
| `attention` | Big Clawd walks on and fetches you. Optional `msg=` sets the overlay text (default: "Clawdmeter test alert"). Duration follows the **Attention animation** setting. |
| `home` | Dismiss attention / return to idle. Sends Clawd back to his corner. |
| `peek` | A tap on the meters: Clawd hops, ducks out through the nearest edge, the face rises and stays curious for up to 5 minutes. |
| `peek <x>` | Same but the face rises toward pixel column `<x>` (e.g. `beat=peek+120`). |
| `show` | After a busy stretch (normally auto-fires at activity level 3 held for 5 min): Clawd points, walks off, the full-size performer walks on and plays a scene (~20 s), then waves and walks off. PSRAM only; falls back to corner acts otherwise. Must be in B_HOME. |
| `screensaver` | After quiet time (normally auto-fires after 10 min of no engagement): Clawd walks off, the face rises and rests, with calm performer dips every 3–5 min. Must be in B_HOME. |
| `lurk` | Clawd starts a lurk trip across the bottom of the screen. PSRAM only; must be in B_HOME. |
| `joy` | Two "jumping happy" acts (queued if he's mid-act). |
| `trace` | Toggle per-step beat logging on the serial console. |
| `liveliness` | Print the current liveliness level to the serial console. |

### QA loop

```bash
URL=${CLAWDMETER_URL:-http://clawdmeter.local}
TOK=${CLAWDMETER_TOKEN}
beat() { curl -sf "$URL/clawd" -X POST -d "beat=$*" -H "Authorization: Bearer $TOK" \
         && echo "ok: $*" || echo "refused: $*"; }

beat attention              # big Clawd walks on
sleep 5
beat home                   # dismiss, he returns to his seat
sleep 2
beat peek                   # Clawd hops and ducks out, face rises
sleep 6
beat home                   # face drops, Clawd walks home
sleep 2
beat show                   # performer walk-on (PSRAM boards only)
sleep 15
beat home                   # cut short: performer runs off
sleep 2
beat screensaver            # face rests, calm dips
sleep 10
beat home                   # face drops, Clawd walks home
```

---

## Steps

### 1. Find the device address

mDNS (`clawdmeter.local`) works on most home networks. Test it:

```bash
curl -s http://clawdmeter.local/event -X POST -d "t=p" -o /dev/null -w "%{http_code}"
# → 204 = success  |  401 = event token required (see step 2)
```

If it times out, find the IP on the device's Wi-Fi screen or your router's DHCP table.

### 2. Configure env vars in Claude Code settings

Add to `~/.claude/settings.json`. Omit any var you don't need:

```json
{
  "env": {
    "CLAWDMETER_URL": "http://192.168.1.42",
    "CLAWDMETER_TOKEN": "your-token-here"
  }
}
```

- **`CLAWDMETER_URL`**: only needed if `clawdmeter.local` doesn't resolve (WSL, some corporate networks)
- **`CLAWDMETER_TOKEN`**: only needed if you set a Hook event token on the device (open `http://clawdmeter.local` → **Hook event token** to set one)

### 3. Add hooks

Paste the JSON from [Hook setup](#hook-setup-post-event) into `~/.claude/settings.json`,
merging with any existing `hooks` block.

### 4. Verify

Ask Claude Code to do anything that uses a tool. You should see:
1. Clawd points from his corner while Claude works (`PreToolUse`)
2. The big Clawd walks on with "Your turn!" when Claude finishes (`Stop`)
3. The next tool use dismisses him and he returns to his corner (`PreToolUse → beat=home`)

Navigate to the usage screen first if needed (swipe or press PWR); the director only runs there.

### Installing the skill globally

If you work across multiple projects, install the bundled skill globally so
`/connect-to-claude-code` is available in any Claude Code session:

```bash
npx skills add mcdeck/clawdmeter-wifi --skill connect-to-claude-code -a claude-code -g -y
```

Requires Node.js 22.20+.

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `curl` times out | Device not on Wi-Fi; check Wi-Fi screen |
| HTTP 401 | Set `CLAWDMETER_TOKEN` in `env` block to match the device's Hook event token |
| No reaction despite 204 | Navigate to SCREEN_USAGE; director only runs there |
| `clawdmeter.local` not found | Set `CLAWDMETER_URL` to the device's IP in the `env` block |
| `/clawd` returns 409 | Beat refused — wrong director state (e.g. `show` requires B_HOME); send `beat=home` first |
| No attention animation on `t=p` | Enable it at `http://clawdmeter.local` → **Clawd → Attention animation** |
