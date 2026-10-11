// Hand-decompiled game functions: shared switches.
//
// Each file in src/decomp/ replaces recompiled game functions with readable
// C++ that behaves identically. Every replacement hook:
//   - calls the original (__imp__sub_XXXXXXXX) instead when decomp_use_originals
//     is on, as an escape hatch if a rewrite misbehaves;
//   - with decomp_verify on, the first time it runs, compares the rewrite with
//     the original over every input it can construct and logs the result
//     ("decomp verify sub_XXXXXXXX: N inputs, M mismatches").

#pragma once

#include <rex/cvar.h>

REXCVAR_DECLARE(bool, decomp_use_originals);
REXCVAR_DECLARE(bool, decomp_verify);
