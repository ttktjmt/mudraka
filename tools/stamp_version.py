#!/usr/bin/env python3
"""Stamp the project version (from the git tag) into pyproject.toml.

The git tag is the single source of version truth for the release (docs/BUILD.md);
this writes it into the wheel/sdist build. npm/package.json is stamped separately by
`npm version` in the release workflow. Usage: stamp_version.py 1.2.3
"""
import re
import sys
from pathlib import Path


def stamp(text: str, version: str) -> str:
    new, n = re.subn(r'(?m)^version = ".*"', f'version = "{version}"', text, count=1)
    if n != 1:
        raise SystemExit('stamp_version: no `version = "..."` line in pyproject.toml')
    return new


def _selfcheck() -> None:
    assert stamp('a\nversion = "0.1.0"\nb\n', "9.9.9") == 'a\nversion = "9.9.9"\nb\n'
    try:
        stamp("no version here", "1.0.0")
    except SystemExit:
        pass
    else:
        raise AssertionError("expected failure when version line is missing")
    print("selfcheck OK")


if __name__ == "__main__":
    if len(sys.argv) != 2 or not sys.argv[1]:
        raise SystemExit("usage: stamp_version.py <version> | --selfcheck")
    if sys.argv[1] == "--selfcheck":
        _selfcheck()
    else:
        p = Path("pyproject.toml")
        p.write_text(stamp(p.read_text(), sys.argv[1]))
        print(f"pyproject.toml version -> {sys.argv[1]}")
