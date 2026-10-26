"""Conservative tracked/index publication gate; not a substitute for diff review."""
from pathlib import PurePosixPath
import hashlib
import subprocess
import sys

ROOT_FILES = {"README.md", "LICENSE",
              ".gitignore", ".gitattributes"}
# Explicitly requested, visually reviewed progress image; all other binaries stay closed.
REVIEWED_IMAGES = {
    "assets/progress/native-scene-hud.png":
        "241db09b52f90e889b8fc9cd60066190120c8e4038d82a2e36e7f3bb2f4c78e3",
}
EXTENSIONS = {"scripts": {".py", ".java"}, "tests": {".py"},
              "native": {".cpp", ".h", ".cmake", ".txt"},
              ".github": {".yml", ".yaml", ".md"}}


def allowed_path(name):
    path = PurePosixPath(name)
    return name in ROOT_FILES or name in REVIEWED_IMAGES or (
        len(path.parts) > 1 and path.parts[0] in EXTENSIONS
        and path.suffix in EXTENSIONS[path.parts[0]])


def content_errors(name, data):
    if name in REVIEWED_IMAGES:
        if not data.startswith(b"\x89PNG\r\n\x1a\n") or hashlib.sha256(data).hexdigest() != REVIEWED_IMAGES[name]:
            return [f"Reviewed image content changed: {name}"]
        return []
    errors = []
    if len(data) > 256 * 1024 or b"\0" in data:
        errors.append(f"Oversized/binary file: {name}")
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        errors.append(f"Not UTF-8 source: {name}")
    return errors


def main():
    paths = subprocess.check_output(["git", "ls-files", "-z"]).decode("utf-8").split("\0")
    if not any(paths):
        print("No tracked/index files to review; stage the intended source first.", file=sys.stderr)
        return 1
    errors = []
    for name in filter(None, paths):
        if not allowed_path(name):
            errors.append(f"Not on source allowlist: {name}")
            continue
        # Read index bytes, including staged files later deleted/modified in the worktree.
        entry = subprocess.check_output(["git", "ls-files", "-s", "--", name]).decode()
        if not entry.startswith("100644 ") and not entry.startswith("100755 "):
            errors.append(f"Unsupported index mode: {name}")
            continue
        data = subprocess.check_output(["git", "show", f":{name}"])
        errors.extend(content_errors(name, data))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Public tree gate passed (tracked/index source; manual review still required).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
