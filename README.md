# sidlc

`sidlc` is the Strata Interface Definition Language compiler. It is built as
part of the host tools graph and installed by the `folisdk-host` package.

The CMake integration module is installed as `UseSIDLC.cmake`.

## Status

* The active compiler is the C++20 executable in `sidlc/core` and `sidlc/lang`.
* The compiler uses subcommands: `compile`, `decompile`, and `generate`.
* `.sidl` is the human-authored source format.
* `.sif` is the compiled Strata InterFace artifact used for code generation
  and ABI history extension.
* `.sif` uses the `SIF\0` magic value and stores a binary tree representation
  of the parsed interface, with 4-byte-aligned strings, mandatory UUID
  identity/prefix metadata, a root revision hash, and a per-revision hash
  table. It does not embed the original `.sidl` source text.
* The only implemented output language is C (`generate --lang=c`).
* The only architecture ABI currently registered in `sidlc` is `x86_64`
  (`generate --arch=x86_64`), with an 8-byte pointer size and six register
  argument slots.
* Supported SIDL declarations include `interface`, contiguous `abirevision`
  blocks starting at `0`, `struct`, `bitfield<T>`, `enum<T>`, and `function`.
* Supported parameter directions are `in`, `out`, and `inout`.
* Supported type forms include built-ins, user-defined types, `ptr<T>`,
  `array<T>`, and `const`.
* Built-in C type mappings include `opaque`, `u8/u16/u32/u64`,
  `s8/s16/s32/s64`, `handle`, and `status`.
* Interface source annotations `@prefix("...")` and
  `@uuid("namespace-uuid", "name")` are required and are stored as SIF header
  metadata after compilation.
* Struct `@align_size(...)` is recognized and emits an aligned struct
  attribute.
* `generate --weak` emits weak client binding symbols.

## CLI Usage

Compile a source interface into a SIF artifact:

```sh
sidlc compile -o byte_stream.sif byte_stream.sidl
```

Extend an existing ABI history:

```sh
sidlc compile -b byte_stream.sif -o byte_stream.next.sif byte_stream.next.sidl
```

Decompile a SIF artifact into canonical SIDL source:

```sh
sidlc decompile -o byte_stream.sidl byte_stream.sif
```

Generate C bindings:

```sh
sidlc generate \
    --arch=x86_64 \
    --lang=c \
    --mode=client \
    --header-dir=gen/sidl \
    --source-path=gen/sidl/byte_stream.c \
    byte_stream.sif
```

## C Generator

The C generator can emit:

* `*.types.h`: shared constants, UUID macros, ABI revision metadata, enums,
  bitfields, and structs.
* `*.h` / `*.client.c`: client-side `Open`, `Query`, and function wrappers
  using `StHandle_Query`, `StHandle_Call*`, and `StHandle_CallN`.
* `*.server.h` / `*.server.c`: server vtable declarations and
  `ServerDispatchArgs` dispatch glue.
* `*.server-client.h` / `*.server-client.c`: client-callable wrappers without
  the `Open`/`Query` handle binding helpers.

## CMake Usage

```cmake
include(UseSIDLC)

sidlc_compile(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SIFS_VAR SIDL_SIFS
    FILES "${SIDLC_INTERFACE_DIRECTORY}/byte_stream.sidl"
)

sidlc_generate(
    CLIENT
    HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SRCS_VAR SIDL_SRCS
    HDRS_VAR SIDL_HDRS
    SIFS ${SIDL_SIFS}
)
```

`sidlc_compile()` creates `.sif` artifacts from `.sidl` sources.
`sidlc_generate()` creates source/header bindings from those `.sif` artifacts.
Pass the generated SIF list to later build steps when they need the compiled
interface artifacts, such as `sma_add_module(PROVIDES ...)`.
