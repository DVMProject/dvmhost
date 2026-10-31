# AGENTS.md

This file applies to the entire `dvmhost` repository unless a more specific
`AGENTS.md` exists below the file being edited.

## Project character and priorities

`dvmhost` is a C/C++ digital-voice modem, host, networking, and firmware project.
Much of the code implements bit-exact DMR, P25, NXDN, modem, and proprietary wire
protocols. Correct framing, byte order, field widths, timer behavior, and backward
compatibility are more important than stylistic modernization.

When changing code:

1. Preserve observable protocol and configuration behavior unless the task
   explicitly changes it.
2. Match the style of the surrounding subsystem. This repository has a stable
   house style, but some firmware and imported code have their own conventions.
3. Keep patches focused. Do not combine a functional change with broad cleanup,
   renaming, or reformatting.
4. Add or update focused tests for protocol, parser, state-machine, and boundary
   changes.
5. Do not claim standards compliance from an encode/decode round trip alone.
   Prefer independent known-answer vectors or captured, documented wire values.

## Repository layout

- `src/common`: shared codecs, protocol primitives, networking, crypto, and
  utility code.
- `src/host`: the modem host and DMR/P25/NXDN RF processing.
- `src/fne`: fixed-network equipment, call handlers, packet data, and OTAR.
- `src/fw/modem`, `src/fw/hotspot`, `src/fw/v24`: embedded firmware and platform
  code. Treat timing, memory, and hardware constraints as part of the interface.
- `src/bridge`, `src/patch`, `src/remote`, and the TUI applications: separate
  executables that reuse the common library.
- `tests`: Catch2 tests, organized by protocol or subsystem.
- `configs`: annotated example configurations and YAML schemas.
- `docs`: technical notes and interface documentation.

Do not edit build outputs or imported artifacts merely to make them match local
style. In particular, avoid formatting `build/`, platform SDK/CMSIS trees,
third-party single-header libraries, generated files, binaries, and submodules
unless the task specifically targets them.

## C and C++ formatting

There is no repository-wide clang-format file. Follow nearby code and these
observed defaults.

The root `.clang-format` is a conservative aid for newly written or selected
code, not authorization to format the whole tree. Clang-format cannot represent
all existing local choices (including mixed anonymous-namespace, `else`, test
macro, and continuation styles). Review every formatting diff, format only the
changed range when practical, and use `// clang-format off` / `on` around
bit-layout diagrams, alignment-sensitive tables, or macro structures it cannot
preserve.

### Whitespace and layout

- Use four spaces per indentation level. Do not use tabs in project-owned source.
- Keep one statement per line and leave a blank line between logical stages.
- Opening braces for functions and namespaces go on the next line:

  ```cpp
  namespace p25
  {
      bool Frame::decode(const uint8_t* data)
      {
          // ...
      }
  }
  ```

- Class declarations and control statements keep the opening brace on the same
  line:

  ```cpp
  class Frame {
  public:
      void reset();
  };

  if (ready) {
      process();
  }
  else {
      reset();
  }
  ```

- A single short controlled statement may omit braces, matching surrounding code.
  Use braces when a branch is multiline, nested, or likely to gain more work.
- Both `} else {` and a newline before `else` occur in maintained code. Preserve
  the convention of the file or block being changed. `catch` follows the same
  local-style rule.
- Keep `switch` labels aligned with the `switch`; indent statements beneath each
  label. For a scoped case, put the case-local `{` on the following line.
- Wrap long calls and conditions by aligning continuation arguments under the
  first argument or indenting one additional level. Favor readability over a
  rigid column limit; do not reflow unrelated lines.
- Remove trailing whitespace. End text files with a newline.

### Types, literals, and casts

- Use fixed-width integer types (`uint8_t`, `uint16_t`, `uint32_t`, and so on) for
  protocol fields, serialized values, counters with defined widths, and firmware
  interfaces.
- Apply unsigned suffixes to integer literals used with unsigned fields or sizes:
  `0U`, `1U`, `0xFFU`.
- Preserve the project's explicit C-style casts where they are part of existing
  low-level code. In new higher-level C++, prefer `static_cast` when it improves
  safety without making a neighboring block inconsistent.
