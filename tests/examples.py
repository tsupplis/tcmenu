#!/usr/bin/env python3
"""Exercise the examples with a controlling terminal."""
import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import termios
import time

root = Path(__file__).resolve().parents[1]
for script, args, keys, expected in (
    ('menu-actions.sh', [], b'\n', b'System information:'),
    ('menu-actions.sh', [], b'\x1bOB\n', b'Files in '),
    ('menu-actions.sh', [], b'x', b'Cancelled.'),
    ('default-action.sh', ['--interactive'], b'\n', b'Files in '),
):
    pid, master = pty.fork()
    if pid == 0:
        os.environ['TERM'] = 'xterm-256color'
        os.chdir('/tmp')
        os.execv(str(root / 'examples' / script), [script, *args])
    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 80, 0, 0))
    output = b''
    status = None
    try:
        time.sleep(.15)
        os.write(master, keys)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if select.select([master], [], [], .05)[0]:
                try:
                    data = os.read(master, 65536)
                    output += data
                    if not data:
                        break
                except OSError as exc:
                    if exc.errno != errno.EIO:
                        raise
                    break
        for _ in range(50):
            child, status = os.waitpid(pid, os.WNOHANG)
            if child:
                break
            time.sleep(.01)
        assert child == pid, ('example timed out', output)
        assert os.waitstatus_to_exitcode(status) == 0, output
        assert expected in output, output
    finally:
        if status is None or child == 0:
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
        os.close(master)
print('Example invocation checks passed')
