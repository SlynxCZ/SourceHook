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
one or two test binaries (see below). Either build can also be pulled in as
a sub-build from another project instead of building standalone — see
"Using this from another project" below.

**Compiler:** requires genuine C++20 (concepts, in particular — used by
`vendor/safetyhook`). Verified with **Clang ≥10** (this repo was built and
tested with **Clang 18**). GCC 9 (Ubuntu 20.04's default `g++`) does **not**
work — it lacks full C++20 concepts support, which `vendor/safetyhook.hpp`
relies on throughout. GCC ≥10 is untested but should work; adjust `CXX`/`CC`
(or AmBuild's compiler detection) accordingly.

## Layout

- `src/core/` — the base engine: `sourcehook.h`/`.cpp` (also where
  `SH_DECL_HOOK*`/`SH_DECL_MANUALHOOK*`/`SH_DECL_INLINEHOOK*` — all three
  hook styles' declare macros — live together), `sourcehook_impl*`,
  `sourcehook_pibuilder.h`. Copied from `metamod-source/core/sourcehook`
  unmodified except for removing a dead, metamod-only debug helper (see
  "Decoupling from metamod" below).
- `src/hookmangen/` — the vtable-hook JIT code generator
  (`sourcehook_hookmangen*`) that `SH_DECL_HOOK`/`SH_DECL_MANUALHOOK` need
  at runtime. See "Known upstream gap" below for its Linux x86_64 status.
- `src/inline/` — the new inline-hook dispatch layer:
  `sourcehook_inline.h` (start here), `sourcehook_impl_inline.h/.cpp`,
  `sourcehook_metamod_override.h` (opt a metamod:source plugin out of the
  server's shared SourceHook and onto a private instance of this one).
- `src/util/` — low-level building blocks shared by the above:
  `sh_*.h`, `FastDelegate.h`.
- `vendor/` — `safetyhook` (the inline-hook engine), `zydis` (safetyhook's
  disassembler dependency), `tl::expected` (safetyhook's error type). Copied
  as plain files from `InventoryManager_mm_es/vendor`, not submodules.
- `test/` — the full upstream SourceHook test suite (`test1.cpp`,
  `testmanual.cpp`, `testvphooks.cpp`, ...), unmodified, plus
  `testinlinehook.cpp` for the new inline-hook dispatch.
- `generate/` — `gen_inline_hooks.py` generates `sourcehook_inline_decl.h`
  (the `SH_DECL_INLINEHOOK0..20` macro family, `#include`d from
  `src/core/sourcehook.h`). `generate/upstream_codegen/` is
  metamod-source's own original `sourcehook.hxx` + `shworker` codegen tool
  that produces `sourcehook.h`, kept for provenance; this repo doesn't run
  it.

## Using this from another project

Both build systems can pull SourceHook in as a sub-build instead of
listing/globbing its sources yourself:

- **CMake**: `add_subdirectory(vendor/sourcehook)` then
  `target_link_libraries(yourtarget sourcehook)` — its `PUBLIC` include
  dirs (and, transitively, `vendor/safetyhook`'s headers) come along
  automatically. Set `SOURCEHOOK_BUILD_TESTS OFF` first if you don't want
  its own test suite pulled into your build.
- **AmBuild**: same convention metamod-source itself uses for
  `versionlib/AMBuildScript`:
  ```python
  SourceHookLib = builder.Build('vendor/sourcehook/AMBuildScript', {
    'SourceHookBundleVendor': False,  # omit if you have no vendor/{safetyhook,zydis} of your own
  })
  ...
  binary.compiler.linkflags += [SourceHookLib[cxx.target.arch]]
  ```
  `SourceHookBundleVendor=False` skips compiling `vendor/safetyhook`/
  `vendor/zydis` into `libsourcehook` itself (headers are still added to
  the include path either way) — set it if your own project already
  compiles its own copies of those two, to avoid duplicate-symbol link
  errors from two static libraries both containing the same object code.

See `InventoryManager_mm_es`'s `sourcehook_inline` branch for both of these
in real use, including `sourcehook_metamod_override.h` (below).

## Three hook styles, one header

`SH_DECL_HOOK*`, `SH_DECL_MANUALHOOK*`, and `SH_DECL_INLINEHOOK*` are all
declared together in `src/core/sourcehook.h` — inline hooks are a third
option alongside the two vtable-hook styles, not a separate thing bolted on
the side. Only inline hooks' *implementation* (`sourcehook_inline.h`, which
needs `safetyhook.hpp`) is a separate include, kept out of the dependency-free
base header for anyone who only wants typed/manual hooks.

All three styles also now share the same **SH_**-branded macro family for
reading/returning results — `SH_IFACEPTR`/`RETURN_SH`/`RETURN_SH_VALUE`/
`SET_SH_RESULT`/`SHRES_IGNORED`..`SHRES_SUPERCEDE` for typed/manual (vtable)
hooks, `SH_INLINE_IFACEPTR`/`RETURN_SH_INLINE`/`RETURN_SH_INLINE_VALUE`/
`SET_SH_INLINE_RESULT` for inline hooks. The old `META_*`/`MRES_*` names
are kept as plain aliases for existing plugin code — nothing about their
behavior changed, only which name is preferred going forward.

## Inline hooks

Handlers look exactly like a typed/manual hook's generated `Func()`
override: a real function with the hooked function's own signature, not a
context-object wrapper. `this` isn't part of the declared parameter list
either (same as `SH_DECL_HOOK`/`SH_DECL_MANUALHOOK`) — get it via
`SH_INLINE_IFACEPTR`, matching `META_IFACEPTR`'s ergonomics.

```cpp
#include "sourcehook_inline.h"

// free function, no `this` (thisclass = void):
SH_DECL_INLINEHOOK1(MyHook, void, int, CBaseEntity *);

int MyDetour(CBaseEntity *pEnt)
{
    RETURN_SH_INLINE_VALUE(MRES_SUPERCEDE, 1);
}

// non-virtual member function (thisclass = CBaseEntity):
SH_DECL_INLINEHOOK1(MyOtherHook, CBaseEntity, void, int);

void MyOtherDetour(int x)
{
    CBaseEntity *pThis = SH_INLINE_IFACEPTR(CBaseEntity);
    RETURN_SH_INLINE(MRES_IGNORED);
}

// targetAddr is whatever your own signature scan resolved -- SourceHook
// doesn't do the scanning itself.
SH_ADD_INLINEHOOK(MyHook, targetAddr, SH_STATIC(MyDetour), false /* pre */);
SH_ADD_INLINEHOOK(MyOtherHook, otherAddr, SH_MEMBER(this, &MyClass::MyOtherDetour), true /* post */);
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
`sourcehook_inline.h`'s file header for why (and how — `this` is just an
implicit first argument at the machine-code level on this ABI, which is
what makes `thisclass` a template parameter instead of a hand-written JIT
detail), and `test/testinlinehook.cpp` for what's verified (both a free
function and a member function target).

## Decoupling a metamod:source plugin from the server's shared SourceHook

A metamod:source plugin normally shares one SourceHook engine instance
across the whole server (`PLUGIN_SAVEVARS()` points `g_SHPtr`/`g_PLID` at
whatever `ISourceHook` `metamod.so` itself was built with). Including
`src/inline/sourcehook_metamod_override.h` after `<ISmmPlugin.h>` opts a
plugin out of that and onto its own private, plugin-owned instance of
*this* SourceHook instead — e.g. to get `SH_DECL_INLINEHOOK` support
without depending on the target server's metamod build at all:

```cpp
#include <ISmmPlugin.h>
#include "sourcehook_metamod_override.h"
```
```cpp
bool MyPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();
    SH_METAMOD_OVERRIDE_SAVEVARS(id);   // right after, same spirit as PLUGIN_SAVEVARS()
    ...
}
```

See the header itself for the full rationale and the one caveat (hooks
through the private instance aren't coordinated with metamod's shared one,
or any other plugin's, on the same vtable slot/address).

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