- Use `nullptr`, not `NULL`, in C++ project code.
- Use `const` aggressively for inputs and values that do not change.
- Keep byte offsets and masks explicit. Use the existing `GET_UINT*`, `SET_UINT*`,
  bit helpers, protocol constants, and length constants instead of open-coded
  endian manipulation.
- Check lengths before indexing or copying. Validate multiplication/addition for
  overflow where untrusted lengths are involved.

### Naming

- Classes and structs use `PascalCase`.
- Functions and local variables generally use `camelCase`.
- Private/protected data members use the `m_` prefix.
- Static/shared members commonly use `s_`; follow the owning class.
- Constants, enumerators, macros, protocol fields, and wire-message identifiers
  generally use `UPPER_SNAKE_CASE` or the established protocol spelling.
- Boolean names should describe a positive condition when practical (`enabled`,
  `authenticated`, `hasMessageNumber`). Preserve public/configuration names for
  compatibility even when their spelling is imperfect.
- Protocol terminology is part of the API. Keep established abbreviations such as
  LLID, RSI, KMM, CSBK, TSBK, TGID, NAC, FNE, DLD, and DLI consistent.

### Includes and namespaces

- Keep the SPDX/license header first, then the matching project header for a
  `.cpp`, then project includes, `using namespace` declarations where the file
  uses them, and standard-library/system includes as established locally.
- Use quoted includes for project headers and angle brackets for standard/system
  headers.
- Do not mechanically reorder includes across an entire file. Add a new include
  to the appropriate existing group.
- Headers must be self-sufficient. Keep include guards in the existing
  `#if !defined(...)` / `#define ...` form and retain the trailing guard comment.
- Nested namespaces use the expanded form already present in the codebase; close
  them with comments such as `} // namespace kmm`.
- Avoid introducing new global `using namespace` directives in headers.

### Classes and ownership

- Order class sections to match neighboring files. The common pattern is public
  API, public property macros/data when required by legacy design, then protected
  or private implementation state.
- Within each access section, declare nested enums/structures/classes and type
  aliases before the data members that use them, then declare data members, and
  place member-function declarations last. In particular, every private data
  member must appear before every private function declaration. Do not interleave
  private helper functions among private state.
- Keep a nested structure next to the logically related member group: declare the
  structure immediately before the first member whose type or state it defines,
  then keep those member variables together. Apply the same rule to nested enums
  and aliases.
- Fully document nested enums, structures, and classes with Doxygen. Nested
  structures require `@brief` and `@ingroup`, and every field requires an
  accurate Doxygen comment. Nested enum values require individual `//!< ...`
  descriptions or equivalent adjacent Doxygen comments.
- Use `explicit` for single-argument constructors unless implicit conversion is
  intentional.
- Use `override` for overridden virtual methods.
- Prefer RAII containers and smart pointers in new host/FNE code. Do not introduce
  heap ownership without a clear destructor/reset path.
- Firmware code may intentionally avoid dynamic allocation or newer library
  facilities. Match its constraints and toolchain.
- Preserve `DECLARE_PROPERTY`, `DECLARE_COPY`, and related project macros when
  extending classes that already use them.
- Initialize members in declaration order. Keep constructor initializer lists in
  that same order.
- Copy/move behavior must be explicit for owning or synchronization-bearing
  classes. Follow existing deletion/copy-helper patterns.

## Source structure

- Keep declarations in the matching `.h` and definitions in `.cpp` unless the
  code is intentionally inline/template-only.
- Organize project-owned headers in the established order when the sections are
  applicable: file Doxygen, include guard, project includes, external/standard
  includes, namespace-level class prototypes, namespace opening, constants,
  namespace-local class prototypes, structure declarations, class declarations,
  and inline/template implementation. Do not add empty sections merely to satisfy
  this list.
- Collect forward declarations under the standard prototype banner instead of
  scattering them between includes or declarations:

  ```cpp
  // ---------------------------------------------------------------------------
  //  Class Prototypes
  // ---------------------------------------------------------------------------
  ```

  Put a prototype in the same namespace as its definition and retain the
  appropriate export macro when the project pattern requires it. Prefer a forward
  declaration over an additional heavy include only when the header needs an
  incomplete type (normally a pointer/reference or friendship). Include the full
  definition when inheritance, by-value storage, inline member access, templates,
  or `sizeof` requires a complete type.
