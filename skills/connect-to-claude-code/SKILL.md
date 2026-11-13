---
name: connect-to-claude-code
description: Use when setting up Claude Code hooks to send live events to a Clawdmeter device, or when a user asks how to wire Clawdmeter to their Claude Code session so Clawd reacts in real time.
---

# Connecting Clawdmeter to Claude Code

Clawdmeter receives `POST /event` requests from Claude Code hooks over your local network. No daemon, no BLE — just `curl` calls from two hook entries.

## Quick Reference

| Hook | Event | Clawd's reaction |
|------|-------|-----------------|
| `PreToolUse` | `t=p` | Waves and points from corner |
| `Stop` | `t=s` | Calms down, returns to idle |
| `PostToolUse` (optional) | `t=b` | Subtle notice (back after gap) |

## Steps

### 1. Confirm the device is reachable

```bash
curl -s http://clawdmeter.local/event -X POST -d "t=p"
# → HTTP 204 No Content = success
# → HTTP 401 Unauthorized = event token required (see step 2)
```

If `clawdmeter.local` doesn't resolve, use the IP shown on the device's Wi-Fi screen or your router's DHCP table.

### 2. Set an event token (optional)

Skip this on a trusted home LAN. For shared networks:

1. Open `http://clawdmeter.local` in a browser.
2. Under **Hook event token**, enter any string and save.
3. Add `-H "Authorization: Bearer <token>"` to every `curl` call below.

### 3. Add hooks to Claude Code settings

Add to `~/.claude/settings.json` (all projects) or `.claude/settings.json` (one project):

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -sf http://clawdmeter.local/event -X POST -d 't=p' -o /dev/null"
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
            "command": "curl -sf http://clawdmeter.local/event -X POST -d 't=s' -o /dev/null"
          }
        ]
      }
    ]
  }
}
```

With an event token, append `-H "Authorization: Bearer <token>"` to each `curl` command.

### 4. Verify

Trigger any Claude Code tool call — Clawd should wave from the corner of the usage screen. If the device is on the splash or another screen, navigate to the usage screen first (swipe or press PWR).

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `curl` times out | Device not on Wi-Fi; check Wi-Fi screen |
| HTTP 401 | Missing or wrong `Authorization: Bearer` header |
| No reaction despite 204 | Navigate to SCREEN_USAGE; director only runs there |
| `clawdmeter.local` not found | mDNS not working on your network; use IP address instead |
