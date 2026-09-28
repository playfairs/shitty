{ self }:
{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.programs.shit;
in
{
  options.programs.shit = {
    enable = lib.mkEnableOption "the shit shell";

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.system}.default;
      defaultText = lib.literalExpression "inputs.shitty.packages.\${pkgs.system}.default";
      description = "The shit shell package to install.";
    };

    shellAliases = lib.mkOption {
      type = lib.types.attrsOf lib.types.str;
      default = { };
      description = "Aliases written to ~/.shitrc.";
    };

    initContent = lib.mkOption {
      type = lib.types.lines;
      default = "";
      description = "Shell commands appended to ~/.shitrc.";
    };
  };

  config = lib.mkIf cfg.enable {
    home.packages = [ cfg.package ];

    home.file.".shitrc".text =
      lib.concatStringsSep "\n" (
        (lib.mapAttrsToList (
          name: value: "alias ${lib.escapeShellArg name}=${lib.escapeShellArg value}"
        ) cfg.shellAliases)
        ++ lib.optional (cfg.initContent != "") cfg.initContent
      )
      + "\n";
  };
}
