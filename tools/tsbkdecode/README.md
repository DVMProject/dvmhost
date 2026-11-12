# tsbkdecode

Standalone test tool using the common P25 TSBK and DMR CSBK decoders, with
local definitions, CLI entry point, decoder implementation, and CMake source
list following `src/testapp`.

Build from the repository root:

```sh
cmake --build build --target tsbkdecode
```

Supply hexadecimal bytes as separate arguments or one quoted/contiguous string:

```sh
build/tsbkdecode tsbk 80 00 04 10 23 12 34 00 00 01
build/tsbkdecode csbk "9f 00 00 00 12 34 56 65 43 21"
build/tsbkdecode tsbk 800004102312340000016ed5
```

Raw input accepts 10 bytes (header and payload; CRC is generated) or 12 bytes
(including CRC; failures warn and decoding continues). CSBK input uses the standard A5A5 CRC mask.
Spaces, colons, commas, and per-byte `0x` prefixes are supported.

Use `--frame` for a 45-byte P25 TSDU or 33-byte DMR burst, without modem tags.
The P25 frame path decodes the first TSBK. Use `--isp` for inbound P25 signalling;
outbound network/RFSS status broadcasts otherwise take precedence over the
inbound authentication opcodes sharing their values.

Output includes decoded bytes, opcode, manufacturer/feature ID, last-block
flag, message description, and fields extracted by the applicable decoder.
Numbers are displayed in decimal and hexadecimal. Interpretation follows the
common library's supported layouts; unknown opcodes and unimplemented decoders
report errors rather than claim a complete interpretation. No channel frequency
lookup is performed.

Exit status is 0 on successful decoding/help/version and 1 for malformed input,
incorrect length or unsupported decoding. CRC failures alone do not cause a nonzero
exit status, including with `--frame`. See `--help` for usage.
