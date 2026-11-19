"""Require the installed application to render its first frame with its own plugins."""

import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory


def main():
    executable = Path(sys.argv[1]).resolve(strict=True)
    windows = sys.platform == "win32"
    plugin_dir = executable.parent.parent / ("plugins/gstreamer" if windows else "PlugIns/gstreamer")
    plugin = plugin_dir / ("gstqt6d3d11.dll" if windows else "libgstqml6.dylib")
    if not plugin.is_file():
        raise SystemExit(f"Missing packaged GStreamer plugin: {plugin}")
    env = {
        key: value for key, value in os.environ.items()
        if not key.upper().startswith(("GST_", "QT_", "QML", "DYLD_"))
    }
    env["PATH"] = os.path.join(os.environ["SystemRoot"], "System32") if windows else "/usr/bin:/bin"
    with TemporaryDirectory() as temp:
        env.update({
            "GST_PLUGIN_PATH_1_0": str(plugin_dir),
            "GST_PLUGIN_SYSTEM_PATH_1_0": "",
            "GST_REGISTRY_1_0": str(Path(temp) / "registry.bin"),
            "GST_REGISTRY_FORK": "no",
        })
        subprocess.run([str(executable), "--check-startup"], cwd=temp, env=env,
                       timeout=150, check=True)
    print(f"Rendered the installed ThorVision window using {plugin}.")


if __name__ == "__main__":
    main()
