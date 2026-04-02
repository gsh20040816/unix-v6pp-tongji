#!/usr/bin/env python3
import base64
import os
import re
import selectors
import shlex
import signal
import subprocess
import sys


SOURCE_SUFFIX_RE = re.compile(
    r'(?P<prefix>(?:"|^|[\s=]))'
    r'(?P<path>('
    r'/[^:\n"]+'
    r'|[A-Za-z]:[/\\][^:\n"]+'
    r'))'
    r'[/\\](?P<file>[^/\\:\n"]+\.(?:c|cc|cpp|cxx|h|hpp))'
    r'(?P<line>:\d+)?'
    r'(?P<suffix>"|$|[\s,])'
)
WINDOWS_DEBUGGER_RE = re.compile(r"^([A-Za-z]:\\.*?\.exe)(?:\s+(.+))?$")


def split_debugger_and_args(argv):
    if not argv:
        raise SystemExit("usage: pipe-win-gdb.sh <remote-debugger> [args...]")

    debugger = argv[0]
    rest = list(argv[1:])

    match = WINDOWS_DEBUGGER_RE.match(debugger)
    if match:
        debugger = match.group(1)
        extra = match.group(2)
        if extra:
            rest = shlex.split(extra, posix=True) + rest

    return debugger, rest


def ps_quote(value):
    return "'" + value.replace("'", "''") + "'"


def build_remote_command(debugger, args):
    ps_args = ", ".join(ps_quote(arg) for arg in args)
    ps_script = f"$argsList = @({ps_args}); & {ps_quote(debugger)} @argsList"
    encoded = base64.b64encode(ps_script.encode("utf-16le")).decode("ascii")
    return ["powershell.exe", "-NoProfile", "-EncodedCommand", encoded]


def rewrite_line(line):
    if "/home/" not in line and not re.search(r'[A-Za-z]:[/\\]', line):
        return line

    def replace_source(match):
        return f"{match.group('prefix')}{match.group('file')}{match.group('line') or ''}{match.group('suffix')}"

    rewritten = SOURCE_SUFFIX_RE.sub(replace_source, line)
    return rewritten


def main():
    remote = os.environ.get("OOS_WIN_HOST", "win")
    debugger, args = split_debugger_and_args(sys.argv[1:])
    remote_command = build_remote_command(debugger, args)

    child = subprocess.Popen(
        ["ssh", "-o", "StrictHostKeyChecking=accept-new", remote, *remote_command],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0,
    )

    def forward_signal(signum, _frame):
        if child.poll() is None:
            child.send_signal(signum)

    signal.signal(signal.SIGINT, forward_signal)
    signal.signal(signal.SIGTERM, forward_signal)

    selector = selectors.DefaultSelector()
    selector.register(sys.stdin.buffer, selectors.EVENT_READ, "stdin")
    selector.register(child.stdout, selectors.EVENT_READ, "stdout")
    selector.register(child.stderr, selectors.EVENT_READ, "stderr")

    stdin_buffer = b""

    while selector.get_map():
        for key, _ in selector.select():
            if key.data == "stdin":
                chunk = os.read(sys.stdin.fileno(), 4096)
                if not chunk:
                    selector.unregister(sys.stdin.buffer)
                    if child.stdin:
                        child.stdin.close()
                    continue

                stdin_buffer += chunk
                while b"\n" in stdin_buffer:
                    line, stdin_buffer = stdin_buffer.split(b"\n", 1)
                    text = line.decode("utf-8", errors="replace")
                    rewritten = rewrite_line(text) + "\n"
                    if child.stdin:
                        child.stdin.write(rewritten.encode("utf-8"))
                        child.stdin.flush()
            else:
                stream = child.stdout if key.data == "stdout" else child.stderr
                chunk = stream.read1(4096) if hasattr(stream, "read1") else stream.read(4096)
                if not chunk:
                    selector.unregister(stream)
                    continue

                target = sys.stdout.buffer if key.data == "stdout" else sys.stderr.buffer
                target.write(chunk)
                target.flush()

        if child.poll() is not None and len(selector.get_map()) <= 1:
            break

    if child.poll() is None:
        child.wait()

    return child.returncode


if __name__ == "__main__":
    raise SystemExit(main())
