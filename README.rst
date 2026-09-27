shitty
======

A rather shitty shell.

``shitty`` is a Unix shell written in C. Its executable is ``shit``. The project
is intended to grow toward POSIX shell compatibility; the initial implementation
provides only a small interactive foundation.

Development
-----------

Enter the Nix development environment and build with Nox:

.. code-block:: sh

   nix develop
   nox build

Run the shell with:

.. code-block:: sh

   ./build/debug/shit/shit
   or
   nox run

The interactive prompt is ``λ :``. Commands are split on whitespace, and
multiple commands can be separated with ``;``. Quoting and redirection are not
implemented. The shell also reads commands from standard input when it is not
attached to a terminal.

Run the test suite with:

.. code-block:: sh

   nox task test
