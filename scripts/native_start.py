"""Pass the no-memory-card startup prompt with bounded ordinary controller input."""
import argparse
import re
import time

import native_pad


class DecoderStarted(Exception):
    """Stop all further startup input once the guest initializes the IPU."""


def advance_startup(run, after_frame=1800, timeout=180, attempts=5):
    if not 1 <= run <= 999 or not 0 <= after_frame <= 18000 or not 1 <= timeout <= 300 or not 1 <= attempts <= 8:
        raise ValueError("Invalid startup automation bounds")
    directory = native_pad.ROOT / "work" / f"native-run-{run:03d}"
    if not (directory / "pad-control.txt").is_file():
        raise ValueError("Run must already opt in to --pad-control")
    deadline = time.monotonic() + timeout

    def snapshot():
        if (directory / "result.json").exists():
            raise ValueError("Native run has already ended")
        if time.monotonic() >= deadline:
            raise ValueError("Startup controller reached its time limit")
        log = directory / "runtime.log"
        data = log.read_bytes() if log.exists() else b""
        if b"[ee-ipu-command]" in data:
            raise DecoderStarted
        if b"Native blocker:" in data or b"[run] exiting loop" in data:
            raise ValueError("Native runner is stopping")
        return data

    def wait(predicate):
        while True:
            data = snapshot()
            if predicate(data):
                return
            time.sleep(0.05)

    def pause(seconds):
        until = time.monotonic() + seconds
        wait(lambda _: time.monotonic() >= until)

    try:
        wait(lambda data: any(int(frame) >= after_frame for frame in re.findall(rb"\[diagnostic\] frame=(\d+) ", data)))
        for attempt in range(1, attempts + 1):
            for button, mask in (("left", 0xff7f), ("cross", 0xbfff)):
                before = len(snapshot())
                event = native_pad.send(run, mask, 700)
                print(f"attempt={attempt} button={button} sequence={event['sequence']}", flush=True)
                report_deadline = min(deadline, time.monotonic() + 10)

                def reported(data):
                    # Offsets and slices both use bytes, including Windows CRLF.
                    recent = data[before:]
                    pressed = recent.find(f"[sio2-pad-input-report] buttons=0x{mask:04x}".encode())
                    released = recent.find(b"[sio2-pad-input-report] buttons=0xffff", max(pressed, 0))
                    if pressed >= 0 and released > pressed:
                        return True
                    if time.monotonic() >= report_deadline:
                        raise ValueError(f"Guest did not report {button} press and release")
                    return False

                wait(reported)
                pause(0.5)
            # Retries cover a prompt that was not yet processing input. This is
            # a checkpoint-specific startup recipe, not visual dialog recognition.
            pause(3)
        wait(lambda _: False)
    except DecoderStarted:
        print("IPU startup observed; startup controller finished", flush=True)
        return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=int, required=True)
    parser.add_argument("--after-frame", type=int, default=1800)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--attempts", type=int, default=5)
    args = parser.parse_args()
    try:
        advance_startup(args.run, args.after_frame, args.timeout, args.attempts)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