- A new protocol message normally belongs in the corresponding common codec
  directory, is routed through its factory, is integrated at the host/FNE layer,
  is added to the relevant CMake source list or globbed location, and receives a
  focused test.
- Separate parsing/validation, state transition, and output generation when
  practical. Never mutate replay, grant, registration, or transaction state until
  the input has passed the checks required for that mutation.
- Keep wire codecs bounded. Decode APIs that process untrusted or variable-size
  input should receive or otherwise prove the available length before accessing
  optional fields.
- Treat native modem, hotspot, V.24, and network transports as distinct
  capabilities. Do not infer one transport's maximum frame/block size from a
  common logical maximum.
- Keep duplicated firmware changes synchronized between `src/fw/modem` and
  `src/fw/hotspot` when the implementations are intentionally parallel. Verify
  both rather than assuming they are identical.
- Maintain backward compatibility in network packets, configuration keys, REST
  fields, and modem serial framing unless a versioned migration is part of the
  task.

## Comments and documentation

Comments should explain protocol intent, invariants, units, ownership, hardware
constraints, compatibility requirements, or why a non-obvious choice is safe.
Do not narrate obvious syntax.

### File and class API documentation

- Preserve the SPDX line and existing project license/copyright block. Do not
  remove or rewrite prior copyright notices. Add a copyright entry only when
  project policy and the actual authorship call for it; do not guess an author.
- Every class or structure declaration in a project-owned header must be fully
  documented with Doxygen, regardless of access level. This applies to the type
  itself and every declared constructor, destructor, operator, public method,
  protected method, private method, callback, and helper. A function being
  private or obvious does not exempt it from documentation.
- Every full, non-nested class and structure declaration/definition must have its
  own declaration banner immediately before its Doxygen block. Forward
  declarations are the exception: collect those under `Class Prototypes` as
  described above rather than giving each prototype a declaration banner. Nested
  structures do not receive a `Structure Declaration` banner; document them with
  Doxygen directly at their location inside the owning class. Use the exact
  established forms for non-nested declarations:

  ```cpp
  // ---------------------------------------------------------------------------
  //  Class Declaration
  // ---------------------------------------------------------------------------

  /**
   * @brief Implements the core traffic networking logic.
   * @ingroup fne_network
   */
  class TrafficNetwork {
      // ...
  };
  ```

  ```cpp
  // ---------------------------------------------------------------------------
  //  Structure Declaration
  // ---------------------------------------------------------------------------

  /**
   * @brief Represents a queued network request.
   * @ingroup fne_network
   */
  struct NetRequest {
      // ...
  };
  ```

  Repeat the appropriate banner for each separately declared non-nested class or
  structure; do not place multiple unrelated declarations under one declaration
  banner.
- Every class and structure Doxygen block must contain both an accurate `@brief`
  and the correct `@ingroup`. Choose the narrowest existing Doxygen group that
  owns the type; define a new group only when the subsystem genuinely needs one.
- Each function declaration must have an accurate `@brief`. Add one `@param` for
  every named parameter and an `@returns` entry for every non-`void` return value.
  Constructors, destructors, and `void` functions do not need `@returns`.
  Parameter-free functions do not need `@param`. Document template parameters
  with `@tparam` and thrown exceptions with `@throws` when applicable.
- Public and private header declarations use the same complete form:

  ```cpp
  /**
   * @brief Decodes a KMM frame.
   * @param[in] data Encoded frame data.
   * @param length Available data length in bytes.
   * @returns bool True if decoding succeeded; otherwise, false.
   */
  ```

- Use `@param[in]`, `@param[out]`, or `@param[in,out]` where direction matters.
  Document units and ownership for lengths, timers, buffers, and returned memory.
- Keep the Doxygen adjacent to the declaration it describes. Do not use one block
  to document multiple overloads unless the overloads are genuinely identical
  and Doxygen explicitly associates the block with all of them.
- Property macros from `ClassProperties.h` are documented by a Doxygen block at
  each macro invocation describing the logical property. The macro's own generic
  documentation does not replace class-specific property documentation.
- Keep `@file` and `@ingroup` tags consistent with neighboring headers. Several
  headers document the paired `.cpp`; preserve that local convention.
