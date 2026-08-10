#!/usr/bin/env python3
"""Export Kontex recipes referenced by a consumer Conan recipe."""

import argparse
import ast
import subprocess
from pathlib import Path


DEPENDENCY_METHODS = {"requires", "tool_requires", "test_requires"}


def requirement_references(consumer: Path):
    tree = ast.parse(consumer.read_text(encoding="utf-8"), filename=str(consumer))
    seen = set()
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        function = node.func
        if not (
            isinstance(function, ast.Attribute)
            and isinstance(function.value, ast.Name)
            and function.value.id == "self"
            and function.attr in DEPENDENCY_METHODS
        ):
            continue
        if not (
            node.args
            and isinstance(node.args[0], ast.Constant)
            and isinstance(node.args[0].value, str)
        ):
            raise SystemExit(
                f"{consumer}:{node.lineno}: dependency reference must be a string literal"
            )
        reference = node.args[0].value
        if reference not in seen:
            seen.add(reference)
            yield reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--consumer", type=Path, required=True)
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not args.consumer.is_file():
        raise SystemExit(f"Consumer recipe not found: {args.consumer}")
    recipes_root = args.index / "recipes"
    if not recipes_root.is_dir():
        raise SystemExit(f"Conan recipe index not found: {recipes_root}")

    for reference in requirement_references(args.consumer):
        name, separator, version = reference.partition("/")
        if not separator:
            continue
        recipe = recipes_root / name / version / "conanfile.py"
        if recipe.is_file():
            if args.dry_run:
                print(f"Would export {name}/{version}")
            else:
                print(f"Exporting {name}/{version}", flush=True)
                subprocess.run(["conan", "export", str(recipe.parent)], check=True)
        elif (recipes_root / name).is_dir():
            raise SystemExit(f"Kontex recipe not found for {name}/{version}")


if __name__ == "__main__":
    main()
