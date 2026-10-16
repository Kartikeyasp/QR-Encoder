# QR Code Generator in C

A QR code generator implemented in C.

## Features

- Supports QR code versions 1–40.
- Supports four error correction levels.
- Automatically selects a suitable QR code version.
- Automatically selects a mask pattern.
- Generates QR codes from user-provided text.
- Exports generated QR codes as PNG images.

## Requirements

- GCC or another compatible C compiler.
- TinyPngOut.h for PNG generation.

## Compilation

```bash
gcc -std=c11 -Wall -Wextra -O2 qr.c test.c -o qr_generator
```

## Usage 
`./qr_generator`
