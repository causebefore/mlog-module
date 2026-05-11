#!/usr/bin/env python3
"""Agent-friendly serial logger for MLog output."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from collections import deque
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional


ANSI_RE = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
DEFAULT_TIMEOUT = 0.2
DEFAULT_RECONNECT_INTERVAL = 2.0
STALE_SECONDS = 5.0


@dataclass(frozen=True)
class SessionPaths:
    root: Path
    session: str
    logs_dir: Path
    state_dir: Path
    raw_log: Path
    jsonl_log: Path
    meta_json: Path
    state_json: Path
    stop_request: Path


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def sanitize_token(value: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_.-]+", "_", value.strip())
    return cleaned.strip("._-") or "serial"


def make_session_name(port: str) -> str:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"{stamp}_{sanitize_token(port)}"


def resolve_root(root: Optional[str]) -> Path:
    return Path(root).resolve() if root else Path.cwd().resolve()


def build_session_paths(root: Path, session: str) -> SessionPaths:
    logs_dir = root / "logs"
    state_dir = root / ".mlog"
    prefix = f"mlog_{session}"
    return SessionPaths(
        root=root,
        session=session,
        logs_dir=logs_dir,
        state_dir=state_dir,
        raw_log=logs_dir / f"{prefix}.raw.log",
        jsonl_log=logs_dir / f"{prefix}.jsonl",
        meta_json=logs_dir / f"{prefix}.meta.json",
        state_json=state_dir / f"{prefix}.state.json",
        stop_request=state_dir / f"{prefix}.stop",
    )


def ensure_dirs(paths: SessionPaths) -> None:
    paths.logs_dir.mkdir(parents=True, exist_ok=True)
    paths.state_dir.mkdir(parents=True, exist_ok=True)


def atomic_write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    tmp.replace(path)


def read_json(path: Path) -> Optional[dict[str, Any]]:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return None


def strip_line_for_text(raw_line: bytes) -> str:
    line = raw_line.rstrip(b"\r\n")
    line = ANSI_RE.sub(b"", line)
    return line.decode("utf-8", errors="replace")


def public_paths(paths: SessionPaths) -> dict[str, str]:
    return {
        "raw": str(paths.raw_log),
        "jsonl": str(paths.jsonl_log),
        "meta": str(paths.meta_json),
        "state": str(paths.state_json),
    }


class SessionWriter:
    def __init__(self, paths: SessionPaths, port: str, baud: int, source: str) -> None:
        ensure_dirs(paths)
        self.paths = paths
        self.port = port
        self.baud = baud
        self.source = source
        self.started_at = utc_now_iso()
        self.raw_file = paths.raw_log.open("ab")
        self.jsonl_file = paths.jsonl_log.open("a", encoding="utf-8", newline="\n")
        self.pending = bytearray()
        self.pending_offset = self.raw_file.tell()
        self.bytes_written = self.raw_file.tell()
        self.lines = 0
        self.reconnects = 0
        self.closed = False
        self.meta: dict[str, Any] = {
            "session": paths.session,
            "status": "running",
            "port": port,
            "baud": baud,
            "source": source,
            "pid": os.getpid(),
            "started_at": self.started_at,
            "updated_at": self.started_at,
            "stopped_at": None,
            "bytes": self.bytes_written,
            "lines": self.lines,
            "reconnects": self.reconnects,
            "error": None,
            "files": public_paths(paths),
        }
        self.write_meta("running")

    def write_meta(self, status: Optional[str] = None, error: Optional[str] = None) -> None:
        if status is not None:
            self.meta["status"] = status
        if error is not None:
            self.meta["error"] = error
        self.meta["updated_at"] = utc_now_iso()
        self.meta["bytes"] = self.bytes_written
        self.meta["lines"] = self.lines
        self.meta["reconnects"] = self.reconnects
        atomic_write_json(self.paths.meta_json, self.meta)
        atomic_write_json(self.paths.state_json, self.meta)

    def write_bytes(self, data: bytes) -> None:
        if not data:
            return
        raw_offset = self.bytes_written
        self.raw_file.write(data)
        self.raw_file.flush()
        self.bytes_written += len(data)
        if not self.pending:
            self.pending_offset = raw_offset
        self.pending.extend(data)
        self._flush_complete_lines()

    def _flush_complete_lines(self) -> None:
        while True:
            try:
                newline_index = self.pending.index(0x0A)
            except ValueError:
                return
            line_len = newline_index + 1
            line = bytes(self.pending[:line_len])
            self._write_record(line, self.pending_offset, complete=True)
            del self.pending[:line_len]
            self.pending_offset += line_len

    def _write_record(self, raw_line: bytes, raw_offset: int, complete: bool) -> None:
        self.lines += 1
        record = {
            "seq": self.lines,
            "pc_time": utc_now_iso(),
            "text": strip_line_for_text(raw_line),
            "raw_offset": raw_offset,
            "raw_len": len(raw_line),
            "complete": complete,
        }
        self.jsonl_file.write(json.dumps(record, ensure_ascii=False) + "\n")
        self.jsonl_file.flush()

    def note_reconnect(self, error: Optional[str] = None) -> None:
        self.reconnects += 1
        self.write_meta("reconnecting", error=error)

    def close(self, status: str, error: Optional[str] = None) -> None:
        if self.closed:
            return
        if self.pending:
            self._write_record(bytes(self.pending), self.pending_offset, complete=False)
            self.pending.clear()
        self.meta["stopped_at"] = utc_now_iso()
        self.write_meta(status, error=error)
        self.jsonl_file.close()
        self.raw_file.close()
        self.closed = True


def print_json(data: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(data, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def human_or_json(data: dict[str, Any], as_json: bool) -> None:
    if as_json:
        print_json(data)
    else:
        status = data.get("status", "ok" if data.get("ok") else "error")
        detail = data.get("message") or data.get("error") or ""
        sys.stdout.write(f"{status}: {detail}\n" if detail else f"{status}\n")


def serial_module():
    try:
        import serial  # type: ignore
        import serial.tools.list_ports  # type: ignore
    except ImportError as exc:
        raise RuntimeError("pyserial is required; run: pip install -r requirements.txt") from exc
    return serial


def open_serial(port: str, baud: int, timeout: float):
    serial = serial_module()
    if "://" in port:
        return serial.serial_for_url(port, baudrate=baud, timeout=timeout)
    return serial.Serial(port=port, baudrate=baud, timeout=timeout)


def capture_loop(args: argparse.Namespace, paths: SessionPaths, foreground: bool) -> int:
    writer = SessionWriter(paths, args.port, args.baud, args.port)
    stop_requested = False
    ser = None
    last_meta = 0.0
    try:
        while not stop_requested:
            if paths.stop_request.exists():
                stop_requested = True
                break
            try:
                if ser is None:
                    ser = open_serial(args.port, args.baud, args.timeout)
                    writer.write_meta("running", error=None)
                data = ser.read(args.read_size)
                if data:
                    writer.write_bytes(data)
                    if foreground:
                        try:
                            sys.stdout.buffer.write(data)
                            sys.stdout.buffer.flush()
                        except BrokenPipeError:
                            stop_requested = True
                else:
                    time.sleep(0.01)
                now = time.monotonic()
                if now - last_meta >= 1.0:
                    writer.write_meta("running")
                    last_meta = now
            except KeyboardInterrupt:
                stop_requested = True
            except Exception as exc:
                if ser is not None:
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = None
                writer.note_reconnect(str(exc))
                time.sleep(args.reconnect_interval)
    finally:
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass
        if stop_requested and paths.stop_request.exists():
            try:
                paths.stop_request.unlink()
            except OSError:
                pass
        writer.close("stopped" if stop_requested else "exited")
    return 0


def find_sessions(root: Path) -> list[dict[str, Any]]:
    state_dir = root / ".mlog"
    sessions: list[dict[str, Any]] = []
    for state_path in sorted(state_dir.glob("mlog_*.state.json")):
        meta = read_json(state_path)
        if not meta:
            continue
        meta = enrich_status(meta)
        sessions.append(meta)
    return sessions


def enrich_status(meta: dict[str, Any]) -> dict[str, Any]:
    status = meta.get("status")
    updated_at = meta.get("updated_at")
    if status in {"running", "reconnecting"} and isinstance(updated_at, str):
        try:
            updated = datetime.fromisoformat(updated_at)
            age = datetime.now(timezone.utc) - updated
            if age.total_seconds() > STALE_SECONDS:
                meta = dict(meta)
                meta["status"] = "stale"
        except ValueError:
            pass
    return meta


def select_session(root: Path, session: Optional[str]) -> tuple[Optional[dict[str, Any]], Optional[str]]:
    sessions = find_sessions(root)
    if session:
        for item in sessions:
            if item.get("session") == session:
                return item, None
        return None, f"session not found: {session}"
    if not sessions:
        return None, "no sessions found"
    return sessions[-1], None


def command_ports(args: argparse.Namespace) -> int:
    try:
        serial = serial_module()
        ports = [
            {
                "device": port.device,
                "name": port.name,
                "description": port.description,
                "hwid": port.hwid,
            }
            for port in serial.tools.list_ports.comports()
        ]
    except RuntimeError as exc:
        human_or_json({"ok": False, "error": str(exc)}, args.as_json)
        return 2
    human_or_json({"ok": True, "ports": ports}, args.as_json)
    return 0


def command_list(args: argparse.Namespace) -> int:
    root = resolve_root(args.root)
    human_or_json({"ok": True, "sessions": find_sessions(root)}, args.as_json)
    return 0


def command_status(args: argparse.Namespace) -> int:
    root = resolve_root(args.root)
    meta, error = select_session(root, args.session)
    if error:
        human_or_json({"ok": False, "error": error}, args.as_json)
        return 3
    human_or_json({"ok": True, "session": meta}, args.as_json)
    return 0


def command_stop(args: argparse.Namespace) -> int:
    root = resolve_root(args.root)
    meta, error = select_session(root, args.session)
    if error:
        human_or_json({"ok": False, "error": error}, args.as_json)
        return 3
    session = str(meta["session"])
    paths = build_session_paths(root, session)
    ensure_dirs(paths)
    paths.stop_request.write_text(utc_now_iso() + "\n", encoding="utf-8")
    deadline = time.monotonic() + args.wait
    stopped_meta = meta
    while time.monotonic() < deadline:
        time.sleep(0.1)
        next_meta = read_json(paths.state_json) or read_json(paths.meta_json)
        if next_meta:
            stopped_meta = enrich_status(next_meta)
            if stopped_meta.get("status") not in {"running", "reconnecting"}:
                break
    human_or_json({"ok": True, "session": stopped_meta, "stop_requested": True}, args.as_json)
    return 0


def command_tail(args: argparse.Namespace) -> int:
    root = resolve_root(args.root)
    meta, error = select_session(root, args.session)
    if error:
        human_or_json({"ok": False, "error": error}, args.as_json)
        return 3
    jsonl_path = Path(meta["files"]["jsonl"])
    try:
        tail_lines: deque[str] = deque(maxlen=max(args.lines, 0))
        with jsonl_path.open("r", encoding="utf-8") as jsonl_file:
            for line in jsonl_file:
                tail_lines.append(line.rstrip("\r\n"))
        lines = list(tail_lines)
    except OSError as exc:
        human_or_json({"ok": False, "error": str(exc)}, args.as_json)
        return 4
    if args.as_json:
        print_json({"ok": True, "session": meta.get("session"), "records": [json.loads(line) for line in lines]})
    else:
        for line in lines:
            try:
                sys.stdout.write(json.loads(line).get("text", "") + "\n")
            except json.JSONDecodeError:
                sys.stdout.write(line + "\n")
    return 0


def command_run(args: argparse.Namespace) -> int:
    serial_module()
    root = resolve_root(args.root)
    session = args.session or make_session_name(args.port)
    return capture_loop(args, build_session_paths(root, session), foreground=True)


def command_worker(args: argparse.Namespace) -> int:
    serial_module()
    root = resolve_root(args.root)
    return capture_loop(args, build_session_paths(root, args.session), foreground=False)


def command_start(args: argparse.Namespace) -> int:
    serial_module()
    root = resolve_root(args.root)
    session = args.session or make_session_name(args.port)
    paths = build_session_paths(root, session)
    ensure_dirs(paths)
    if paths.state_json.exists() or paths.meta_json.exists():
        human_or_json({"ok": False, "error": f"session already exists: {session}"}, args.as_json)
        return 5

    cmd = [
        sys.executable,
        str(Path(__file__).resolve()),
        "--root",
        str(root),
        "_worker",
        "--session",
        session,
        "--port",
        args.port,
        "--baud",
        str(args.baud),
        "--timeout",
        str(args.timeout),
        "--reconnect-interval",
        str(args.reconnect_interval),
        "--read-size",
        str(args.read_size),
    ]
    creationflags = 0
    if os.name == "nt":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS
    proc = subprocess.Popen(
        cmd,
        cwd=str(root),
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=creationflags,
        close_fds=True,
    )

    deadline = time.monotonic() + args.start_wait
    meta = None
    while time.monotonic() < deadline:
        time.sleep(0.1)
        meta = read_json(paths.state_json) or read_json(paths.meta_json)
        if meta is not None:
            break
    response = {
        "ok": True,
        "session": session,
        "pid": proc.pid,
        "files": public_paths(paths),
        "state_dir": str(paths.state_dir),
        "meta": enrich_status(meta) if meta else None,
    }
    human_or_json(response, args.as_json)
    return 0


def add_common_serial_args(parser: argparse.ArgumentParser, *, session_required: bool = False) -> None:
    parser.add_argument("--port", required=True, help="Serial port, for example COM3 or loop://")
    parser.add_argument("--baud", required=True, type=int, help="Serial baud rate")
    parser.add_argument(
        "--session",
        required=session_required,
        help="Session name; defaults to timestamp plus port",
    )
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="Serial read timeout in seconds")
    parser.add_argument(
        "--reconnect-interval",
        type=float,
        default=DEFAULT_RECONNECT_INTERVAL,
        help="Delay before reopening the serial port after an error",
    )
    parser.add_argument("--read-size", type=int, default=256, help="Maximum bytes to read per serial call")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Agent-friendly serial logger for MLog output.")
    parser.add_argument("--root", help="Project root for logs/ and .mlog/; defaults to current directory")
    subparsers = parser.add_subparsers(
        dest="command",
        required=True,
        metavar="{ports,run,start,status,stop,list,tail}",
    )

    ports_p = subparsers.add_parser("ports", help="List available serial ports")
    ports_p.add_argument("--json", dest="as_json", action="store_true", help="Print machine-readable JSON")
    ports_p.set_defaults(func=command_ports)

    run_p = subparsers.add_parser("run", help="Run foreground capture")
    add_common_serial_args(run_p)
    run_p.set_defaults(func=command_run)

    start_p = subparsers.add_parser("start", help="Start background capture")
    add_common_serial_args(start_p)
    start_p.add_argument("--json", dest="as_json", action="store_true", help="Print machine-readable JSON")
    start_p.add_argument("--start-wait", type=float, default=1.0, help="Seconds to wait for initial metadata")
    start_p.set_defaults(func=command_start)

    status_p = subparsers.add_parser("status", help="Show session status")
    status_p.add_argument("--session", help="Session name; defaults to newest session")
    status_p.add_argument("--json", dest="as_json", action="store_true", help="Print machine-readable JSON")
    status_p.set_defaults(func=command_status)

    stop_p = subparsers.add_parser("stop", help="Request graceful stop")
    stop_p.add_argument("--session", help="Session name; defaults to newest session")
    stop_p.add_argument("--json", dest="as_json", action="store_true", help="Print machine-readable JSON")
    stop_p.add_argument("--wait", type=float, default=3.0, help="Seconds to wait for the session to stop")
    stop_p.set_defaults(func=command_stop)

    list_p = subparsers.add_parser("list", help="List sessions")
    list_p.add_argument("--json", dest="as_json", action="store_true", help="Print machine-readable JSON")
    list_p.set_defaults(func=command_list)

    tail_p = subparsers.add_parser("tail", help="Print recent JSONL records as text")
    tail_p.add_argument("--session", help="Session name; defaults to newest session")
    tail_p.add_argument("--lines", type=int, default=20, help="Number of records to show")
    tail_p.add_argument("--json", dest="as_json", action="store_true", help="Print machine-readable JSON")
    tail_p.set_defaults(func=command_tail)

    worker_p = subparsers.add_parser("_worker", help=argparse.SUPPRESS)
    add_common_serial_args(worker_p, session_required=True)
    worker_p.set_defaults(func=command_worker)
    subparsers._choices_actions = [
        action for action in subparsers._choices_actions if action.dest != "_worker"
    ]

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.func(args))
    except RuntimeError as exc:
        as_json = bool(getattr(args, "as_json", False))
        human_or_json({"ok": False, "error": str(exc)}, as_json)
        return 2
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
