"""Report local preparation tools without installing or reading game inputs."""
import json
import platform
import shutil
import sys

if __name__ == "__main__":
    print(json.dumps({
        "python": platform.python_version(),
        "python_supported": sys.version_info >= (3, 10),
        "host": platform.platform(),
        "tools": {name: shutil.which(name) for name in
                  ("git", "cmake", "ninja", "cl", "clang-cl", "gh")},
        "note": "Native tools are optional at M0; game/runtime are not checked.",
    }, indent=2))
    sys.exit(0 if sys.version_info >= (3, 10) else 1)
