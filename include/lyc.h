#pragma once

#include <stdint.h>

#include "../../ltree/include/ltree.h"
#include "../../simpleimg/include/simpleimg.h"
#include "../../wholefile/include/wholefile.h"

// LaYerCollection
typedef struct {
	int32_t offset[2];
	Simpleimg img;
} Lyc;

size_t lyc_load(Lyc **lycp, char* path);
void lyc_deinit(Lyc *lycp);