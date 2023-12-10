#include <vulkan/vulkan.h>

#include "../../vkhelper2/include/vkhelper2.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../include/vwdraw.h"

static void sync_submit(Vwdraw *vwd, VkCommandBuffer cbuf) {
	// first round blending, submit drawing in prev frame
	// 1
	vwdedit_upload_paint(&vwd->ve, cbuf);
	// 2
	vwdedit_blend(&vwd->ve, cbuf);
	// 3
	vwdedit_download_layer(&vwd->ve, cbuf, &vwd->patchdmg);
	vwd->ve.dmg_paint = vwd->patchdmg;
	// 4
	Vwdlayer *vllayer = vwdlayout_ldx(&vwd->vl, (size_t)vwd->focus);
	vwdedit_copy(cbuf, &vwd->patchdmg, &vllayer->image, &vwd->ve.layer);
	vkhelper2_barrier_shader(cbuf, &vllayer->image);
	vkhelper2_barrier_shader(cbuf, &vwd->ve.layer);
}

static void sync_undo(Vwdraw *vwd, VkCommandBuffer cbuf, Dmgrect *rect) {
	// first round blending, submit drawing in prev frame
	// 1
	vwdedit_upload_paint(&vwd->ve, cbuf);
	// 2
	vwdedit_blend(&vwd->ve, cbuf);
	// 3
	vwdedit_download_layer(&vwd->ve, cbuf, rect);
}

// GPU-CPU sync part, make it as fast as possible
// maybe separate render thread?
static void sync(Vwdraw *vwd) {
	imgview_render_prepare(&vwd->iv);
	vwdedit_upload_paint(&vwd->ve, vwd->iv.vks.cbuf);
	vwdedit_blend(&vwd->ve, vwd->iv.vks.cbuf);
	vwdlayout_build_command(&vwd->vl, vwd->iv.vks.device, vwd->iv.vks.cbuf);
	vwdview_build_camera(&vwd->vv, vwd->iv.uniform.view);
	imgview_render(&vwd->iv, &vwd->vl.output.image);
}

static void focus(Vwdraw *vwd, int32_t ldx) {
	if (ldx == vwd->focus) { return; }
	vwd->focus = ldx;
	if (ldx < 0) { return; }
	assert((size_t)ldx < vwd->vl.layers.len);
	vwd->player = vwdlayout_ldx(&vwd->vl, (size_t)ldx);
	vwdedit_setup(&vwd->ve, &vwd->iv.vks, &vwd->player->image,
		(void**)&vwd->paint.data,
		(void**)&vwd->layer.data);
	vwd->paint.width = vwd->player->image.size[0];
	vwd->paint.height = vwd->player->image.size[1];
	vwd->layer.width = vwd->player->image.size[0];
	vwd->layer.height = vwd->player->image.size[1];
	vwd->vv.offset[0] = vwd->player->offset[0];
	vwd->vv.offset[1] = vwd->player->offset[1];
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	vwdedit_download_layout_layer(&vwd->ve, cbuf, &vwd->player->image);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);
	printf("focus %d: set up %ux%u\n", ldx,
		vwd->paint.width, vwd->paint.height);
}

static void simpleimg_to_dmg(Simpleimg *img, Dmgrect *rect) {
	rect->offset[0] = 0; rect->offset[1] = 0;
	rect->size[0] = img->width;
	rect->size[1] = img->height;
}

static void vwdraw_save(Vwdraw *vwd) {
	vwdraw_lyc_clear_png(vwd->path);
	Dmgrect rect;
	char pngfile[4096];
	for (size_t ldx = 0; ldx < vwd->vl.layers.len; ldx += 1) {
		printf("saving layer: %zu\n", ldx);
		focus(vwd, (int32_t)ldx);
		VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
		simpleimg_to_dmg(&vwd->layer, &rect);
		vwdedit_download_layer(&vwd->ve, cbuf, &rect);
		vkstatic_oneshot_end(cbuf, &vwd->iv.vks);
		snprintf(pngfile, 4096, "%s/%zu_%d_%d.png",
			vwd->path,
			ldx,
			vwd->player->offset[0],
			vwd->player->offset[1]);
		simpleimg_save(&vwd->layer, pngfile);
	}
}

