"""The public tree excludes local documentation and binary artifacts."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from check_public_tree import allowed_path, content_errors


class SourceOnlyPublicTree(unittest.TestCase):
    def test_documentation_and_former_image_exceptions_are_closed(self):
        for name in ("docs/NEXT_MODEL_PROMPT.md", "docs/CONTINUE.md",
                     "docs/images/native-frame-before.png",
                     "docs/images/native-frame-after.png", "docs/other.md"):
            with self.subTest(name=name):
                self.assertFalse(allowed_path(name))

    def test_binary_paths_and_text_disguises_remain_closed(self):
        for name in ("native/image.png", "docs/images/game.iso", "work/frame.png"):
            self.assertFalse(allowed_path(name))
        self.assertTrue(content_errors("README.md", b"\x89PNG\0\xff"))


if __name__ == "__main__":
    unittest.main()
