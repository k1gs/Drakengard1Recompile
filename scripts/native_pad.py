"""Send a bounded controller command to an opt-in native diagnostic run."""
import argparse
import contextlib
import datetime
import json
import os
from pathlib import Path
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BUTTONS = dict(zip(("select", "l3", "r3", "start", "up", "right", "down", "left",
                    "l2", "r2", "l1", "r1", "triangle", "circle", "cross", "square"), range(16)))


@contextlib.contextmanager
def command_lock(path):
    with path.open("x"):
        pass
    try:
        yield
    finally:
        path.unlink()


def send(run, buttons, hold, axes=None):
    if axes is not None and (len(axes)!=4 or any(type(x) is not int or not 0<=x<=255 for x in axes)):
        raise ValueError("Invalid pad axes")
    if hold==0 and axes is not None and tuple(axes)!=(127,127,127,127):
        raise ValueError("Zero hold requires centered axes")
    if not 1 <= run <= 999 or not 0 <= buttons <= 0xffff or not 0 <= hold <= 10000 or (hold == 0 and buttons != 0xffff):
        raise ValueError("Invalid bounded pad command")
    directory = ROOT / "work" / f"native-run-{run:03d}"
    path = directory / "pad-control.txt"
    with command_lock(directory / "pad-control.lock"):
        # Read only the opt-in host file, never guest input or memory dumps.
        with path.open("r", encoding="ascii") as current:
            payload = current.read(129)
        fields = payload.removesuffix("\n").split(" ")
        if len(payload) > 128 or not payload.endswith("\n") or not ((len(fields)==4 and fields[0]=="D1PAD1") or (len(fields)==5 and fields[0]=="D1PAD2")):
            raise ValueError("Invalid existing pad-control protocol")
        sequence = int(fields[1]) + 1
        if not 1 <= sequence <= 0xffffffffffffffff:
            raise ValueError("Pad sequence exhausted")
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(mode="w", encoding="ascii", newline="\n",
                                             dir=directory, prefix="pad-command-", delete=False) as stream:
                temporary = Path(stream.name)
                axis_text="" if axes is None else "".join(f"{x:02X}" for x in axes)
                stream.write(f"D1PAD1 {sequence} {buttons:04X} {hold}\n" if axes is None else
                             f"D1PAD2 {sequence} {buttons:04X} {hold} {axis_text}\n")
            deadline = time.monotonic() + 2
            while True:
                try:
                    os.replace(temporary, path)
                    break
                except PermissionError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.01)
            event = {"sequence": sequence, "buttons": f"{buttons:04X}", "hold_ms": hold,
                     "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                     "source": "explicit host virtual controller; no OS input or guest RAM writes"}
            if axes is not None: event["axes_rx_ry_lx_ly"]=list(axes)
            with (directory / "pad-control-events.jsonl").open("a", encoding="utf-8") as log:
                log.write(json.dumps(event) + "\n")
            return event
        finally:
            if temporary is not None and temporary.exists():
                temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=int, required=True, choices=range(1, 1000))
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--press", nargs="+", choices=tuple(BUTTONS))
    action.add_argument("--release", action="store_true")
    action.add_argument("--axes",type=int,nargs=4,choices=range(256),metavar=("RX","RY","LX","LY"))
    parser.add_argument("--hold-ms", type=int, default=1000, choices=range(1, 10001))
    args = parser.parse_args()
    mask = 0xffff
    for name in args.press or ():
        mask &= ~(1 << BUTTONS[name])
    try:
        print(json.dumps(send(args.run, mask, 0 if args.release else args.hold_ms, args.axes), indent=2))
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
