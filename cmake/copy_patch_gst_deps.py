#! /usr/bin/env python3
import os
import subprocess
import shutil
import sys
from pathlib import Path

OTOOL_CMD = "otool"
INT_CMD = "install_name_tool"


def run(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return []
    return result.stdout.strip().splitlines()


def is_system_lib(path):
    return path.startswith(("/System", "/usr/lib"))


def get_deps(dylib):
    libs = run([OTOOL_CMD, "-L", dylib])
    libs = libs[1:]
    libs = [lib[1:] for lib in libs]
    libs = [lib.split(" ", 1)[0] for lib in libs]
    return libs


def search_dirs():
    dirs = [
        Path("/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/"),
        # Path("/opt/homebrew/lib"),
        # Path("/usr/local/Cellar"),
    ]
    return [d for d in dirs if d.exists()]


def find_dylib_on_disk(name):
    """Search by basename ONLY."""
    for d in search_dirs():
        candidate = d / name
        if candidate.exists():
            return candidate
    return None


def copy_and_patch(src, dst_dir, visited):
    src_path = Path(src).resolve()

    if not src_path.exists():
        return
    if src_path.name in visited:
        return
    visited.add(src_path.name)

    deps = get_deps(src_path)

    for dep in deps:
        if is_system_lib(dep):
            continue

        dylib = Path(dep).name
        dep_path = find_dylib_on_disk(dylib)

        if not dep_path:
            continue

        dest_dep_path = dst_dir / dep_path.name
        if not dest_dep_path.exists():
            print(f"Copying dependency {dep_path} -> {dest_dep_path}")
            shutil.copy2(dep_path, dest_dep_path)

        copy_and_patch(dep_path, dst_dir, visited)


def main():
    if len(sys.argv) != 3:
        print("Usage: copy_patch_gst_deps.py <target_dylib> <bundle_dir>")
        sys.exit(1)

    target = Path(sys.argv[1])
    bundle_dir = Path(sys.argv[2])
    frameworks_dir = bundle_dir / "Contents" / "Frameworks"

    visited = set()

    if target.is_file():
        copy_and_patch(target, frameworks_dir, visited)
    else:
        for dylib in target.glob("*.dylib"):
            if dylib.is_file():
                print(f"Patching dylib: {dylib}")
                copy_and_patch(dylib, frameworks_dir, visited)

    print(f"Done patching {target}")


if __name__ == "__main__":
    main()
