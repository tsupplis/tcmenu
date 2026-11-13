#!/usr/bin/env python3
"""CLI checks using a real pseudo-terminal; run after make."""
import fcntl
import os
import pty
import signal
import struct
import subprocess
import termios
import time
import select
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = str(ROOT / 'tcmenu')

def run(args, expected, keys=b'\n', size=(24, 80), sig=None):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', *size, 0, 0))
    before = termios.tcgetattr(slave)
    proc = subprocess.Popen([BIN, *args], stdin=slave, stdout=slave,
                            stderr=subprocess.PIPE, env={**os.environ, 'TERM': 'xterm-256color', 'LC_ALL': 'en_US.UTF-8'})
    try:
        time.sleep(.15)
        if sig:
            proc.send_signal(sig)
        else:
            os.write(master, keys)
        deadline = time.monotonic() + 3
        while proc.poll() is None and time.monotonic() < deadline:
            if select.select([master], [], [], .05)[0]:
                os.read(master, 65536)
        _, err = proc.communicate(timeout=.5)
        assert proc.returncode == expected, (args, proc.returncode, err)
        after = termios.tcgetattr(slave)
        after[3] &= ~getattr(termios, "PENDIN", 0)
        before[3] &= ~getattr(termios, "PENDIN", 0)
        assert after == before, "terminal modes not restored"
        return err
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        os.close(master)
        os.close(slave)

assert run(['-d', '1', '-e', 'a', 'b'], 1) == b'1\n'
for options in (['-i', '-d', '1'], ['-d', '1', '-i']):
    assert run([*options, '-e', 'a', 'b'], 1) == b'1\n'
assert run(['-e', 'a', 'b'], 0, keys=b'x\n') == b'0\n'
assert run(['-x', '-e', 'a', 'b'], 253, keys=b'x') == b'-3\n'
for theme in ('dark', 'light', 'nocolor', 'simple'):
    run(['--theme', theme, '-t', 'Café 界 é', '-e', 'ééééééééééééé', '界界界'], 0, size=(8, 16))
run(['-e', 'a'], 254, size=(5, 9))
for sig in (signal.SIGINT, signal.SIGTERM):
    assert run(['-e', 'a'], 128 + sig, sig=sig) == b''
for args in (['-d'], ['-d', 'bad', '-e', 'a'], ['-d', '1', '-e', 'a'], ['-f', 'bad', '-e', 'a']):
    run(args, 2)
print('CLI regression checks passed')
