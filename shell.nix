# Development shell for the C++ AST processor.
#
# Usage:
#   nix-shell            # enters a shell with all dependencies
#   make                 # builds web/processor inside the shell
#
# Packages needed (also installable imperatively):
#   nix profile install nixpkgs#clang nixpkgs#llvm nixpkgs#python3

{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "cpp-ast-processor";

  buildInputs = with pkgs; [
    clang        # clang++ + LibTooling dev headers + libclang-cpp
    llvm         # llvm-config
    gnumake      # make
    python3
  ];

  shellHook = ''
    echo "C++ AST processor shell ready (clang $(clang++ --version | head -1))"
  '';
}