static void key(void *data, uint8_t key, bool pressed) {
	Vwdraw *vwd = data;
	if (key == WLEZWRAP_PROXIMITY) {
		vwd->iv.show_cursor = pressed;
	}
	if (!pressed) { return; }
	if (key == 'e') {
		printf("tool: eraser\n");
		sib_simple_config_eraser(&vwd->brush);
		vwd->ve.pidx = 1;
	} else if (key == 'a') {
		sib_simple_config(&vwd->brush);
		printf("tool: pen\n");
		vwd->ve.pidx = 0;
	} else if (key == 'u') {
		imgview_try_present(&vwd->iv);
		int32_t focus_save = vwd->focus;
		vwdraw_save(vwd);
		focus(vwd, focus_save);
	} else {
		return;
	}
}

static void do_init(Vwdraw *vwd, Dmgrect *canvasvp) {
	vwdview_init(&vwd->vv);
	imgview_init(&vwd->iv, vwd->vv.wew.wl.display,
		vwd->vv.wew.wl.surface, canvasvp);
	vwdlayout_init(&vwd->vl, &vwd->iv.vks, canvasvp);
	vwdedit_init(&vwd->ve, vwd->iv.vks.device);
	vwdraw_plist_init(&vwd->plist);
}

static uint32_t i2u(int32_t v) {
	assert(v >= 0);
	return (uint32_t)v;
}

static void submit(void *data) {
	Vwdraw *vwd = data;
	dmgrect_union(&vwd->patchdmg, &vwd->brush.pending);
	if (dmgrect_is_empty(&vwd->patchdmg)) { return; }
	dmgrect_init(&vwd->brush.pending);
	imgview_try_present(&vwd->iv);
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	sync_submit(vwd, cbuf);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);
	// 5
	simpleimg_clear(&vwd->paint,
		(uint32_t)vwd->patchdmg.offset[0],
		(uint32_t)vwd->patchdmg.offset[1],
		vwd->patchdmg.size[0],
		vwd->patchdmg.size[1]);

	uint32_t area[4] = {
		i2u(vwd->patchdmg.offset[0]), i2u(vwd->patchdmg.offset[1]),
		vwd->patchdmg.size[0], vwd->patchdmg.size[1],
	};
	vwdraw_plist_record_update(&vwd->plist, vwd->focus,
		area, &vwd->layer);
	dmgrect_init(&vwd->patchdmg);
}

static void undo(void *data, bool undo) {
	Vwdraw *vwd = data;
	Dmgrect dmg;
	int32_t ldx = vwdraw_plist_walk_layer(&vwd->plist, &dmg, undo);
	if (ldx < 0) { return; }
	focus(vwd, ldx);

	// fetching
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	sync_undo(vwd, cbuf, &dmg);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);

	if (vwdraw_plist_walk_update(&vwd->plist, &vwd->layer, undo) != 0) {
		return;
	}
	imgview_try_present(&vwd->iv);

	// writing
	cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	vwdedit_upload_layer(&vwd->ve, cbuf, &dmg);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);

	dmgrect_union(&vwd->ve.dmg_paint, &dmg);
	// vwdraw_plist_debug(&vwd->plist);
}

static void whole_lyc_to_damage(Dmgrect *dmg, VwdrawLyc *lyc) {
	int32_t x = lyc->offset[0];
	int32_t y = lyc->offset[1];
	uint32_t w = lyc->img.width;
	uint32_t h = lyc->img.height;
	*dmg = (Dmgrect) {
		.offset = {x, y},
		.size = {w, h},
	};
}

