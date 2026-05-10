---
name: mlog-serial-control
description: Use when collecting, controlling, reading, or troubleshooting MLog serial logs with tools/mlog_serial_logger.py. Trigger for MLog serial capture, UART log recording, JSONL log reading, session status, or stopping a capture.
---

# MLog Serial Control

Use the repository script `tools/mlog_serial_logger.py` to collect MLog UART output. Prefer machine-readable commands and files.

## Setup

Install the only runtime dependency if needed:

```bash
pip install -r requirements.txt
```

## Agent Workflow

1. List serial ports when the user did not provide one:

```bash
python tools/mlog_serial_logger.py ports --json
```

2. Start a capture with explicit port and baud:

```bash
python tools/mlog_serial_logger.py start --port COM3 --baud 115200 --json
```

3. Read the returned `files.meta` first, then read `files.jsonl` for log content. Prefer `.jsonl` over console output. Use `.raw.log` only when exact bytes are needed.

4. Check status:

```bash
python tools/mlog_serial_logger.py status --json
```

5. Stop capture before finishing:

```bash
python tools/mlog_serial_logger.py stop --json
```

## Rules

- Always use `--json` for agent control commands.
- Do not parse human-readable stdout when JSON is available.
- Treat each JSONL record's `text` field as the cleaned log line.
- Use `raw_offset` and `raw_len` to map a JSONL record back to `.raw.log`.
- If status is `stale`, report that the capture process likely stopped unexpectedly and inspect `meta.json`.
