{
  description = "A Unix shell written in C";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    nox.url = "github:playfairs/nox";
    nox.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs =
    {
      self,
      nixpkgs,
      nox,
      ...
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      treefmt-nix = nox.inputs.treefmt-nix;
    in
    {
      formatter = forAllSystems (
        system:
        let
          pkgs = import nixpkgs {
            inherit system;
          };
          formatter = import ./nix/formatter.nix {
            inherit pkgs treefmt-nix self;
          };
        in
        formatter.wrapper
      );

      checks = forAllSystems (
        system:
        let
          pkgs = import nixpkgs {
            inherit system;
          };
          formatter = import ./nix/formatter.nix {
            inherit pkgs treefmt-nix self;
          };
        in
        {
          formatting = formatter.check;
        }
      );

      devShells = forAllSystems (system: {
        default = nixpkgs.legacyPackages.${system}.mkShell {
          packages = [
            nox.packages.${system}.nox
            nixpkgs.legacyPackages.${system}.clang
            nixpkgs.legacyPackages.${system}.python3
            nixpkgs.legacyPackages.${system}.fzf
          ];
        };
      });
    };
}
