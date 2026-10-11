// Hand-decompiled game functions: shared switches (see decomp.h).

#include "decomp.h"

REXCVAR_DEFINE_BOOL(decomp_use_originals, false, "Oratan/Decomp",
                    "Developer: decompiled functions call the original recompiled code instead");
REXCVAR_DEFINE_BOOL(decomp_verify, false, "Oratan/Decomp",
                    "Developer: each decompiled function compares itself with the original on "
                    "every input it can construct, the first time it runs, and logs the result");
