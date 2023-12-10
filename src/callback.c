#include "../include/vwdraw.h"
#include "../include/callback.h"

static uint32_t i2u(int32_t v) {
	assert(v >= 0);
	return (uint32_t)v;
}

static void sync_submit(Vwdraw *vwd, VkCommandBuffer cbuf) {
	vwdraw_flush_pending_paint(vwd, cbuf);
	vwdedit_download_layer(&vwd->ve, cbuf, &vwd->patchdmg);
	Vwdlayer *vllayer = vwdlayout_ldx(&vwd->vl, (size_t)vwd->focus);
	vwdedit_copy(cbuf, &vwd->patchdmg, &vllayer->image, &vwd->ve.layer);
	vkhelper2_barrier_shader(cbuf, &vllayer->image);
	vkhelper2_barrier_shader(cbuf, &vwd->ve.layer);
}

void vwdraw_cb_submit(void *data) {
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

void vwdraw_cb_undo(void *data, bool undo) {
	Vwdraw *vwd = data;
	Dmgrect dmg;
	int32_t ldx = vwdraw_plist_walk_layer(&vwd->plist, &dmg, undo);
	if (ldx < 0) { return; }
	vwdraw_focus(vwd, ldx);

	// fetching
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	vwdraw_flush_pending_paint(vwd, cbuf);
	vwdedit_download_layer(&vwd->ve, cbuf, &dmg);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);

	if (vwdraw_plist_walk_update(&vwd->plist, &vwd->layer, undo) != 0) {
		return;
	}
	imgview_try_present(&vwd->iv);

	// writing
	cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	vwdedit_upload_layer(&vwd->ve, cbuf, &dmg);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);

	vwd->brush.pending = dmg;
	// vwdraw_plist_debug(&vwd->plist);
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
		vwdraw_focus(vwd, (int32_t)ldx);
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

void vwdraw_cb_key(void *data, uint8_t key, bool pressed) {
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
		vwdraw_focus(vwd, focus_save);
	} else if (key == 'k') {
		if (vwd->focus < (int32_t)vwd->vl.layers.len - 1) {
			vwdraw_focus(vwd, vwd->focus + 1);
		} else {
			fprintf(stderr, "already focusing top layer\n");
		}
	} else if (key == 'j') {
		if (vwd->focus > 0) {
			vwdraw_focus(vwd, vwd->focus - 1);
		} else {
			fprintf(stderr, "already focusing bot layer\n");
		}
	} else {
		return;
	}
}
