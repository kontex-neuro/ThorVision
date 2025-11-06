#!python
import os
import shutil
import sys
from pathlib import Path
import pefile


def is_system_dll(path):
    """Skip system DLLs (Windows ones)."""
    sys_dirs = [os.environ.get("SystemRoot", "C:\\Windows") + "\\System32"]
    path_lower = str(path).lower()
    return any(path_lower.startswith(d.lower()) for d in sys_dirs)


def find_dependencies(dll_path):
    """Return a list of dependent DLL names (not full paths)."""
    try:
        pe = pefile.PE(str(dll_path))
        deps = [entry.dll.decode("utf-8") for entry in pe.DIRECTORY_ENTRY_IMPORT]
        return deps
    except Exception as e:
        print(f"Warning: Failed to read dependencies for {dll_path}: {e}")
        return []


def find_dll_on_disk(dll_name, search_dirs):
    """Try to find a DLL in given search directories."""
    for d in search_dirs:
        candidate = d / dll_name
        if candidate.exists():
            return candidate
    return None


def copy_dependencies(target, search_dirs, dest_dir, visited, skip_root=False):
    """Recursively copy DLLs that target depends on."""
    target = Path(target).resolve()
    if target in visited:
        return
    visited.add(target)

    # Copy the target DLL itself unless skipping root
    dest_target_path = dest_dir / target.name
    if not skip_root and not dest_target_path.exists():
        print(f"Copying target DLL {target} -> {dest_target_path}")
        shutil.copy2(target, dest_target_path)

    deps = find_dependencies(target)
    print(f"Dependencies of {target}: {deps}")

    for dep in deps:
        dep_path = find_dll_on_disk(dep, search_dirs)
        if not dep_path:
            continue
        if is_system_dll(dep_path):
            print(f"Skipping system DLL dependency: {dep_path}")
            continue

        dest_path = dest_dir / dep_path.name
        if not dest_path.exists():
            print(f"Copying dependency {dep_path} -> {dest_path}")
            shutil.copy2(dep_path, dest_path)

        # Recurse for this dependency's own dependencies
        copy_dependencies(dep_path, search_dirs, dest_dir, visited)


def main():
    if len(sys.argv) != 5:
        print(
            "Usage: copy_gst_deps_windows.py <target_dir_or_dll> <gst_lib_dir> <install_dir> <vcpkg_lib_dir>"
        )
        sys.exit(1)

    target_input = Path(sys.argv[1])
    gst_lib_dir = Path(sys.argv[2])
    dest_dir = Path(sys.argv[3])
    vcpkg_lib_dir = Path(sys.argv[4])

    print(f"Target input: {target_input}")
    print(f"GStreamer lib dir: {gst_lib_dir}")
    print(f"Destination dir: {dest_dir}")
    print(f"Vcpkg lib dir: {vcpkg_lib_dir}")

    if not target_input.exists():
        print(f"Target not found: {target_input}")
        sys.exit(1)
    if not gst_lib_dir.exists():
        print(f"GStreamer lib dir not found: {gst_lib_dir}")
        sys.exit(1)

    dest_dir.mkdir(parents=True, exist_ok=True)

    search_dirs = [
        gst_lib_dir,
        gst_lib_dir / "lib" / "gstreamer-1.0",
        gst_lib_dir / "bin",
        vcpkg_lib_dir,
        target_input.parent,
    ]

    visited = set()

    # Handle both single file and directory
    if target_input.is_file():
        # Skip copying the plugin itself, only copy dependencies
        copy_dependencies(target_input, search_dirs, dest_dir, visited, skip_root=True)
    elif target_input.is_dir():
        for dll_path in target_input.glob("*.dll"):
            if dll_path.is_file():
                print(f"Processing DLL: {dll_path}")
                copy_dependencies(
                    dll_path, search_dirs, dest_dir, visited, skip_root=True
                )
    else:
        print(f"Invalid target: {target_input}")
        sys.exit(1)

    print("Done copying dependencies.")


if __name__ == "__main__":
    main()
