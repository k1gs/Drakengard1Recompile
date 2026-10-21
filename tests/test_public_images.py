"""The screenshot exception must not open publication to arbitrary binary data."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from check_public_tree import REVIEWED_IMAGES, allowed_path, content_errors


class ReviewedDocumentationImages(unittest.TestCase):
    def test_approved_images_match_reviewed_bytes(self):
        for name in REVIEWED_IMAGES:
            with self.subTest(name=name):
                self.assertTrue(allowed_path(name))
                self.assertEqual(content_errors(name, (ROOT / name).read_bytes()), [])

    def test_replacement_bytes_are_not_automatically_approved(self):
        for name in REVIEWED_IMAGES:
            self.assertTrue(content_errors(name, b"\x89PNG\r\n\x1a\nunreviewed fixture"))

    def test_other_binary_paths_and_text_disguises_remain_closed(self):
        for name in ("docs/images/other.png", "docs/images/game.iso", "work/frame.png"):
            self.assertFalse(allowed_path(name))
        self.assertTrue(content_errors("docs/frame.md", b"\x89PNG\0\xff"))


if __name__ == "__main__":
    unittest.main()
