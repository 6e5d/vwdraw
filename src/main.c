#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#include "../../imgview/include/imgview.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../../simpleimg/include/simpleimg.h"
#include "../include/brush.h"

static bool click, drag;
// double px, py;

static void f_event(Imgview* iv, uint8_t type, WlezwrapEvent *event) {
	if (type == 3) {
		if (event->key[0] == WLEZWRAP_LCLICK) {
			if (event->key[1]) {
				click = true;
			}
		}
		if (!event->key[1]) {
			click = false;
			drag = false;
		}
	} else if (type == 2) {
		if (!click) { return; }
		vec2 s, w;
		s[0] = (float)event->motion[0];
		s[1] = (float)event->motion[1];
		if (drag) {
			imgview_s2w(iv, s, w);
			if (w[0] >= 0.0f && w[0] < (float)iv->vb2.img.width &&
				w[1] >= 0.0f && w[1] < (float)iv->vb2.img.height) {
				imgview_damage_all(iv);
				brush_fcircle(&iv->vb2.img,
					w[0], w[1], 6.0, 1.0);
				iv->dirty = true;
			}
		}
		drag = true;
	}
}

static uint64_t get_usec(void) {
	static struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

static void print_ftime(uint64_t dt) {
	static int skip = 0;
	skip += 1; skip %= 10;
	if (skip != 0) { return; }
	printf("[2Ktf=%lu\r", dt); fflush(stdout);
}

int main(int argc, char **argv) {
	const uint64_t FTIME = 20000;
	Imgview iv = {0};
	iv.event = f_event;
	if (argc >= 2) {
		Simpleimg img;
		simpleimg_load(&img, argv[1]);
		imgview_init(&iv, img.width, img.height);
		memcpy(iv.vb2.img.data, img.data, img.width * img.height * 4);
	} else {
		uint32_t w = 800, h = 600;
		imgview_init(&iv, w, h);
		memset(iv.vb2.img.data, 255, w * h * 4);
	}
	imgview_damage_all(&iv);
	uint64_t time1 = 0, time2 = 0;
	while(!iv.quit) {
		wl_display_roundtrip(iv.wew.wl.display);
		wl_display_dispatch_pending(iv.wew.wl.display);
		time2 = get_usec();
		uint64_t dt = time2 - time1;
		print_ftime(dt);
		if (dt < FTIME) {
			usleep((uint32_t)(FTIME - dt));
		}
		imgview_render(&iv);
		time1 = get_usec();
	}
	imgview_deinit(&iv);
	return 0;
}

