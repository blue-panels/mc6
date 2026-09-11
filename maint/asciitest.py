#!/usr/bin/env python3

from pathlib import Path

MC_SOURCE_ROOT = Path(__file__).parent.parent

SOURCE_DIRS = ("lib", "src")

# A character from outside ASCII in the sources is a typo, most often a dash
# pasted in from somewhere else.  Test fixtures are not scanned: theirs are there
# on purpose.

if not (MC_SOURCE_ROOT / "lib/global.h").exists():
    raise FileNotFoundError("cannot read lib/global.h")

found = []

for directory in SOURCE_DIRS:
    for source in sorted((MC_SOURCE_ROOT / directory).glob("**/*.[ch]")):
        name = str(source.relative_to(MC_SOURCE_ROOT))

        for number, line in enumerate(source.read_bytes().split(b"\n"), start=1):
            try:
                line.decode("ascii")
            except UnicodeDecodeError:
                found.append(f"{name}:{number}: {line.decode('utf-8', 'replace').strip()}")

if found:
    raise AssertionError("sources are ASCII, these lines are not:\n" + "\n".join(found))
