#ifndef INCLUDEGUARD_VWDRAW_LYCH
#define INCLUDEGUARD_VWDRAW_LYCH

#include <stdint.h>

#include "../../simpleimg/include/simpleimg.h"

// LaYerCollection
typedef struct {
int32_t offset[2];
Simpleimg img;
} Lyc;

size_t lyc_load(Lyc **lycp, char* path);
void lyc_deinit(Lyc *lycp);

#endif