- Implementation definitions commonly use a short block comment immediately
  above the definition, for example `/* Decode a KMM frame. */`. Keep this style
  when adding methods to such a file.

### Constants and Doxygen groups

- Place namespace/file constants under the standard constants banner:

  ```cpp
  // ---------------------------------------------------------------------------
  //  Constants
  // ---------------------------------------------------------------------------
  ```

- Constants that form part of a documented subsystem API should be enclosed in
  that subsystem's Doxygen group, following `P25Defines.h`. Do not add `@name`
  subgroups inside the `@addtogroup` block; they make the generated and source
  documentation unnecessarily noisy:

  ```cpp
  /**
   * @addtogroup p25
   * @{
   */

  /** @brief Encoded frame length in bytes. */
  const uint32_t FRAME_LENGTH_BYTES = 12U;

  /** @} */
  ```

- Use `@addtogroup <existing-group>` when constants, enums, or small namespaces
  belong to an existing documented module and should appear together in its API
  reference. Always close every opened group with `/** @} */` at the correct
  scope.
- Organize a large constants collection into logical contiguous blocks using
  concise ordinary/Doxygen comments where useful, but do not use the Doxygen
  `@name` parameter. Use one correctly scoped `@addtogroup` block and close it
  once with `/** @} */`.
- Give individual constants an accurate `@brief` where their meaning is not
  already unambiguous from a documented named group and adjacent normative
  comment. Include units, encoded width, valid range, and standard section where
  useful.
- Small translation-unit-private implementation constants do not require an
  `@addtogroup` wrapper, but they still belong under the constants banner and
  should be commented when their purpose or units are not obvious.
- Keep constants in the narrowest appropriate namespace/scope. Do not introduce
  macros when a typed `const`/`constexpr` value works, but preserve macros that
  are part of the existing public or preprocessor interface.

## Required common utility macros

Before open-coding bit access, byte-order conversion, class properties, copying,
or temporary runtime arrays, check `src/common/BitManipulation.h`,
`src/common/ClassProperties.h`, and `src/common/VariableLengthArray.h`. Use their
established macros where the operation matches the macro contract. Do not create
a second spelling for the same project idiom.

### `BitManipulation.h`

- Use `WRITE_BIT(buffer, bitOffset, value)` and `READ_BIT(buffer, bitOffset)` for
  MSB-first bit addressing in protocol byte arrays. Do not duplicate the
  `BIT_MASK_TABLE` indexing expression.
- `READ_BIT` yields a masked nonzero value, not a normalized `bool`. Compare it to
  zero or explicitly convert it when a true `bool` is required.
- Use `SET_UINT16`, `SET_UINT24`, and `SET_UINT32` to serialize big-endian integer
  fields, and the matching `GET_UINT16`, `GET_UINT24`, and `GET_UINT32` macros to
  decode them. Do not hand-write shifts for formats these macros cover.
- Validate the complete buffer range before invoking any bit or integer macro.
  The macros do not perform bounds checks.
- Pass side-effect-free arguments. Macro parameters can be evaluated more than
  once; do not pass `index++`, function calls with effects, or expressions that
  mutate the source buffer.
- The `SET_UINT*` macros expand to multiple statements. Use them as standalone
  statements inside an already braced scope; never as the unbraced body of an
  `if`, `else`, or loop.
- Ensure `common/Defines.h` is included before `BitManipulation.h`, normally via
  the subsystem's standard `Defines.h` include order.

### `ClassProperties.h`

- Use `DECLARE_PROPERTY`, `DECLARE_RO_PROPERTY`, and their `PROTECTED` or `PLAIN`
  variants for simple value properties in classes that already follow the
  property-macro pattern. Do not manually add an `m_` field plus trivial getter
  and setter beside macro-based properties.
- Select the narrowest API: prefer read-only over read/write; choose private
  storage unless derived classes require protected access; use a `PLAIN` variant
  only when the surrounding API intentionally omits `get`/`set` prefixes.
- Property macros inject access specifiers and storage. Place them last in the
  applicable `public` property section, as required by `ClassProperties.h`, and
  inspect the expanded access flow before declaring anything after them.
- Add a Doxygen block immediately above every property macro invocation. Describe
  the logical value, units, valid range, and special values where applicable.
- Initialize the macro-generated `m_<variableName>` member in every constructor,
  in declaration order.
