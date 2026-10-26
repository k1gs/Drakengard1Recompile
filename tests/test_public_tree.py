"""Local documents and unreviewed binary artifacts stay outside the public tree."""
from pathlib import Path
import hashlib
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from check_public_tree import allowed_path, content_errors, REVIEWED_IMAGES


class SourceOnlyPublicTree(unittest.TestCase):
    def test_documentation_and_former_image_exceptions_are_closed(self):
        for name in ("AGENTS.md", "docs/NEXT_MODEL_PROMPT.md", "docs/CONTINUE.md",
                     "docs/images/native-frame-before.png",
                     "docs/images/native-frame-after.png", "docs/other.md"):
            with self.subTest(name=name):
                self.assertFalse(allowed_path(name))

    def test_binary_paths_and_text_disguises_remain_closed(self):
        for name in ("native/image.png", "docs/images/game.iso", "work/frame.png"):
            self.assertFalse(allowed_path(name))
        self.assertTrue(content_errors("README.md", b"\x89PNG\0\xff"))

    def test_only_exact_reviewed_image_content_is_accepted(self):
        name = "assets/progress/native-scene-hud.png"
        self.assertTrue(allowed_path(name))
        self.assertFalse(allowed_path("assets/progress/other.png"))
        data = b"\x89PNG\r\n\x1a\nsynthetic gate fixture"
        with patch.dict(REVIEWED_IMAGES, {name: hashlib.sha256(data).hexdigest()}):
            self.assertEqual(content_errors(name, data), [])
            self.assertTrue(content_errors(name, data + b"altered"))


if __name__ == "__main__":
    unittest.main()
