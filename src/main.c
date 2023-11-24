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
#include "../../vwdview/include/vwdview.h"
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
static Vwdview vv;
static Vwdlayout vl;
static Vwdedit ve;
static Simpleimg overlay;
static Vwdlayer *player;
static Dmgrect dmg_stroke;

static void f_event(Vwdview* ivv, uint8_t type, WlezwrapEvent *event) {
	if (type == 3) {
		if (event->key[0] == WLEZWRAP_LCLICK) {
			if (event->key[1]) {
				click = true;
				dmgrect_init(&dmg_stroke);
			}
		}
		if (!event->key[1]) {
			click = false;
			sib_simple_finish(&brush);
			// TODO: submission
		}
	} else if (type == 2) {
		if (!click) { return; }
		// TODO: handle coordinate mapping in imgview
		vec2 s, w;
		s[0] = (float)event->motion[0];
		s[1] = (float)event->motion[1];
		float p = (float)event->motion[2];
		vwdview_s2w(ivv, s, w);
		sib_simple_update(&brush, &overlay, &ve.dmg_paint,
			w[0] - (float)player->offset[0],
			w[1] - (float)player->offset[1],
			p
		);
		dmgrect_union(&dmg_stroke, &ve.dmg_paint);
	}
}

static void focus(size_t ldx) {
	printf("focusing %zu\n", ldx);
	player = vwdlayout_ldx(&vl, ldx);
	printf("focus: setting up %ux%u simpleimg\n",
		overlay.width, overlay.height);
	vwdedit_setup(&ve, &iv.vks, &player->image, (void**)&overlay.data);
	overlay.width = player->image.size[0];
	overlay.height = player->image.size[1];
}

static void do_init(uint32_t w, uint32_t h) {
	vwdview_init(&vv);
	imgview_init(&iv, vv.wew.wl.display, vv.wew.wl.surface, w, h);
	vwdlayout_init(&vl, &iv.vks, &iv.img);
	vwdedit_init(&ve, iv.vks.device);
}

int main(int argc, char **argv) {
	bool init = false;
	const uint64_t FTIME = 20000000;
	assert(argc == 2);
	vv.event = f_event;
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
		focus(lid + 1);
		VkCommandBuffer cbuf = vkstatic_oneshot_begin(&iv.vks);
		printf("copying lyc image to cpu buffer\n");
		memcpy(overlay.data, lyc[lid].img.data, 4 * w * h);
		printf("uploading cpu buffer to paint\n");
		vwdedit_damage_all(&ve);
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
	focus(ldx);
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&iv.vks);
	vwdedit_download_layout_layer(&ve, cbuf, player->image.image);
	vkstatic_oneshot_end(cbuf, &iv.vks);

	if (llen > 0) {
		vv.camcon.x = (float)lyc[0].offset[0] -
			(float)lyc[0].img.width / 2;
		vv.camcon.y = (float)lyc[1].offset[1] -
			(float)lyc[1].img.height / 2;
	}
	free(lyc);
	sib_simple_config(&brush);
	ChronoTimer timer;
	while(!vv.quit) {
		chrono_timer_reset(&timer);
		if (vwdview_flush_events(&vv)) {
			imgview_resize(&iv, vv.wew.wl.surface,
				vv.window_size[0], vv.window_size[1]);
		}
		imgview_render_prepare(&iv);
		vwdedit_build_command_upload(&ve, iv.vks.device, iv.vks.cbuf);
		vwdedit_build_command(&ve, iv.vks.device, iv.vks.cbuf);
		vwdlayout_build_command(&vl, iv.vks.device, iv.vks.cbuf);
		vwdview_build_camera(&vv, iv.uniform.view);
		imgview_render(&iv);
		uint64_t dt = chrono_timer_finish(&timer);
		if (dt < FTIME) {
			chrono_sleep((uint32_t)(FTIME - dt));
		}
	}
	assert(0 == vkDeviceWaitIdle(iv.vks.device));
	vwdedit_deinit(&ve, iv.vks.device);
	vwdlayout_deinit(&vl, iv.vks.device);
	imgview_deinit(&iv);
	vwdview_deinit(&vv);
	return 0;
}

