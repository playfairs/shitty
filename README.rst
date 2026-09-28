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

The interactive prompt is ``λ :``. The shell supports single- and
double-quoted words, environment-variable expansion, ``;``-separated commands,
pipelines with ``|``, and the ``export`` and ``unset`` builtins. Assignment-only
commands update the shell environment; assignments before a command apply to
that command only. Redirections, control-flow syntax, and full POSIX shell
compatibility are not implemented yet. Interactive input supports tab
completion for command names and filesystem paths; lists with more than 20
matches require a second Tab to show. Ctrl-F opens command history in ``fzf``
when it is installed. The shell also reads commands from standard input when it
is not attached to a terminal.

Run the test suite with:

.. code-block:: sh

   nox task test
