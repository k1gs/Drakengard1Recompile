"""Prepare private compiler units with discarded VF0 writes and guarded SUBs."""
import argparse
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ELF_SHA256 = "065a94591d295bb936a8dca92e722c3bd72cd05848c7274b26d572b0f782e918"
GRID_UNIT = "fn_002408DC_0x2408dc.cpp"
MARKER = "// Prepared macro-VU register semantics v1."
DISCARD = "drakengard::discardVu0Write"
# Game bindings are restricted to two instruction identities. The surrounding
# generator shape is separately checked below; no guest code is published.
GRID_SUBS = ((0x241108, 0x4BE06004, 12, 0, True, "_mm_blendv_ps"),
             (0x241120, 0x4BEC682C, 13, 12, False, "PS2_VBLEND"))
FUNCTION = re.compile(r"void fn_\w+\(uint8_t\* rdram, R5900Context\* ctx, PS2Runtime \*runtime\) \{")
VF0 = r"ctx\s*->\s*vu0_vf\s*\[\s*0\s*\]"
WRITE = re.compile(VF0 + r"\s*=(?!=)")
SIMPLE_WRITE = re.compile(VF0 + r"[ \t]*=[ \t]*(?P<rhs>[^;\n]+);")
OTHER_WRITE = re.compile(VF0 + r"\s*(?:[+*/&|^-]=|\+\+|--)")


def _grid_sub(text, binding):
    pc, opcode, fs, ft, broadcast, blend = binding
    if len(re.findall(rf"^    // 0x{pc:x}:", text, re.MULTILINE)) != 1:
        raise ValueError(f"Pinned macro-VU SUB identity duplicated/missing at {pc:#x}")
    left = f"ctx->vu0_vf[{fs}]"
    right = f"ctx->vu0_vf[{ft}]"
    if broadcast:
        right = f"_mm_shuffle_ps({right}, {right}, _MM_SHUFFLE(0,0,0,0))"
    # The pinned emitter has two spellings of the same destination blend.
    body = (f"{{ __m128 res = PS2_VSUB({left}, {right}); "
            "__m128i mask = _mm_set_epi32(-1, -1, -1, -1); "
            f"ctx->vu0_vf[0] = {blend}(ctx->vu0_vf[0], res, _mm_castsi128_ps(mask)); }}")
    pattern = re.compile(
        rf"(?P<comment>    // 0x{pc:x}: 0x{opcode:x}  [^\n]+\n)"
        + re.escape(f"    ctx->pc = 0x{pc:x}u;\n    {body}"))
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise ValueError(f"Pinned macro-VU SUB shape changed at {pc:#x}")
    replacement = (matches[0].group("comment") + f"    ctx->pc = 0x{pc:x}u;\n"
                   f"    drakengard::eeVu0Sub(ctx, 0u, 15u, {left}, {right});")
    return pattern.sub(lambda _: replacement, text, count=1)


def prepare_source(text, name):
    """Pure transform. Accept originals only, so prepared text cannot hide drift."""
    if MARKER in text or DISCARD in text or '"ee_vu0_sub.h"' in text:
        raise ValueError("Expected original generated source, not a prepared unit")
    if OTHER_WRITE.search(text):
        raise ValueError("Unreviewed VF0 assignment operator")
    changed = False
    if name == GRID_UNIT:
        for binding in GRID_SUBS:
            text = _grid_sub(text, binding)
        changed = True
    if WRITE.search(text):
        functions = list(FUNCTION.finditer(text))
        if len(functions) != 1:
            raise ValueError("Generated function signature contract changed")
        simple = list(SIMPLE_WRITE.finditer(text))
        if len(simple) != len(WRITE.findall(text)):
            raise ValueError("Unreviewed multiline/compound VF0 assignment")
        for match in simple:
            if re.search(r"(?<![<>=!])=(?!=)", match.group("rhs")):
                raise ValueError("Unreviewed nested VF0 assignment")
        text = SIMPLE_WRITE.sub(lambda m: f'{DISCARD}({m.group("rhs")});', text)
        changed = True
    return (MARKER + '\n#include "ee_vu0_sub.h"\n' + text) if changed else text


def prepare(source, output, elf):
    # Verify before reading generated guest units or writing any output.
    if hashlib.sha256(elf.read_bytes()).hexdigest() != ELF_SHA256:
        raise ValueError("Unsupported input ELF SHA-256")
    source, output = source.resolve(), output.resolve()
    if (not output.is_relative_to(ROOT / "build") or output == source
            or source.is_relative_to(output) or output.is_relative_to(source)):
        raise ValueError("Compiler units must remain private in build/ and outside the source")
    paths = sorted(source.glob("*.cpp"))
    if not any(path.name == GRID_UNIT for path in paths):
        raise ValueError("Missing pinned grid compiler unit")
    # Validate the complete batch before creating/replacing compiler units.
    prepared = []
    for path in paths:
        original = path.read_text(encoding="utf-8")
        content = prepare_source(original, path.name)
        if content != original:
            prepared.append((path.name, content))
    if output.exists() and {path.name for path in output.glob("*.cpp")} - {name for name, _ in prepared}:
        raise ValueError("Stale compiler units in output; choose a fresh build-local directory")
    output.mkdir(parents=True, exist_ok=True)
    changed = 0
    for name, content in prepared:
        destination = output / name
        if not destination.is_file() or destination.read_text(encoding="utf-8") != content:
            destination.write_text(content, encoding="utf-8")
            changed += 1
    return len(paths), changed


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--input-complete", action="store_true")
    parser.add_argument("--source", type=Path, default=ROOT / "generated/lift-011")
    parser.add_argument("--input", type=Path, default=ROOT / "work/extracted-001/SLES_523.22")
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error("Confirm original inputs are complete with --input-complete")
    count, changed = prepare(args.source, args.output, args.input)
    print(f"Prepared {count} macro-VU compiler units; {changed} files updated")


if __name__ == "__main__":
    main()
