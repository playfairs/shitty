{
  pkgs,
  treefmt-nix,
  self,
}:

let
  treefmtEval = treefmt-nix.lib.evalModule pkgs (_: {
    projectRootFile = ".git/config";
    programs = {
      nixfmt.enable = true;
      nixf-diagnose.enable = true;
      toml-sort.enable = true;
      black.enable = true;
      clang-format.enable = true;
    };

    settings.formatter.clang-format.includes = [
      "*.c"
      "*.h"
      "*.cpp"
    ];
  });
in
{
  wrapper = treefmtEval.config.build.wrapper;
  check = treefmtEval.config.build.check self;
}
