{ pkgs ? import <nixpkgs> {} }:

let
  clangPkg = pkgs.clang;
in
pkgs.mkShell {
  name = "cpp-ast-processor";

  buildInputs = with pkgs; [
    clang
    clang.cc
    llvm
    gnumake
    python3
    lsof
  ];

  shellHook = ''
    export CLANG_RESOURCE_DIR="$(${clangPkg}/bin/clang++ -print-resource-dir 2>/dev/null || true)"
    if [ -n "$CLANG_RESOURCE_DIR" ] && [ -f "$CLANG_RESOURCE_DIR/include/clang/AST/ASTConsumer.h" ]; then
      echo "clang resource dir: $CLANG_RESOURCE_DIR"
    else
      echo "warning: clang AST headers not found under CLANG_RESOURCE_DIR=$CLANG_RESOURCE_DIR"
    fi
    echo "C++ AST processor shell ready (clang $(clang++ --version | head -1))"
  '';
}