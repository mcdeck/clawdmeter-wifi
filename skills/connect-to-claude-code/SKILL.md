---
name: connect-to-claude-code
description: Use when setting up Claude Code hooks to send live events to a Clawdmeter device, or when a user asks how to wire Clawdmeter to their Claude Code session so Clawd reacts in real time.
---

# Connecting Clawdmeter to Claude Code

Clawdmeter receives `POST /event` requests from Claude Code hooks over your local network. No daemon, no BLE — just `curl` calls from two hook entries.

Two env vars control the connection — set them once in `~/.claude/settings.json` and the hooks just work:

| Var | Default | Purpose |
|-----|---------|---------|
| `CLAWDMETER_URL` | `http://clawdmeter.local` | Device address (override if mDNS doesn't work) |
| `CLAWDMETER_TOKEN` | _(empty)_ | Bearer token (omit if no token is set on the device) |

## Quick Reference

| Hook | Event | Clawd's reaction |
|------|-------|-----------------|
| `PreToolUse` | `t=p` | Waves and points from corner |
| `Stop` | `t=s` | Calms down, returns to idle |
| `PostToolUse` (optional) | `t=b` | Subtle notice (back after gap) |

## Steps

### 1. Find the device address

mDNS (`clawdmeter.local`) works on most home networks. Test it:

```bash
curl -s http://clawdmeter.local/event -X POST -d "t=p"
# → HTTP 204 No Content = success
# → HTTP 401 Unauthorized = event token required (see step 2)
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

### 3. Add hooks to Claude Code settings

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
            "command": "curl -sf ${CLAWDMETER_URL:-http://clawdmeter.local}/event -X POST -d 't=p' -H \"Authorization: Bearer ${CLAWDMETER_TOKEN}\" -o /dev/null"
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
            "command": "curl -sf ${CLAWDMETER_URL:-http://clawdmeter.local}/event -X POST -d 't=s' -H \"Authorization: Bearer ${CLAWDMETER_TOKEN}\" -o /dev/null"
          }
        ]
      }
    ]
  }
}
```

If no token is set, the `Authorization: Bearer ` header is sent with an empty value — the device ignores it when no token is configured.

### 4. Verify

Trigger any Claude Code tool call — Clawd should wave from the corner of the usage screen. If the device is on the splash or another screen, navigate to the usage screen first (swipe or press PWR).

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `curl` times out | Device not on Wi-Fi; check Wi-Fi screen |
| HTTP 401 | Set `CLAWDMETER_TOKEN` in `env` block to match the device's Hook event token |
| No reaction despite 204 | Navigate to SCREEN_USAGE; director only runs there |
| `clawdmeter.local` not found | Set `CLAWDMETER_URL` to the device's IP in the `env` block |