- Use property macros only for simple return-by-value/set-by-value types for which
  their generated signatures are appropriate. Use explicit, fully documented
  methods when validation, locking, ownership transfer, reference returns, side
  effects, or nontrivial invariants are required.
- Use `DECLARE_COPY(Type)` or `DECLARE_PROTECTED_COPY(Type)` in classes following
  the project copy-helper pattern. Implement the required `copy(const Type& data)`
  method and copy every relevant base/member field deliberately. Do not combine
  these macros with an independently declared assignment operator.

### `VariableLengthArray.h`

- Use `UInt8Array`, `CharArray`, and `ShortArray` for owned runtime-sized arrays
  that must cross a function boundary or otherwise retain ownership. Prefer these
  aliases to raw `new[]`/`delete[]`.
- Use `DECLARE_UINT8_ARRAY(name, length)` or `DECLARE_CHAR_ARRAY(name, length)`
  for a zero-initialized, scope-local runtime buffer when existing APIs require a
  raw pointer. Use `DECLARE_SHORT_ARRAY(name, length)` for the corresponding
  short-array ownership pattern subject to its zeroing caveat below. Do not use
  compiler-specific C/C++ variable-length stack arrays.
- Treat the exposed raw pointer as a non-owning view. Do not `delete[]` it, return
  it, store it beyond the macro's scope, or move the hidden owner independently.
- Validate the requested length and any size arithmetic before allocation. Pass a
  stable, side-effect-free length expression because the macro uses it more than
  once.
- Lengths passed to `DECLARE_UINT8_ARRAY` and `DECLARE_CHAR_ARRAY` are byte/element
  counts. `DECLARE_SHORT_ARRAY` currently allocates `len` shorts but its zeroing
  expression clears `len` bytes; do not rely on full-array zero initialization
  without first correcting or explicitly accounting for that implementation.
- Prefer `std::array` for compile-time fixed sizes and `std::vector` when resizing,
  iteration, or value semantics are required. The project macros are for their
  established ownership/raw-pointer bridge, not a replacement for every
  container.

### Section banners and inline comments

- Major implementation sections use the existing banner form:

  ```cpp
  // ---------------------------------------------------------------------------
  //  Public Class Members
  // ---------------------------------------------------------------------------
  ```

- New inline comments are sentence fragments in lowercase when they describe the
  next operation, and complete sentences with punctuation when they explain a
  decision. Match the surrounding block.
- Put comments above the code they explain. Use end-of-line comments only for
  short, local facts.
- Use actionable tagged debt comments, preferably with a subsystem identifier:
  `TODO(P25P2-FW): ...`. Explain the missing behavior and the condition for
  removal. Do not add vague `TODO`, `FIXME`, joke, profanity, or author-addressed
  comments.
- Existing informal or blunt legacy comments are not the style for new code.
  Do not broaden a task solely to rewrite them.
- Cite the relevant standard section/table when implementing a subtle normative
  rule, but do not paste copyrighted standards text into source comments.

## Logging and errors

- Use the existing logging facilities and subsystem/category constants instead of
  `printf`, `std::cout`, or ad hoc diagnostics.
- Match established message vocabulary and include identifiers needed to trace a
  transaction: protocol, direction, slot/channel, source/destination ID, message
  type, and reason/status as applicable.
- Never log encryption keys, temporary keys, passwords, preshared keys, full
  sensitive KMM bodies, or other secret material. Packet dumps involving secrets
  must be redacted or omitted even in debug mode.
- Validate first, log a concise reason, clean up partial state, and return the
  established error form. Avoid logging the same failure at every layer.
- Preserve the distinction among malformed input, unsupported service, policy
  denial, transient resource failure, and internal error.

## YAML and configuration files

Configuration compatibility and comments are user-facing interfaces.

### Mandatory AI/LLM notice

The example YAML files state that DVMProject does not support AI/LLM-generated or
AI/LLM-modified configurations. Before editing or generating a user configuration:

1. Tell the user that DVMProject support will not troubleshoot or validate an
   AI-generated or AI-modified configuration.
2. Direct them to the official generator at `tools/dvmcfggen` (the public project
   link is `https://github.com/DVMProject/dvmhost/tree/master/tools/dvmcfggen`).
3. Prefer the generator, schemas, and existing examples for creation/validation.

