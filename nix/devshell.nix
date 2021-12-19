{ mkShell
, zig
}: mkShell rec {
  name = "zig-graph";

  buildInputs = [
    zig
  ];
}
