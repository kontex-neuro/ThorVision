#!/usr/bin/env python3
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


def is_runtime_ref(path):
    return path.startswith(("@rpath", "@loader_path", "@executable_path"))


def has_rpath(binary, rpath):
    """Return True if the binary already has the given rpath."""
    lines = run([OTOOL_CMD, "-l", binary])
    for i, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH":
            if i + 2 < len(lines) and rpath in lines[i + 2]:
                return True
    return False


def get_deps(dylib):
    deps = []
    lines = run([OTOOL_CMD, "-L", dylib])
    for line in lines[1:]:
        dep = line.strip().split(" ")[0]
        if dep and not is_system_lib(dep) and dep != dylib:
            deps.append(dep)
    return deps


def ensure_rpath(binary, rpath):
    """Add an rpath only if it's not already present."""
    if not has_rpath(binary, rpath):
        subprocess.run([INT_CMD, "-add_rpath", rpath, binary], check=False)


def copy_and_patch(src, dst_dir, visited):
    src_path = Path(src)
    src_name = src_path.name
    if not src_path.exists() or is_runtime_ref(str(src_path)):
        # Skip unresolved runtime references
        return
    if src_path in visited:
        return
    visited.add(src_path)

    dst_path = Path(dst_dir) / src_path.name
    if not dst_path.exists():
        print(f"Copying {src_path} → {dst_path}")
        shutil.copy2(src_path, dst_path)

    subprocess.run(
        [INT_CMD, "-id", f"@rpath/{src_name}", str(dst_path)],
        check=False,
    )

    # Patch its dependencies recursively
    deps = get_deps(str(dst_path))
    for dep in deps:
        if is_system_lib(dep) or is_runtime_ref(dep):
            continue
        dep_name = Path(dep).name
        new_path = f"@rpath/{dep_name}"

        subprocess.run([INT_CMD, "-change", dep, new_path, str(dst_path)], check=False)

        ensure_rpath(str(dst_path), "@executable_path/../Frameworks")

        copy_and_patch(dep, dst_dir, visited)


def main():
    if len(sys.argv) != 3:
        print("Usage: copy_patch_gst_deps.py <target_dylib> <bundle_dir>")
        sys.exit(1)

    target = Path(sys.argv[1])
    bundle_dir = Path(sys.argv[2])
    frameworks_dir = bundle_dir / "Contents" / "Frameworks"

    # Set plugin ID to @rpath
    dep_name = target.name
    subprocess.run([INT_CMD, "-id", f"@rpath/{dep_name}", str(target)], check=False)

    ensure_rpath(str(target), "@executable_path/../Frameworks")

    visited = set()
    deps = get_deps(str(target))
    for dep in deps:
        if is_system_lib(dep) or is_runtime_ref(dep):
            continue
        copy_and_patch(dep, frameworks_dir, visited)

    for dep in deps:
        if is_system_lib(dep) or is_runtime_ref(dep):
            continue
        dep_name = Path(dep).name
        subprocess.run(
            [INT_CMD, "-change", dep, f"@rpath/{dep_name}", str(target)],
            check=False,
        )

    print(f"Done patching {target}")


if __name__ == "__main__":
    main()