Do not remove or weaken the notice in example files.

### YAML style

- Preserve the indentation style of the file being changed. Main configuration
  examples use four spaces for nested mappings; list-oriented files such as
  `talkgroup_rules.example.yml` use their established two-space sequence nesting.
  Never use tabs.
- Keep keys in the established section order. The loaders may tolerate arbitrary
  order, but the annotated examples are maintained as human-readable reference
  documents.
- Use lower camel case for configuration keys and preserve existing public key
  names exactly.
- Use lowercase `true` and `false`. Use decimal integers unless a field's existing
  contract/documentation uses another representation.
- Quote strings when they could be parsed as another YAML type, contain special
  characters, are secrets/placeholders, or neighboring examples quote the field.
- Keep one space after `:` and remove trailing whitespace.
- Preserve blank lines and the `#` section-banner style used by the example.
- Place a concise explanatory comment immediately above each example key. State
  units, valid ranges, dependencies, security implications, and restart behavior
  where relevant.
- Align continuation comments using the local `#   ...` convention. Wrap long
  comments without changing their meaning.
- Update every relevant example, schema, loader, default, generator template, and
  documentation location when adding or renaming a configuration option.
- Never silently change example IDs, ports, credentials, RF parameters, keys,
  site relationships, or safety acknowledgments as part of formatting work.
- Parse or schema-validate changed YAML when tooling is available. Syntactic YAML
  validity alone is not proof that a DVM configuration is operationally valid.

## CMake, scripts, and documentation

- Match existing CMake command casing and four-space continuation indentation.
  Keep explicit closing conditions such as `endif (COMPILE_WIN32)` where the
  surrounding file uses them.
- Add new project sources to the correct component list. Do not depend on an
  existing build directory accidentally discovering a new file.
- Shell scripts should retain their current shell and portability assumptions.
  Quote variables used as paths or user input, and avoid new Bash-only syntax in
  scripts intended for `/bin/sh`.
- Python should follow the existing tool's local style; do not apply a repository-
  wide formatter as part of an unrelated change.
- Technical documentation should use the project's established protocol names,
  units, and field spelling. Update documentation when behavior, configuration,
  packet formats, or compatibility changes.

## Testing and verification

Build from the repository's CMake build directory. Reuse an existing configured
build when possible:

```sh
cmake --build build -j2
```

The Catch2 test executable is normally `build/dvmtests` when working inside the
`dvmhost` repository. Run the smallest relevant tag set first, for example:

```sh
./build/dvmtests '[dmr]' --reporter compact
./build/dvmtests '[p25][kmm]' --reporter compact
./build/dvmtests '[p25][pdu]' --reporter compact
./build/dvmtests '[nxdn]' --reporter compact
```

The enclosing superproject may place the executable at
`../build/dvmhost/dvmtests`; use the path produced by the active build tree.

For a change:

- Build every affected target, including firmware variants when firmware changes.
- Run focused tests and then the broader protocol/subsystem suite when practical.
- Add boundary tests for zero, minimum, maximum, one-past-maximum, truncation,
  duplicate, reordered, timeout, and retry cases as applicable.
- For wire codecs, test exact encoded bytes from an independent source in addition
  to round trips.
- For concurrency/state changes, test duplicate delivery, stale completion,
  teardown, restart/cleanup, and lock ordering where feasible.
- For YAML, parse the file and validate against its schema/generator if one exists.
- State clearly when hardware, RF, external equipment, privileged sockets, or a
  restricted environment prevents complete verification.

Do not treat expected negative-test log messages as failures without checking the
test result. Conversely, a successful build is not sufficient verification for a
protocol or configuration change.

## Scope discipline for automated changes

- Inspect the relevant factory, codec, caller, transport, tests, configuration,
  and documentation before changing a protocol feature.
- Preserve unrelated working-tree changes.
- Do not mass-format the repository or normalize legacy files unless explicitly
  requested.
- Do not edit generated output to hide a generator/source problem; fix the source
  and regenerate when the generator is available.
- Do not create standards, protocol constants, configuration options, or hardware
  behavior from assumption. Locate the existing definition or authoritative local
  specification material.
- If behavior is intentionally unsupported, fail explicitly and document the
  supported profile rather than adding a permissive fallback.
