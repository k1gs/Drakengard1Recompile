"""Create a tiny original MIPS fixture (return 42), never game-derived bytes."""
from pathlib import Path
import struct


def elf_bytes():
    code = struct.pack("<III", 0x2402002A, 0x03E00008, 0)  # addiu v0,zero,42; jr ra; nop
    names = b"\0.text\0.shstrtab\0"
    code_offset, names_offset, sections_offset = 0x100, 0x10C, 0x120
    header = b"\x7fELF\x01\x01\x01" + bytes(9)
    header += struct.pack("<HHIIIIIHHHHHH", 2, 8, 1, 0x1000, 52, sections_offset,
                          0x20924001, 52, 32, 1, 40, 3, 2)
    segment = struct.pack("<IIIIIIII", 1, code_offset, 0x1000, 0x1000, len(code), len(code), 5, 0x100)
    data = bytearray(sections_offset + 120)
    data[:52] = header
    data[52:84] = segment
    data[code_offset:code_offset + len(code)] = code
    data[names_offset:names_offset + len(names)] = names
    struct.pack_into("<IIIIIIIIII", data, sections_offset + 40,
                     1, 1, 6, 0x1000, code_offset, len(code), 0, 0, 4, 0)
    struct.pack_into("<IIIIIIIIII", data, sections_offset + 80,
                     7, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0)
    return bytes(data)


def main():
    root = Path(__file__).resolve().parents[1]
    fixture = root / "work/smoke-001"
    fixture.mkdir(parents=True, exist_ok=False)
    (fixture / "return42.elf").write_bytes(elf_bytes())
    (fixture / "functions.csv").write_text(
        "Name,Start,End,Size\nreturn42,0x00001000,0x0000100C,12\n", encoding="utf-8")
    output = root / "generated/smoke-001"
    (fixture / "config.toml").write_text(
        '[general]\n'
        f'input = "{(fixture / "return42.elf").as_posix()}"\n'
        f'ghidra_output = "{(fixture / "functions.csv").as_posix()}"\n'
        f'output = "{output.as_posix()}"\n'
        'single_file_output = false\nlow_memory_mode = true\noutput_worker_threads = 1\n'
        'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\n'
        'stubs = []\nskip = []\n', encoding="utf-8")
    print(fixture / "config.toml")


if __name__ == "__main__":
    main()
