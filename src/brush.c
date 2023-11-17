#include <math.h>

#include "../../simpleimg/include/simpleimg.h"
#include "../include/brush.h"

static uint32_t cap(float v, uint32_t max) {
	if (v >= (float)max) {
		return max - 1;
	}
	if (v < 0) {
		return 0;
	}
	return (uint32_t)v;
}

void brush_fcircle(Simpleimg* img, float x, float y, float size, float alpha) {
	float xmaxf = ceilf(x + size);
	float xminf = floorf(x - size);
	float ymaxf = ceilf(y + size);
	float yminf = floorf(y - size);
	uint32_t xmax = cap(xmaxf, img->width);
	uint32_t xmin = cap(xminf, img->width);
	uint32_t ymax = cap(ymaxf, img->height);
	uint32_t ymin = cap(yminf, img->height);
	for (uint32_t i = xmin; i < xmax; i += 1) {
		for (uint32_t j = ymin; j < ymax; j += 1) {
			uint8_t* p = simpleimg_offset(img, i, j);
			*(p + 0) = 0;
			*(p + 1) = 0;
			*(p + 2) = 0;
			*(p + 3) = 255;
		}
	}
}
