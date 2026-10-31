#!/usr/bin/env python3
"""Replay every committed scene in fresh processes; measure budgets/resources.

Usage: validate.py --binary /path/to/harvest_gtest --output validation.json
Round 1 uses manifest order, round 2 reverse order, round 3 fixed shuffled order.
Pass --rounds 1 for budget refresh only. No benchmark source directories needed.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import random
import signal
import stat
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(binary, scene, timeout):
    with tempfile.TemporaryDirectory(prefix='sqlite-measure-') as tmp:
        report = Path(tmp) / 'gtest.json'
        with (Path(tmp) / 'output').open('w+', errors='replace') as log:
            start = time.monotonic()
            child = subprocess.Popen([str(binary), '--gtest_filter=' + scene,
                '--gtest_output=json:' + str(report)], stdout=log,
                stderr=subprocess.STDOUT, start_new_session=True)
            peak_bytes = 0
            while True:
                pid, status, usage = os.wait4(child.pid, os.WNOHANG)
                if pid:
                    child.returncode = os.waitstatus_to_exitcode(status)
                    break
                try:
                    cwd = Path(os.readlink(f'/proc/{child.pid}/cwd'))
                    sizes = {}
                    if cwd.name.startswith('harvest-sqlite-'):
                        for path in cwd.rglob('*'):
                            info = path.stat()
                            if stat.S_ISREG(info.st_mode):
                                sizes[info.st_dev, info.st_ino] = info.st_size
                    # Include writable regular files outside cwd and unlinked
                    # SQLite temp files, which remain visible through /proc.
                    for fd in Path(f'/proc/{child.pid}/fd').iterdir():
                        try:
                            target = os.readlink(fd)
                            if target.startswith(tmp + '/'):
                                continue  # validation stdout/JSON, not scene data
                            info = fd.stat()
                            flags_line = next(line for line in
                                Path(f'/proc/{child.pid}/fdinfo/{fd.name}').read_text().splitlines()
                                if line.startswith('flags:'))
                            flags = int(flags_line.split()[1], 8)
                            if stat.S_ISREG(info.st_mode) and flags & os.O_ACCMODE:
                                sizes[info.st_dev, info.st_ino] = info.st_size
                        except (FileNotFoundError, ProcessLookupError, PermissionError, StopIteration):
                            pass
                    peak_bytes = max(peak_bytes, sum(sizes.values()))
                except (FileNotFoundError, ProcessLookupError, PermissionError):
                    pass
                if time.monotonic() - start > timeout:
                    os.killpg(child.pid, signal.SIGKILL)
                time.sleep(0.002)
            elapsed = time.monotonic() - start
            log.seek(0)
            output = log.read()
        data = json.loads(report.read_text()) if report.exists() else {}
        tests = [t for s in data.get('testsuites', []) for t in s.get('testsuite', [])]
        ok = child.returncode == 0 and len(tests) == 1 and not tests[0].get('failures')
        return {'scene': scene, 'passed': ok, 'seconds': round(elapsed, 6),
                'max_rss_kib': usage.ru_maxrss, 'sampled_peak_file_bytes': peak_bytes,
                'sources': tests[0].get('sources', '') if tests else '',
                **({'output': output[-4000:]} if not ok else {})}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--rounds', type=int, default=3)
    p.add_argument('--jobs', type=int, default=4)
    p.add_argument('--timeout', type=float, default=240)
    args = p.parse_args()
    registry = json.loads((ROOT / 'scenario_manifest.json').read_text())['scenarios']
    expected = ['SqliteTractor.Replay/' + s['id'] for s in registry]
    listed = subprocess.check_output([str(args.binary.resolve()), '--gtest_list_tests'], text=True)
    actual = []
    suite = ''
    for line in listed.splitlines():
        line = line.split('#')[0].rstrip()
        if line and not line.startswith(' '):
            suite = line
        elif line.strip():
            actual.append(suite + line.strip())
    assert actual == expected, 'gtest registry differs from committed scene manifest'
    rounds = []
    for n in range(args.rounds):
        order = expected.copy()
        if n == 1:
            order.reverse()
        elif n >= 2:
            random.Random(1234 + n).shuffle(order)
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            rows = list(pool.map(lambda s: run(args.binary.resolve(), s, args.timeout), order))
        rounds.append(rows)
        args.output.write_text(json.dumps({'rounds': rounds,
            'file_sampling_interval_ms': 2, 'tcl': '8.6',
            'registered_scenarios': len(expected)}, indent=2) + '\n')
        print(f'round {n+1}: {sum(r["passed"] for r in rows)}/{len(rows)} passed', flush=True)
    assert all(r['passed'] for rows in rounds for r in rows), 'scenario failures'


if __name__ == '__main__':
    main()
