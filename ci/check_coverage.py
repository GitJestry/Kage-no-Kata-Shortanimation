#!/usr/bin/env python3
"""Enforce committed coverage floors and coverage of changed C++ lines."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

EXEMPT_CHANGED_SOURCES = {
    "src/assets/tinygltf_implementation.cpp",
    "src/main.cpp",
}


def changed_lines(base: str) -> dict[str, set[int]]:
    command = [
        "git", "diff", "--unified=0", "--diff-filter=ACMR", f"{base}...HEAD",
        "--", "src",
    ]
    diff = subprocess.run(command, check=True, text=True, capture_output=True).stdout
    result: dict[str, set[int]] = {}
    current = ""
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current = line[6:]
            if Path(current).suffix in {".cpp", ".hpp"}:
                result.setdefault(current, set())
            else:
                current = ""
            continue
        match = re.match(r"@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", line)
        if current and match:
            start = int(match.group(1))
            count = int(match.group(2) or "1")
            result[current].update(range(start, start + count))
    return result


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: check_coverage.py COVERAGE_JSON THRESHOLDS_JSON BASE_REF")
        return 2
    coverage = json.loads(Path(sys.argv[1]).read_text())
    thresholds = json.loads(Path(sys.argv[2]).read_text())
    data = coverage["data"][0]
    totals = data["totals"]
    line_percent = float(totals["lines"]["percent"])
    branch_percent = float(totals["branches"]["percent"])
    failures: list[str] = []
    if line_percent < thresholds["line_percent"]:
        failures.append(
            f"global line coverage {line_percent:.2f}% < {thresholds['line_percent']:.2f}%"
        )
    if branch_percent < thresholds["branch_percent"]:
        failures.append(
            f"global branch coverage {branch_percent:.2f}% < {thresholds['branch_percent']:.2f}%"
        )

    changed = changed_lines(sys.argv[3])
    instrumented = 0
    covered = 0
    reported_sources: set[str] = set()
    root = Path.cwd().resolve()
    for file_data in data["files"]:
        path = Path(file_data["filename"]).resolve()
        try:
            relative = path.relative_to(root).as_posix()
        except ValueError:
            continue
        reported_sources.add(relative)
        wanted = changed.get(relative, set())
        if not wanted:
            continue
        counts: dict[int, int] = {}
        for segment in file_data["segments"]:
            line, _column, count, has_count = segment[:4]
            if has_count:
                counts[line] = max(counts.get(line, 0), int(count))
        for line in wanted & counts.keys():
            instrumented += 1
            covered += counts[line] > 0
    required_sources = {
        path for path in changed
        if path.endswith(".cpp") and path not in EXEMPT_CHANGED_SOURCES
    }
    for missing in sorted(required_sources - reported_sources):
        failures.append(f"changed source is absent from coverage report: {missing}")
    changed_percent = 100.0 if instrumented == 0 else covered * 100.0 / instrumented
    if changed_percent < thresholds["changed_line_percent"]:
        failures.append(
            f"changed-line coverage {changed_percent:.2f}% < "
            f"{thresholds['changed_line_percent']:.2f}% ({covered}/{instrumented})"
        )
    print(
        f"coverage: lines={line_percent:.2f}% branches={branch_percent:.2f}% "
        f"changed={changed_percent:.2f}% ({covered}/{instrumented})"
    )
    for failure in failures:
        print(f"error: {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
