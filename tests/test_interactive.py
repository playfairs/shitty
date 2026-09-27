import os
import pty
import select
import shutil
import sys
import tempfile
import time

PROMPT = b"\x1b[1;36m\xce\xbb\x1b[0;37m : \x1b[0m"


class ShellSession:
    def __init__(self, executable):
        self.home = tempfile.TemporaryDirectory()
        self.completion_bin = os.path.join(self.home.name, "bin")
        os.mkdir(self.completion_bin)
        echo_executable = shutil.which("echo")
        os.symlink(echo_executable,
                   os.path.join(self.completion_bin, "tab-completion"))
        for index in range(21):
            name = f"tab-many-{index:02d}"
            os.symlink(echo_executable,
                       os.path.join(self.completion_bin, name))
        self.process_id, self.terminal = pty.fork()
        if self.process_id == 0:
            environment = os.environ.copy()
            environment["HOME"] = self.home.name
            environment["PATH"] = (
                self.completion_bin
                + os.pathsep
                + environment.get("PATH", "")
            )
            os.execve(executable, [executable], environment)
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

    def read_prompt(self):
        return self.read_until(b"\n" + PROMPT)

    def close(self):
        _, status = os.waitpid(self.process_id, 0)
        os.close(self.terminal)
        self.home.cleanup()
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
            raise AssertionError(f"shell exited with status {status}")


def test_history_and_interrupts(executable):
    session = ShellSession(executable)
    session.read_until(PROMPT)
    print("started", file=sys.stderr, flush=True)

    session.write(b"\t")
    session.read_until(b"\a")
    session.redraw(b"")

    session.write(b"\x1b[A")
    session.redraw(b"")
    session.write(b"\x1b[B")
    session.redraw(b"")
    session.write(b"echo first\n")
    first_run = session.read_prompt()
    assert b"first" in first_run
    print("first command", file=sys.stderr, flush=True)

    session.write(b"\x1b[A")
    session.redraw(b"echo first")
    session.write(b"\x7f" * 5 + b"edited")
    session.redraw(b"echo edited")
    session.write(b"\n")
    edited_run = session.read_prompt()
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
    session.read_prompt()
    print("interrupt empty", file=sys.stderr, flush=True)
    session.write(b"echo partial\x03")
    session.read_prompt()
    print("interrupt typed", file=sys.stderr, flush=True)
    session.write(b"echo clean\n")
    clean_run = session.read_prompt()
    assert b"clean" in clean_run
    assert b"partial\n" not in clean_run

    session.write(b"tab-c\t")
    session.redraw(b"tab-completion ")
    session.write(b"completed\n")
    completed_command = session.read_prompt()
    assert b"completed" in completed_command

    session.write(b"cd src/co\t")
    session.redraw(b"cd src/core/")
    session.write(b"\n")
    session.read_prompt()
    session.write(b"pwd\n")
    completed_path = session.read_prompt()
    assert b"src/core" in completed_path

    many_prefix = b"tab-many-"
    session.write(many_prefix)
    session.redraw(many_prefix)
    session.write(b"\t")
    confirmation = session.read_until(b"Press Tab again to show all")
    assert b"tab-many-00" not in confirmation
    session.redraw(many_prefix)
    session.write(b"\t")
    displayed = session.redraw(many_prefix)
    for index in range(21):
        assert f"tab-many-{index:02d}".encode() in displayed
    session.write(b"\x03")
    session.read_prompt()

    session.write(b"exit 0\n")
    session.close()
    print("closed", file=sys.stderr, flush=True)


if __name__ == "__main__":
    test_history_and_interrupts(sys.argv[1])
