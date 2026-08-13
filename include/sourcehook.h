/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

// Compatibility shim, not the real header: metamod-source's own
// ISmmPlugin.h (core/ISmmPlugin.h, unmodified upstream code every
// metamod:source plugin's build already needs on its include path)
// unconditionally does a bare #include "sourcehook.h" -- it has no way to
// know this fork moved its real content to sourcehook/sourcehook.h (see
// "Namespace public headers under include/sourcehook/"). This file exists
// purely so that bare #include still resolves, to the exact same real
// header/types (no separate copy, no ABI-mismatch risk) -- anything in
// this repo's own sources or a downstream project's own code should
// still prefer #include "sourcehook/sourcehook.h" directly.
#include "sourcehook/sourcehook.h"
