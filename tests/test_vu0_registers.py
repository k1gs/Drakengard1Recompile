"""Synthetic emitter-contract checks; no game files or installed tools needed."""
import contextlib
import hashlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import prepare_vu0_registers as vu

FUNCTION = "void fn_synthetic(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {\n"


def synthetic_grid():
    blocks = []
    for pc, opcode, fs, ft, broadcast, blend in vu.GRID_SUBS:
        left, right = f"ctx->vu0_vf[{fs}]", f"ctx->vu0_vf[{ft}]"
        if broadcast:
            right = f"_mm_shuffle_ps({right}, {right}, _MM_SHUFFLE(0,0,0,0))"
        blocks.append(f"    // 0x{pc:x}: 0x{opcode:x}  synthetic SUB shape\n"
                      f"    ctx->pc = 0x{pc:x}u;\n"
                      f"    {{ __m128 res = PS2_VSUB({left}, {right}); "
                      "__m128i mask = _mm_set_epi32(-1, -1, -1, -1); "
                      f"ctx->vu0_vf[0] = {blend}(ctx->vu0_vf[0], res, _mm_castsi128_ps(mask)); }}\n")
    return FUNCTION + "".join(blocks) + "}\n"


class Vu0RegistersTests(unittest.TestCase):
    def test_discard_write_keeps_rhs_and_vf0_reads(self):
        source = FUNCTION + "    ctx->vu0_vf[0] = READ128(side_effect());\n    consume(ctx->vu0_vf[0]);\n}\n"
        result = vu.prepare_source(source, "synthetic.cpp")
        self.assertIn("drakengard::discardVu0Write(READ128(side_effect()));", result)
        self.assertIn("consume(ctx->vu0_vf[0]);", result)
        self.assertEqual(result.count("drakengard::discardVu0Write("), 1)

    def test_unrelated_source_exactly_unchanged(self):
        source = FUNCTION + "    ctx->vu0_vf[1] = ctx->vu0_vf[0];\n}\n"
        self.assertEqual(vu.prepare_source(source, "synthetic.cpp"), source)

    def test_two_bound_helpers_keep_instruction_comments(self):
        result = vu.prepare_source(synthetic_grid(), vu.GRID_UNIT)
        self.assertEqual(result.count("drakengard::eeVu0Sub("), 2)
        self.assertIn("ctx, 0u, 15u, ctx->vu0_vf[13], ctx->vu0_vf[12]", result)
        self.assertIn("_MM_SHUFFLE(0,0,0,0)", result)
        self.assertNotIn("drakengard::discardVu0Write", result)
        for pc, opcode, *_ in vu.GRID_SUBS:
            self.assertIn(f"// 0x{pc:x}: 0x{opcode:x}", result)

    def test_anchor_and_signature_drift_rejected(self):
        source = synthetic_grid()
        for broken in (source.replace("PS2_VSUB", "_mm_sub_ps", 1),
                       source.replace("0x4be06004", "0x4be06005"),
                       source + source):
            with self.assertRaises(ValueError):
                vu.prepare_source(broken, vu.GRID_UNIT)
        with self.assertRaises(ValueError):
            vu.prepare_source("ctx->vu0_vf[0] = f();", "bad.cpp")
        with self.assertRaises(ValueError):
            vu.prepare_source(FUNCTION + "ctx->vu0_vf[0] += f();\n}", "bad.cpp")
        for assignment in ("ctx->vu0_vf[0] =\n f();", "ctx->vu0_vf[0] = other = f();"):
            with self.assertRaises(ValueError):
                vu.prepare_source(FUNCTION + assignment + "\n}", "bad.cpp")

    def test_prepared_units_are_not_accepted_as_originals(self):
        with self.assertRaises(ValueError):
            vu.prepare_source(vu.prepare_source(synthetic_grid(), vu.GRID_UNIT), vu.GRID_UNIT)

    def test_no_input_access_before_opt_in(self):
        with patch.object(Path, "read_bytes", side_effect=AssertionError("input read")):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                vu.main(["unused-output"])

    def test_hash_guard_batch_validation_and_idempotence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); source = root / "source"; source.mkdir()
            elf = root / "fixture.elf"; elf.write_bytes(b"synthetic-only-input")
            (source / vu.GRID_UNIT).write_text(synthetic_grid(), encoding="utf-8")
            (source / "other.cpp").write_text(FUNCTION + "ctx->vu0_vf[0] = f();\n}", encoding="utf-8")
            unchanged = FUNCTION + "    ctx->vu0_vf[1] = ctx->vu0_vf[0];\n}\n"
            (source / "unchanged.cpp").write_text(unchanged, encoding="utf-8")
            output = root / "build/prepared"
            with patch.object(vu, "ROOT", root):
                with self.assertRaises(ValueError):
                    vu.prepare(source, output, elf)
                self.assertFalse(output.exists())
                with patch.object(vu, "ELF_SHA256", hashlib.sha256(elf.read_bytes()).hexdigest()):
                    self.assertEqual(vu.prepare(source, output, elf), (3, 2))
                    self.assertFalse((output / "unchanged.cpp").exists())
                    self.assertEqual((source / "unchanged.cpp").read_text(), unchanged)
                    stamp = (output / vu.GRID_UNIT).stat().st_mtime_ns
                    self.assertEqual(vu.prepare(source, output, elf), (3, 0))
                    self.assertEqual((output / vu.GRID_UNIT).stat().st_mtime_ns, stamp)
                    self.assertEqual((source / vu.GRID_UNIT).read_text(), synthetic_grid())
                    # An unchanged original is stale if it exists in sparse output.
                    (output / "unchanged.cpp").write_text("stale", encoding="utf-8")
                    with self.assertRaises(ValueError):
                        vu.prepare(source, output, elf)
                    (output / "unchanged.cpp").unlink()
                    with self.assertRaises(ValueError):
                        vu.prepare(source, root / "public", elf)
                    (source / vu.GRID_UNIT).write_text("broken", encoding="utf-8")
                    fresh = root / "build/fresh"
                    with self.assertRaises(ValueError):
                        vu.prepare(source, fresh, elf)
                    self.assertFalse(fresh.exists())


if __name__ == "__main__":
    unittest.main()
