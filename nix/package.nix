{
  lib,
  stdenv,
}:
stdenv.mkDerivation {
  pname = "shitty";
  version = "unstable";
  src = ../.;

  buildPhase = ''
    runHook preBuild
    $CC -std=c17 -D_POSIX_C_SOURCE=200809L \
      -Wall -Wextra -Wpedantic -Werror \
      -Iinclude \
      src/core/main.c \
      src/core/shell.c \
      src/parsing/lexer.c \
      src/parsing/parser.c \
      src/execution/executor.c \
      src/expansion/expansion.c \
      src/history/history.c \
      src/input/input.c \
      src/terminal/terminal.c \
      src/prompt/prompt.c \
      src/builtin/registry.c \
      src/builtin/cd.c \
      src/builtin/pwd.c \
      src/builtin/exit.c \
      src/environment/environment.c \
      src/signals/signals.c \
      -o shit
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 shit "$out/bin/shit"
    runHook postInstall
  '';

  meta = {
    description = "A Unix shell written in C";
    mainProgram = "shit";
    platforms = lib.platforms.unix;
  };
}
