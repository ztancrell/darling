#!/usr/bin/env python3
"""Exercise a real launcher against fake servers, without starting a container."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

launcher = str(Path(sys.argv[1]).resolve())
baseline = str(Path(sys.argv[2]).resolve()) if len(sys.argv) > 2 else None
with tempfile.TemporaryDirectory(prefix='darling-startup-exit-') as directory:
    root = Path(directory)
    install = root / 'install'
    (install / 'bin').mkdir(parents=True)
    (install / 'libexec/darling').mkdir(parents=True)
    server = install / 'bin/darlingserver'
    env = os.environ.copy()
    env.pop('DARLING_PROFILE', None)
    env.update(DARLING_INSTALL_PREFIX=str(install), DARLING_NONROOT='1')
    # Only these fake scripts are executed, never a real darlingserver.
    cases = [
        ('exit', 'exit 23', 'exited with status 23'),
        ('success-exit', 'exit 0', 'exited with status 0'),
        ('signal', 'kill -TERM $$', 'died from signal 15'),
        ('delayed', 'sleep 0.15\nexit 42', 'exited with status 42'),
        ('exec-missing', None, 'exited with status 1'),
    ]
    for label, body, expected in cases:
        if server.exists(): server.unlink()
        if body is not None:
            server.write_text('#!/bin/sh\n' + body + '\n')
            server.chmod(0o755)
        env['DPREFIX'] = str(root / ('prefix-' + label))
        start = time.monotonic()
        result = subprocess.run([launcher, 'shell', 'uname'], env=env,
                                capture_output=True, text=True, timeout=3)
        elapsed = time.monotonic() - start
        assert result.returncode == 1, (label, result)
        assert expected in result.stderr, (label, result.stderr)
        assert 'Timed out waiting' not in result.stderr, result.stderr
        print(f'PASS {label}: {elapsed:.3f}s; {expected}')
    if baseline:
        server.write_text('#!/bin/sh\nexit 23\n'); server.chmod(0o755)
        env['DPREFIX'] = str(root / 'baseline-prefix')
        try:
            result = subprocess.run([baseline, 'shell', 'uname'], env=env,
                                    capture_output=True, text=True, timeout=2)
        except subprocess.TimeoutExpired:
            # Safe here: the fake server has exited and no guest was launched.
            print('PASS negative control: old launcher still waiting after 2 seconds')
        else:
            raise AssertionError(('baseline unexpectedly completed', result))