static void vwdraw_load_layer(Vwdraw *vwd, size_t ldx, VwdrawLyc *lyc) {
	Dmgrect dmg;
	whole_lyc_to_damage(&dmg, lyc);
	vwdlayout_insert_layer(&vwd->vl, &vwd->iv.vks, ldx,
		dmg.offset[0], dmg.offset[1],
		dmg.size[0], dmg.size[1]);
	focus(vwd, (int32_t)ldx);
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	printf("copying lyc image to cpu buffer\n");
	memcpy(vwd->layer.data, lyc->img.data, 4 * dmg.size[0] * dmg.size[1]);
	printf("uploading cpu buffer to paint\n");
	vwdedit_damage_all(&vwd->ve);
	vwdedit_upload_layer(&vwd->ve, cbuf, &dmg);
	// direct copy
	printf("blend paint to focus layer\n");
	vwdedit_blend(&vwd->ve, cbuf);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);
	printf("deinit lyc\n");
	vwdraw_lyc_deinit(lyc);
}

void vwdraw_init(Vwdraw *vwd, char *path) {
	// 1. load Lyc
	vwd->focus = -1; // during layer loading layers should be focused 1 by 1
	VwdrawLyc *lyc = NULL;
	size_t llen = vwdraw_lyc_load(&lyc, path);
	vwd->path = strdup(path);
	assert(llen > 0);
	whole_lyc_to_damage(&vwd->ve.dmg_paint, &lyc[0]);
	do_init(vwd, &vwd->ve.dmg_paint);
	for (size_t ldx = 0; ldx < llen; ldx += 1) {
		vwdraw_load_layer(vwd, ldx, &lyc[ldx]);
	}
	vwdlayout_descset_write(&vwd->vl, vwd->iv.vks.device);
	vwdlayout_layer_info(&vwd->vl);
	printf("inserted %zu layers\n", llen);
	printf("adjusting camera\n");
	vwd->vv.camcon.x = (float)lyc[0].offset[0] -
		(float)lyc[0].img.width / 2;
	vwd->vv.camcon.y = (float)lyc[0].offset[1] -
		(float)lyc[0].img.height / 2;
	vwd->vv.cb_submit = submit;
	vwd->vv.cb_undo = undo;
	vwd->vv.cb_key = key;
	vwd->vv.data = (void*)vwd;
	free(lyc);

	// 2. initial focus
	focus(vwd, 0);

	// 3. configure brush
	sib_simple_config(&vwd->brush);
	vwd->brush.canvas = &vwd->paint;
	dmgrect_init(&vwd->brush.pending);
	dmgrect_init(&vwd->patchdmg);
	vwd->vv.brush = (void*)&vwd->brush;
	vwd->vv.ifdraw = sib_simple_ifdraw();
}

void vwdraw_deinit(Vwdraw *vwd) {
	assert(0 == vkDeviceWaitIdle(vwd->iv.vks.device));
	vwdedit_deinit(&vwd->ve, vwd->iv.vks.device);
	vwdlayout_deinit(&vwd->vl, vwd->iv.vks.device);
	imgview_deinit(&vwd->iv);
	vwdview_deinit(&vwd->vv);
	vwdraw_plist_deinit(&vwd->plist);
	free(vwd->path);
}

void vwdraw_go(Vwdraw *vwd) {
	static const uint64_t FTIME = 10000000;
	chrono_timer_reset(&vwd->timer);
	if (vwdview_flush_events(&vwd->vv)) {
		imgview_resize(&vwd->iv, vwd->vv.wew.wl.surface,
			vwd->vv.window_size[0], vwd->vv.window_size[1]);
	}
	// dmg source: draw + undo
	dmgrect_union(&vwd->ve.dmg_paint, &vwd->brush.pending);
	dmgrect_union(&vwd->patchdmg, &vwd->brush.pending);
	dmgrect_init(&vwd->brush.pending);
	imgview_draw_cursor(&vwd->iv, vwd->vv.pps[0], vwd->vv.pps[1],
		vwd->brush.size_b + vwd->brush.size_k);
	sync(vwd);
	dmgrect_init(&vwd->ve.dmg_paint);

	uint64_t dt = chrono_timer_finish(&vwd->timer);
	if (dt < FTIME) {
		chrono_sleep((uint32_t)(FTIME - dt));
	}
}
