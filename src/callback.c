#include "../include/vwdraw.h"
#include "../include/callback.h"
#include "../include/patch.h"

static uint32_t i2u(int32_t v) {
	assert(v >= 0);
	return (uint32_t)v;
}

static void sync_submit(Vwdraw() *vwd, VkCommandBuffer cbuf) {
	vwdraw(flush_pending_paint)(vwd, cbuf);
	vwdedit(download_layer)(&vwd->ve, cbuf, &vwd->patchdmg);
	Vwdlayer() *vllayer = vwdlayout(ldx)(&vwd->vl, (size_t)vwd->focus);
	vwdedit(copy)(cbuf, &vwd->patchdmg, &vllayer->image, &vwd->ve.layer);
	vkhelper2(barrier_shader)(cbuf, &vllayer->image);
	vkhelper2(barrier_shader)(cbuf, &vwd->ve.layer);
}

void vwdraw(cb_submit)(void *data) {
	Vwdraw() *vwd = data;
	dmgrect(union)(&vwd->patchdmg, &vwd->brush.pending);
	if (dmgrect(is_empty)(&vwd->patchdmg)) { return; }
	imgview(try_present)(&vwd->iv);
	VkCommandBuffer cbuf = vkstatic(oneshot_begin)(&vwd->iv.vks);
	sync_submit(vwd, cbuf);
	vkstatic(oneshot_end)(cbuf, &vwd->iv.vks);
	simpleimg(clear)(&vwd->paint,
		(uint32_t)vwd->patchdmg.offset[0],
		(uint32_t)vwd->patchdmg.offset[1],
		vwd->patchdmg.size[0],
		vwd->patchdmg.size[1]);
	uint32_t area[4] = {
		i2u(vwd->patchdmg.offset[0]), i2u(vwd->patchdmg.offset[1]),
		vwd->patchdmg.size[0], vwd->patchdmg.size[1],
	};
	vwdraw(plist_record_update)(&vwd->plist, vwd->focus,
		area, &vwd->layer);
	vwd->submitundo = vwd->patchdmg;
	dmgrect(init)(&vwd->patchdmg);
}

void vwdraw(cb_undo)(void *data, bool undo) {
	Vwdraw() *vwd = data;
	Dmgrect() dmg;
	int32_t ldx = vwdraw(plist_walk_layer)(&vwd->plist, &dmg, undo);
	if (ldx < 0) { return; }
	vwdraw(focus)(vwd, ldx);

	// fetching
	VkCommandBuffer cbuf = vkstatic(oneshot_begin)(&vwd->iv.vks);
	vwdraw(flush_pending_paint)(vwd, cbuf);
	vwdedit(download_layer)(&vwd->ve, cbuf, &dmg);
	vkstatic(oneshot_end)(cbuf, &vwd->iv.vks);

	if (vwdraw(plist_walk_update)(&vwd->plist, &vwd->layer, undo) != 0) {
		return;
	}
	imgview(try_present)(&vwd->iv);

	// writing
	cbuf = vkstatic(oneshot_begin)(&vwd->iv.vks);
	vwdedit(upload_layer)(&vwd->ve, cbuf, &dmg);
	vkstatic(oneshot_end)(cbuf, &vwd->iv.vks);

	vwd->submitundo = dmg;
	// vwdraw(plist_debug)(&vwd->plist);
}

static void simpleimg(to_dmg)(Simpleimg() *img, Dmgrect() *rect) {
	rect->offset[0] = 0; rect->offset[1] = 0;
	rect->size[0] = img->width;
	rect->size[1] = img->height;
}

static void vwdraw(save_layer)(Vwdraw() *vwd, char *filename) {
	Dmgrect() rect;
	VkCommandBuffer cbuf = vkstatic(oneshot_begin)(&vwd->iv.vks);
	simpleimg(to_dmg)(&vwd->layer, &rect);
	vwdedit(download_layer)(&vwd->ve, cbuf, &rect);
	vkstatic(oneshot_end)(cbuf, &vwd->iv.vks);
	simpleimg(save)(&vwd->layer, filename);
}

static void vwdraw(save)(Vwdraw() *vwd) {
	char pngfile[4096];
	vwdraw(lyc_clear_png)(vwd->path);
	for (size_t ldx = 0; ldx < vwd->vl.layers.len; ldx += 1) {
		printf("saving layer: %zu\n", ldx);
		vwdraw(focus)(vwd, (int32_t)ldx);
		snprintf(pngfile, 4096, "%s/%zu_%d_%d.png",
			vwd->path,
			ldx,
			vwd->player->offset[0],
			vwd->player->offset[1]);
		vwdraw(save_layer)(vwd, pngfile);
	}
	VkCommandBuffer cbuf = vkstatic(oneshot_begin)(&vwd->iv.vks);
	vwdlayout(download_output)(&vwd->vl, cbuf);
	vkstatic(oneshot_end)(cbuf, &vwd->iv.vks);
	snprintf(pngfile, 4096, "%s/output.png", vwd->path);
	simpleimg(save)(&vwd->vl.output_img, pngfile);
}

void vwdraw(cb_key)(void *data, uint8_t key, bool pressed) {
	Vwdraw() *vwd = data;
	if (key == wlezwrap(proximity)) {
		vwd->iv.show_cursor = pressed;
	}
	if (!pressed) { return; }
	if (key == 'e') {
		printf("tool: eraser\n");
		sib(simple_config_eraser)(&vwd->brush);
		vwd->ve.pidx = 1;
	} else if (key == 'a') {
		sib(simple_config)(&vwd->brush);
		printf("tool: pen\n");
		vwd->ve.pidx = 0;
	} else if (key == 'u') {
		imgview(try_present)(&vwd->iv);
		int32_t focus_save = vwd->focus;
		vwdraw(save)(vwd);
		vwdraw(focus)(vwd, focus_save);
	} else if (key == 'k') {
		if (vwd->focus < (int32_t)vwd->vl.layers.len - 1) {
			vwdraw(focus)(vwd, vwd->focus + 1);
		} else {
			fprintf(stderr, "already focusing top layer\n");
		}
	} else if (key == 'j') {
		if (vwd->focus > 0) {
			vwdraw(focus)(vwd, vwd->focus - 1);
		} else {
			fprintf(stderr, "already focusing bot layer\n");
		}
	} else {
		return;
	}
}
