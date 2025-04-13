#include "rarsjs/core.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    assemble((const char*)Data, Size);
    free_runtime();
    return 0;
}


