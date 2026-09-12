#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measure a separate capture-only probe; never change desktop audio routing."""
import argparse
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("probe", type=Path)
parser.add_argument("--seconds", type=int, choices=range(1, 121), default=15, metavar="1..120")
parser.add_argument("--unavailable", action="store_true", help="use a private empty runtime directory with no audio server")
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="luma-resources-") as runtime:
    environment = os.environ.copy()
    if args.unavailable:
        environment.update(PIPEWIRE_RUNTIME_DIR=runtime, PIPEWIRE_REMOTE="pipewire-0")
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.monotonic()
    result = subprocess.run([str(args.probe.resolve()), "--seconds", str(args.seconds)],
                            env=environment, capture_output=True, text=True, timeout=args.seconds + 15)
    elapsed = time.monotonic() - start
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    cpu = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
    print(json.dumps({
        "scenario": "unavailable server" if args.unavailable else "current default output",
        "wall_seconds": round(elapsed, 3),
        "cpu_seconds": round(cpu, 4),
        "cpu_percent_one_core": round(100 * cpu / elapsed, 3),
        "voluntary_context_switches": after.ru_nvcsw - before.ru_nvcsw,
        "involuntary_context_switches": after.ru_nivcsw - before.ru_nivcsw,
        "peak_rss_kib": after.ru_maxrss,
        "probe_exit_code": result.returncode,
        "probe_output": result.stdout.splitlines(),
        "probe_errors": result.stderr.splitlines(),
    }, indent=2))
    raise SystemExit(result.returncode)
