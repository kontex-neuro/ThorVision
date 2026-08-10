import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "export_conan_recipes.py"


class ExportConanRecipesCliTests(unittest.TestCase):
    def test_dry_run_selects_exact_recipes_present_in_index(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            consumer = root / "conanfile.py"
            index = root / "kontex-conan"
            consumer.write_text(
                """
from conan import ConanFile

class Consumer(ConanFile):
    def requirements(self):
        self.requires("spdlog/1.13.0")
        self.requires("libxvc/0.3.2")
        self.requires("xdaqmetadata/0.2.0")
""",
                encoding="utf-8",
            )
            for reference in ("libxvc/0.3.2", "xdaqmetadata/0.2.0"):
                recipe = index / "recipes" / reference / "conanfile.py"
                recipe.parent.mkdir(parents=True)
                recipe.write_text("# fixture\n", encoding="utf-8")

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--consumer",
                    str(consumer),
                    "--index",
                    str(index),
                    "--dry-run",
                ],
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                result.stdout.splitlines(),
                [
                    "Would export libxvc/0.3.2",
                    "Would export xdaqmetadata/0.2.0",
                ],
            )

    def test_fails_when_known_index_package_lacks_requested_version(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            consumer = root / "conanfile.py"
            index = root / "kontex-conan"
            consumer.write_text(
                """
class Consumer:
    def requirements(self):
        self.requires("libxvc/0.4.0")
""",
                encoding="utf-8",
            )
            existing_recipe = index / "recipes" / "libxvc" / "0.3.2" / "conanfile.py"
            existing_recipe.parent.mkdir(parents=True)
            existing_recipe.write_text("# fixture\n", encoding="utf-8")

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--consumer",
                    str(consumer),
                    "--index",
                    str(index),
                    "--dry-run",
                ],
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "Kontex recipe not found for libxvc/0.4.0",
                result.stderr,
            )

    def test_discovers_all_dependency_methods_and_deduplicates_references(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            consumer = root / "conanfile.py"
            index = root / "kontex-conan"
            consumer.write_text(
                """
class Consumer:
    def requirements(self):
        self.requires("runtime/1.0.0")
        self.requires("runtime/1.0.0")

    def build_requirements(self):
        self.tool_requires("tool/2.0.0")

    def test_requirements(self):
        self.test_requires("test-helper/3.0.0")
""",
                encoding="utf-8",
            )
            for reference in ("runtime/1.0.0", "tool/2.0.0", "test-helper/3.0.0"):
                recipe = index / "recipes" / reference / "conanfile.py"
                recipe.parent.mkdir(parents=True)
                recipe.write_text("# fixture\n", encoding="utf-8")

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--consumer",
                    str(consumer),
                    "--index",
                    str(index),
                    "--dry-run",
                ],
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                result.stdout.splitlines(),
                [
                    "Would export runtime/1.0.0",
                    "Would export tool/2.0.0",
                    "Would export test-helper/3.0.0",
                ],
            )

    def test_fails_for_dynamically_constructed_dependency_reference(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            consumer = root / "conanfile.py"
            index = root / "kontex-conan"
            consumer.write_text(
                """
class Consumer:
    def requirements(self):
        version = "0.4.0"
        self.requires(f"libxvc/{version}")
""",
                encoding="utf-8",
            )
            (index / "recipes").mkdir(parents=True)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--consumer",
                    str(consumer),
                    "--index",
                    str(index),
                    "--dry-run",
                ],
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "dependency reference must be a string literal",
                result.stderr,
            )
            self.assertIn(f"{consumer}:5", result.stderr)


if __name__ == "__main__":
    unittest.main()
