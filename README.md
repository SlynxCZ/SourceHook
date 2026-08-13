# SourceHook

A standalone build of [metamod-source](https://github.com/alliedmodders/metamod-source)'s
`core/sourcehook` (virtual-function hooking engine + its test suite), plus a
new **inline-hook dispatch** layer for hooking raw, non-virtual functions
(found e.g. by a byte-signature scan) the same way SourceHook already lets
many plugins share one vtable hook.

## Build

Two build systems, same split as [DynLibUtils](https://github.com/Kenzzer/DynLibUtils):

- **AmBuild** (`configure.py` + `AMBuildScript`) — production builds.
  ```bash
  python3 configure.py --enable-optimize --enable-tests
  ambuild objdir/
  ```
- **CMake** (`CMakeLists.txt`) — local/dev builds, with `ctest` wired up.
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build .
  ctest --output-on-failure
  ```

Both produce a static library (`sourcehook`) plus, when tests are enabled,
one or two test binaries (see below).

**Compiler:** requires genuine C++20 (concepts, in particular — used by
`vendor/safetyhook`). Verified with **Clang ≥10** (this repo was built and
tested with **Clang 18**). GCC 9 (Ubuntu 20.04's default `g++`) does **not**
work — it lacks full C++20 concepts support, which `vendor/safetyhook.hpp`
relies on throughout. GCC ≥10 is untested but should work; adjust `CXX`/`CC`
(or AmBuild's compiler detection) accordingly.

## Layout

- Root: SourceHook's engine, copied from `metamod-source/core/sourcehook`
  unmodified except for removing a dead, metamod-only debug helper (see
  "Decoupling from metamod" below) — `sourcehook.h`/`.cpp`,
  `sourcehook_impl*`, `sourcehook_hookmangen*`, `sh_*.h`, `FastDelegate.h`.
- `sourcehook_inline.h`, `sourcehook_impl_inline.h/.cpp` — the new inline-hook
  dispatch layer. Start reading at `sourcehook_inline.h`'s file header.
- `vendor/` — `safetyhook` (the inline-hook engine), `zydis` (safetyhook's
  disassembler dependency), `tl::expected` (safetyhook's error type). Copied
  as plain files from `InventoryManager_mm_es/vendor`, not submodules.
- `test/` — the full upstream SourceHook test suite (`test1.cpp`,
  `testmanual.cpp`, `testvphooks.cpp`, ...), unmodified, plus
  `testinlinehook.cpp` for the new inline-hook dispatch.
- `generate/` — `gen_inline_hooks.py` generates `sourcehook_inline_decl.h`
  (the `SH_DECL_INLINEHOOK0..20` macro family). `generate/upstream_codegen/`
  is metamod-source's own original `sourcehook.hxx` + `shworker` codegen
  tool that produces `sourcehook.h`, kept for provenance; this repo doesn't
  run it.

## Inline hooks

```cpp
#include "sourcehook_inline.h"

SH_DECL_INLINEHOOK1(MyHook, int, CBaseEntity *);

META_RES MyDetour(SourceHook::InlineHookContext<int, CBaseEntity *> &ctx)
{
    CBaseEntity *pEnt = ctx.Arg<0>();
    ctx.SetOverrideRet(1);
    return MRES_SUPERCEDE;
}

// targetAddr is whatever your own signature scan resolved -- SourceHook
// doesn't do the scanning itself.
SH_ADD_INLINEHOOK(MyHook, targetAddr, SH_STATIC(MyDetour), false /* pre */);
SH_ADD_INLINEHOOK(MyHook, targetAddr, SH_MEMBER(this, &MyClass::MyDetour), true /* post */);
SH_REMOVE_INLINEHOOK(MyHook, targetAddr, SH_STATIC(MyDetour), false);
```

If two different `SH_ADD_INLINEHOOK` calls resolve to the **same**
`targetAddr` with the **same** declared signature — the "two plugins found
the same function via a byte signature" case — they share one real
`safetyhook::InlineHook` and one dispatcher; only the first caller for a
given address actually detours anything. A second caller that resolved the
same address but declared an incompatible signature is rejected (returns
`0`) instead of silently double-detouring.

**Scope:** implemented and tested for **Linux x86_64 (SysV ABI)** only. See
`sourcehook_inline.h`'s file header for why (and how), and
`test/testinlinehook.cpp` for what's verified.

## Decoupling from metamod

The only metamod/HL2SDK coupling anywhere in `core/sourcehook` was a dead
debug helper (`PrintDebug()`) in `sourcehook_hookmangen_x86_64.cpp`, called
only from commented-out lines. Removed, along with its five metamod/HL2SDK
includes. Nothing else changed in the ported engine files.

## Known upstream gap (not introduced by this port)

`sourcehook_hookmangen_x86_64.cpp` (the **vtable**-hook JIT generator used by
`SH_DECL_HOOK`/`SH_DECL_MANUALHOOK`) only implements its register-marshaling
paths for MSVC; the GCC/Linux paths are `static_assert(false, "Missing ...
for linux")` stubs, and it uses `xor` as an identifier (a reserved
alternative token under GCC/Clang in C++). metamod-source's own root
`AMBuilder` already excludes this file on Linux x86_64 for exactly this
reason (`core/AMBuilder`: `arch == 'x86_64' and platform != 'linux'`) — this
repo's build files mirror that same exclusion rather than silently trying
(and failing) to compile it. In other words: on Linux x86_64, typed/manual
**vtable** hooks have no working codegen backend here, matching upstream's
own current state. **Inline hooks are unaffected** — they don't use this
file at all.
