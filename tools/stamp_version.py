#!/usr/bin/env python3
"""Stamp the release version into pyproject.toml and npm/package.json.

The git tag is the single source of version truth (docs/BUILD.md); this writes it into
both packaging manifests so the wheel, the npm package, and the repo all agree. Edits are
regex line-replacements, so hand-formatting is preserved. Usage: stamp_version.py 1.2.3
(or --selfcheck).
"""
import re
import sys
from pathlib import Path


def stamp_pyproject(text: str, version: str) -> str:
    new, n = re.subn(r'(?m)^version = ".*"', f'version = "{version}"', text, count=1)
    if n != 1:
        raise SystemExit('stamp_version: no `version = "..."` line in pyproject.toml')
    return new


def stamp_package_json(text: str, version: str) -> str:
    new, n = re.subn(r'("version":\s*)"[^"]*"', rf'\g<1>"{version}"', text, count=1)
    if n != 1:
        raise SystemExit('stamp_version: no `"version": "..."` in npm/package.json')
    return new


def _selfcheck() -> None:
    assert stamp_pyproject('a\nversion = "0.1.0"\nb\n', "9.9.9") == 'a\nversion = "9.9.9"\nb\n'
    assert stamp_package_json('{\n  "version": "0.1.0",\n}\n', "9.9.9") == '{\n  "version": "9.9.9",\n}\n'
    for fn, bad in ((stamp_pyproject, "no version here"), (stamp_package_json, "{}")):
        try:
            fn(bad, "1.0.0")
        except SystemExit:
            pass
        else:
            raise AssertionError("expected failure when version is missing")
    print("selfcheck OK")


if __name__ == "__main__":
    if len(sys.argv) != 2 or not sys.argv[1]:
        raise SystemExit("usage: stamp_version.py <version> | --selfcheck")
    if sys.argv[1] == "--selfcheck":
        _selfcheck()
    else:
        ver = sys.argv[1]
        py = Path("pyproject.toml")
        py.write_text(stamp_pyproject(py.read_text(), ver))
        pj = Path("npm/package.json")
        pj.write_text(stamp_package_json(pj.read_text(), ver))
        print(f"stamped version -> {ver} (pyproject.toml, npm/package.json)")
