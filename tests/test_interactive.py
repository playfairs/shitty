import os
import pty
import select
import sys
import time

PROMPT = b"\xce\xbb : "


class ShellSession:
    def __init__(self, executable):
        self.process_id, self.terminal = pty.fork()
        if self.process_id == 0:
            os.execv(executable, [executable])
        self.buffer = bytearray()

    def write(self, data):
        os.write(self.terminal, data)

    def read_until(self, expected, timeout=5):
        deadline = time.monotonic() + timeout
        while expected not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(
                    f"timed out waiting for {expected!r}: {bytes(self.buffer)!r}"
                )
            readable, _, _ = select.select([self.terminal], [], [], remaining)
            if not readable:
                continue
            chunk = os.read(self.terminal, 4096)
            if not chunk:
                raise AssertionError(f"shell exited while waiting for {expected!r}")
            self.buffer.extend(chunk)
        end = self.buffer.index(expected) + len(expected)
        result = bytes(self.buffer[:end])
        del self.buffer[:end]
        return result

    def redraw(self, line):
        return self.read_until(b"\r\x1b[K" + PROMPT + line)

    def close(self):
        os.close(self.terminal)
        _, status = os.waitpid(self.process_id, 0)
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
            raise AssertionError(f"shell exited with status {status}")


def test_history_and_interrupts(executable):
    session = ShellSession(executable)
    session.read_until(PROMPT)
    print("started", file=sys.stderr, flush=True)

    session.write(b"\x1b[A")
    session.redraw(b"")
    session.write(b"\x1b[B")
    session.redraw(b"")
    session.write(b"echo first\n")
    first_run = session.read_until(PROMPT)
    assert b"first" in first_run
    print("first command", file=sys.stderr, flush=True)

    session.write(b"\x1b[A")
    session.redraw(b"echo first")
    session.write(b"\x7f" * 5 + b"edited")
    session.redraw(b"echo edited")
    session.write(b"\n")
    edited_run = session.read_until(PROMPT)
    assert b"edited" in edited_run
    print("edited history", file=sys.stderr, flush=True)

    session.write(b"\x1b[A")
    session.redraw(b"echo edited")
    session.write(b"\x1b[A")
    session.redraw(b"echo first")
    session.write(b"\x1b[B")
    session.redraw(b"echo edited")
    session.write(b"\x1b[B")
    session.redraw(b"")
    print("navigation", file=sys.stderr, flush=True)

    session.write(b"echo scratch")
    session.redraw(b"echo scratch")
    session.write(b"\x1b[A")
    session.redraw(b"echo edited")
    session.write(b"\x1b[B")
    session.redraw(b"echo scratch")

    session.write(b"\x03")
    session.read_until(PROMPT)
    print("interrupt empty", file=sys.stderr, flush=True)
    session.write(b"echo partial\x03")
    session.read_until(PROMPT)
    print("interrupt typed", file=sys.stderr, flush=True)
    session.write(b"echo clean\n")
    clean_run = session.read_until(PROMPT)
    assert b"clean" in clean_run
    assert b"partial\n" not in clean_run

    session.write(b"exit\n")
    session.close()
    print("closed", file=sys.stderr, flush=True)


if __name__ == "__main__":
    test_history_and_interrupts(sys.argv[1])
