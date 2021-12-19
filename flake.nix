{
  description = "zig-graph";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    zig.url = "github:arqv/zig-overlay";
  };

  outputs = { self, nixpkgs, flake-utils, ... }@inputs:
    # These are the same systems that zig supports
    let systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
    in flake-utils.lib.eachSystem systems (system:
      let
        # Our in-repo overlay of packages
        overlay = (import ./nix/overlay.nix) {
          inherit nixpkgs;
          zigpkgs = inputs.zig.packages.${system};
        };

        # Initialize our package repository, adding overlays from inputs
        pkgs = import nixpkgs {
          inherit system;

          overlays = [
            overlay
          ];
        };
      in rec {
        devShell = pkgs.devShell;
      }
    );
}
