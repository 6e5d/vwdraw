#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <wayland-client.h>
#include <vulkan/vulkan.h>

#include "../../chrono/include/chrono.h"
#include "../../dmgrect/include/dmgrect.h"
#include "../../imgview/include/imgview.h"
#include "../../vwdedit/include/vwdedit.h"
#include "../../vwdedit/include/layout.h"
#include "../../vkstatic/include/oneshot.h"
#include "../../vwdlayer/include/layer.h"
#include "../../vwdlayout/include/command.h"
#include "../../vwdlayout/include/layer.h"
#include "../../vwdlayout/include/vwdlayout.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../../sib/include/simple.h"
#include "../include/lyc.h"

static bool click;
static SibSimple brush;
static Imgview iv;
static Vwdlayout vl;
static Vwdedit ve;
static Simpleimg overlay;
static Vwdlayer *player;
static Dmgrect damage;

static void f_event(Imgview* ivv, uint8_t type, WlezwrapEvent *event) {
	if (type == 3) {
		if (event->key[0] == WLEZWRAP_LCLICK) {
			if (event->key[1]) {
				click = true;
			}
		}
		if (!event->key[1]) {
			click = false;
			sib_simple_finish(&brush);
		}
	} else if (type == 2) {
		if (!click) { return; }
		// TODO: handle coordinate mapping in imgview
		vec2 s, w;
		s[0] = (float)event->motion[0];
		s[1] = (float)event->motion[1];
		float p = (float)event->motion[2];
		imgview_s2w(ivv, s, w);
		sib_simple_update(&brush, &overlay, damage, w[0], w[1], p);
		ivv->dirty = true;
	}
}

static void print_ftime(uint64_t dt) {
	static int skip = 0;
	skip += 1; skip %= 20;
	if (skip != 0) { return; }
	printf("[2Ktf=%lu\r", dt / 1000); fflush(stdout);
}

static Vwdlayer *focus(size_t ldx) {
	printf("focusing %zu\n", ldx);
	Vwdlayer *layer = vwdlayout_ldx(&vl, ldx);
	printf("focus: setting up %ux%u simpleimg\n",
		overlay.width, overlay.height);
	vwdedit_setup(&ve, &iv.vks, &layer->image, (void**)&overlay.data);
	overlay.width = layer->image.size[0];
	overlay.height = layer->image.size[1];
	return layer;
}

static void do_init(uint32_t w, uint32_t h) {
	imgview_init(&iv, w, h);
	vwdlayout_init(&vl, &iv.vks, &iv.img);
	vwdedit_init(&ve, iv.vks.device);
}

int main(int argc, char **argv) {
	bool init = false;
	const uint64_t FTIME = 20000000;
	assert(argc == 2);
	iv.event = f_event;
	Lyc *lyc = NULL;
	size_t llen = lyc_load(&lyc, argv[1]);
	for (size_t lid = 0; lid < llen; lid += 1) {
		printf("preparing layer %zu\n", lid + 1);
		uint32_t w = lyc[lid].img.width;
		uint32_t h = lyc[lid].img.height;
		if (!init) {
			do_init(w, h);
			init = true;
		}
		vwdlayout_insert_layer(&vl, &iv.vks, lid + 1,
			lyc[lid].offset[0], lyc[lid].offset[1], w, h);
		player = focus(lid + 1);
		VkCommandBuffer cbuf = vkstatic_oneshot_begin(&iv.vks);
		printf("copying lyc image to cpu buffer\n");
		memcpy(overlay.data, lyc[lid].img.data, 4 * w * h);
		printf("uploading cpu buffer to paint\n");
		vwdedit_build_command_upload(&ve, iv.vks.device, cbuf);
		// direct copy
		printf("blend paint to focus layer\n");
		vwdedit_build_command(&ve, iv.vks.device, cbuf);
		vkstatic_oneshot_end(cbuf, &iv.vks);
		printf("deinit lyc\n");
		lyc_deinit(&lyc[lid]);
	}
	vwdlayout_descset_write(&vl, iv.vks.device);
	vwdlayout_layer_info(&vl);
	printf("inserted %zu layers\n", llen);
	size_t ldx = 3;
	player = focus(ldx);
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&iv.vks);
	vwdedit_download_layout_layer(&ve, cbuf, player->image.image);
	vkstatic_oneshot_end(cbuf, &iv.vks);

	if (llen > 0) {
		iv.camcon.x = (float)lyc[0].offset[0] -
			(float)lyc[0].img.width / 2;
		iv.camcon.y = (float)lyc[1].offset[1] -
			(float)lyc[1].img.height / 2;
	}
	free(lyc);
	sib_simple_config(&brush);
	ChronoTimer timer;
	while(!iv.quit) {
		chrono_timer_reset(&timer);
		imgview_render_prepare(&iv);
		vwdedit_build_command_upload(&ve, iv.vks.device, iv.vks.cbuf);
		vwdedit_build_command(&ve, iv.vks.device, iv.vks.cbuf);
		vwdlayout_build_command(&vl, iv.vks.device, iv.vks.cbuf);
		imgview_render(&iv);
		uint64_t dt = chrono_timer_finish(&timer);
		print_ftime(dt);
		if (dt < FTIME) {
			chrono_sleep((uint32_t)(FTIME - dt));
		}
	}
	assert(0 == vkDeviceWaitIdle(iv.vks.device));
	vwdedit_deinit(&ve, iv.vks.device);
	vwdlayout_deinit(&vl, iv.vks.device);
	imgview_deinit(&iv);
	return 0;
}

