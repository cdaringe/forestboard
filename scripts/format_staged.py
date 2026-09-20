#!/usr/bin/env python3
"""Format staged C/C++ blobs without staging unrelated working-tree edits."""
import pathlib
import shutil
import subprocess
import sys


def git(*args, data=None):
    return subprocess.check_output(["git", *args], input=data)


def main():
    formatter = shutil.which("clang-format")
    if formatter is None:
        sys.exit("clang-format is required to commit. Install it, then retry.")
    root = pathlib.Path(git("rev-parse", "--show-toplevel").decode().strip())
    paths = git("diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z").split(b"\0")
    for raw_path in paths:
        if not raw_path:
            continue
        name = raw_path.decode("utf-8", errors="surrogateescape")
        if pathlib.Path(name).suffix not in {".c", ".cpp", ".h", ".hpp"}:
            continue
        entry = git("ls-files", "--stage", "-z", "--", name).split(b"\t", 1)[0]
        mode, blob, stage = entry.split()
        if stage != b"0" or mode not in {b"100644", b"100755"}:
            continue
        original = git("cat-file", "blob", blob.decode())
        formatted = subprocess.check_output(
            [formatter, "--style=file", f"--assume-filename={root / name}"], input=original
        )
        if formatted == original:
            continue
        working_path = root / name
        # Only synchronize a clean working copy. Partially staged files remain intact.
        update_working = not working_path.is_symlink() and working_path.exists() and working_path.read_bytes() == original
        new_blob = git("hash-object", "-w", "--stdin", data=formatted).decode().strip()
        git("update-index", "--cacheinfo", mode.decode(), new_blob, name)
        if update_working:
            working_path.write_bytes(formatted)
        print(f"Formatted staged {name}")


if __name__ == "__main__":
    main()
