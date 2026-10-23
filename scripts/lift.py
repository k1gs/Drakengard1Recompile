"""Generate an unpatched local lift for the exact verified European ELF."""
import argparse
import csv
from pathlib import Path
import re
import subprocess
import sys
from intake import fingerprint, inspect_elf, ROOT

ELF_SHA256 = "065a94591d295bb936a8dca92e722c3bd72cd05848c7274b26d572b0f782e918"
TOOL_COMMIT = "eb8688b2fd191129f103a39543002c005b33409c"


def validate_map(path, metadata):
    """Reject overlapping, gap-spanning or non-executable map records before lifting."""
    previous_end = -1
    count = 0
    with Path(path).open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ["Name", "Start", "End", "Size"]:
            raise ValueError("Wrong map columns")
        for row in reader:
            start, end, size = int(row["Start"], 0), int(row["End"], 0), int(row["Size"])
            if start % 4 or end % 4 or start >= end or end - start != size:
                raise ValueError("Map record spans holes or has invalid alignment/size")
            if start < previous_end:
                raise ValueError("Map must contain sorted disjoint code fragments")
            if not any(s["flags"] & 1 and s["vaddr"] <= start and
                       end <= s["vaddr"] + s["filesz"] for s in metadata["load_segments"]):
                raise ValueError("Map record is outside file-backed executable memory")
            previous_end = end
            count += 1
    if not count:
        raise ValueError("Empty function map")
    return count


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-complete", action="store_true")
    parser.add_argument("--input", type=Path, default=ROOT / "work/extracted-001/SLES_523.22")
    parser.add_argument("--map", type=Path, default=ROOT / "work/functions-ghidra-002.csv")
    parser.add_argument("--run-id", default="lift-002")
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error("--input-complete is required before touching the ELF")
    if not re.fullmatch(r"lift-[0-9]{3}", args.run_id):
        parser.error("run-id must be lift-NNN")
    try:
        metadata = inspect_elf(args.input)
        if metadata["sha256"] != ELF_SHA256:
            raise ValueError("Wrong ELF SHA-256; game bindings are revision-specific")
        source = ROOT / "tools/ps2recomp"
        actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
        if actual != TOOL_COMMIT:
            raise ValueError("Wrong PS2Recomp revision")
        if subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"]):
            raise ValueError("PS2Recomp source is dirty; review and record a new tool revision first")
        map_path = args.map.resolve()
        if not map_path.is_relative_to(ROOT / "work") or not map_path.is_file():
            raise ValueError("Function map must be an existing local work/ CSV")
        validate_map(map_path, metadata)
        output = ROOT / "generated" / args.run_id
        run = ROOT / "work" / args.run_id
        if output.exists() or run.exists():
            raise ValueError("Run already exists; use a fresh run-id")
        exe = ROOT / "build/ps2recomp-tools/ps2xRecomp/Release/ps2_recomp.exe"
        if not exe.is_file():
            raise ValueError("Build the pinned PS2Recomp checkout before running lift")
        run.mkdir(parents=True)
        config = run / "config.toml"
        config.write_text('[general]\n'
            f'input = "{args.input.resolve().as_posix()}"\n'
            f'ghidra_output = "{map_path.as_posix()}"\n'
            f'output = "{output.as_posix()}"\n'
            'single_file_output = false\nlow_memory_mode = true\noutput_worker_threads = 1\n'
            'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\n'
            'stubs = []\nskip = []\n', encoding="utf-8")
        with (run / "recompiler.log").open("x", encoding="utf-8") as log:
            result = subprocess.run([str(exe), str(config)], stdout=log,
                                    stderr=subprocess.STDOUT, timeout=300, cwd=ROOT)
        if fingerprint(args.input)["sha256"] != ELF_SHA256:
            raise ValueError("ELF changed during lift")
        print(f"Recompiler exit {result.returncode}; local log: {run / 'recompiler.log'}")
        return result.returncode
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Lift refused/failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
