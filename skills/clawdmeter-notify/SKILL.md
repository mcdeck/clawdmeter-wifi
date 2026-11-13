---
name: clawdmeter-notify
description: Use at the end of a long task, or at any point where Claude is handing work back to the user for review. Sends a Clawdmeter attention beat so Clawd physically fetches the user at their desk. Requires the connect-to-claude-code skill to be set up.
---

# Clawdmeter: notify user

When you have finished a task and are waiting for the user to review or respond,
alert them via their Clawdmeter device by running this command:

```bash
curl -sf "${CLAWDMETER_URL:-http://clawdmeter.local}/clawd" \
  -X POST \
  --data-urlencode "beat=attention" \
  --data-urlencode "msg=<message>" \
  -H "Authorization: Bearer ${CLAWDMETER_TOKEN}" \
  -o /dev/null || true
```

The `|| true` ensures the task does not fail if the device is unreachable.

## Writing the message

Derive a short message (under 25 characters) from the task you just completed.
It appears as an overlay on the device screen. Good examples:

| Situation | `msg=` |
|-----------|--------|
| Code change ready to review | `Ready for review` |
| Long build / test run finished | `Tests passing` |
| PR created | `PR created` |
| Waiting for a decision | `Your call` |
| Question for the user | `Question for you` |
| Generic hand-off | `Your turn!` |

## When to use

Invoke this skill at the natural "I'm done, your turn" moment:

- After presenting the results of a long implementation task
- After a build, deploy, or test run completes
- When you've reached a decision point and need input before continuing
- After creating a PR or filing a report the user needs to act on

**Do not use** for quick back-and-forth replies or when the user is clearly already
watching the conversation. The Stop hook already sends a "Your turn!" alert
automatically; this skill is for when you want a **custom message** or when you are
calling it explicitly from within another skill or workflow.

## Example

At the end of a feature implementation:

```bash
curl -sf "${CLAWDMETER_URL:-http://clawdmeter.local}/clawd" \
  -X POST \
  --data-urlencode "beat=attention" \
  --data-urlencode "msg=Ready for review" \
  -H "Authorization: Bearer ${CLAWDMETER_TOKEN}" \
  -o /dev/null || true
```

Then tell the user what you did and what you need from them, as usual.

## Requirements

- The `connect-to-claude-code` skill must be set up (hooks added, env vars configured)
- The device must be on the usage screen for the director to respond
- `CLAWDMETER_URL` and `CLAWDMETER_TOKEN` must be set in `~/.claude/settings.json`
  if the defaults do not work on your network
