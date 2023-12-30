#include "../include/vwdraw.h"
#include "../include/callback.h"
#include "../include/patch.h"

static void whole_lyc_to_damage(Dmgrect() *dmg, Vwdraw(Lyc) *lyc) {
	int32_t x = lyc->offset[0];
	int32_t y = lyc->offset[1];
	uint32_t w = lyc->img.width;
	uint32_t h = lyc->img.height;
	*dmg = (Dmgrect()) {
		.offset = {x, y},
		.size = {w, h},
	};
}

static void vwdraw(load_layer)(Vwdraw() *vwd, size_t ldx, Vwdraw(Lyc) *lyc) {
	Dmgrect() dmg;
	whole_lyc_to_damage(&dmg, lyc);
	vwdlayout(insert_layer)(&vwd->vl, &vwd->iv.vks, ldx,
		dmg.offset[0], dmg.offset[1],
		dmg.size[0], dmg.size[1]);
	vwdraw(focus)(vwd, (int32_t)ldx);
	VkCommandBuffer cbuf = vkstatic(oneshot_begin)(&vwd->iv.vks);
	printf("copying lyc image to cpu buffer\n");
	memcpy(vwd->layer.data, lyc->img.data, 4 * dmg.size[0] * dmg.size[1]);
	printf("uploading cpu buffer to paint\n");
	dmg.offset[0] = 0; dmg.offset[1] = 0;
	vwdedit(upload_layer)(&vwd->ve, cbuf, &dmg);
	printf("blend paint to focus layer\n");
	vwdedit(blend)(&vwd->ve, cbuf, &dmg);
	vkstatic(oneshot_end)(cbuf, &vwd->iv.vks);
	printf("deinit lyc\n");
	vwdraw(lyc_deinit)(lyc);
}

static void vwdraw(focus2)(Vwdraw() *vwd, Vwdlayer() *layer) {
	vwd->paint.width = layer->image.size[0];
	vwd->paint.height = layer->image.size[1];
	vwd->layer.width = layer->image.size[0];
	vwd->layer.height = layer->image.size[1];
	vwd->vv.offset[0] = layer->offset[0];
	vwd->vv.offset[1] = layer->offset[1];
	VkCommandBuffer cbuf = vkstatic(oneshot_begin)(&vwd->iv.vks);
	vwdedit(download_layout_layer)(&vwd->ve, cbuf, &layer->image);
	vkstatic(oneshot_end)(cbuf, &vwd->iv.vks);
}

void vwdraw(focus)(Vwdraw() *vwd, int32_t ldx) {
	printf("focus %d\n", ldx);
	if (ldx == vwd->focus) { return; }
	vwd->focus = ldx;
	if (ldx < 0) { return; }
	assert((size_t)ldx < vwd->vl.layers.len);
	vwd->player = vwdlayout(ldx)(&vwd->vl, (size_t)ldx);
	vwdedit(setup)(&vwd->ve, &vwd->iv.vks, &vwd->player->image,
		(void**)&vwd->paint.data,
		(void**)&vwd->layer.data);
	vwdraw(focus2)(vwd, vwd->player);
}

void vwdraw(deinit)(Vwdraw() *vwd) {
	assert(0 == vkDeviceWaitIdle(vwd->iv.vks.device));
	vwdedit(deinit)(&vwd->ve, vwd->iv.vks.device);
	vwdlayout(deinit)(&vwd->vl, vwd->iv.vks.device);
	imgview(deinit)(&vwd->iv);
	vwdview(deinit)(&vwd->vv);
	vwdraw(plist_deinit)(&vwd->plist);
	free(vwd->path);
}

static void do_init(Vwdraw() *vwd, Dmgrect() *canvasvp) {
	vwdview(init)(&vwd->vv);
	imgview(init)(&vwd->iv, vwd->vv.wew.wl.display,
		vwd->vv.wew.wl.surface, canvasvp);
	vwdlayout(init)(&vwd->vl, &vwd->iv.vks, canvasvp);
	vwdedit(init)(&vwd->ve, vwd->iv.vks.device);
	vwdraw(plist_init)(&vwd->plist);
}

void vwdraw(init)(Vwdraw() *vwd, char *path) {
	// 1. load Lyc
	vwd->focus = -1; // during layer loading layers should be focused 1 by 1
	Vwdraw(Lyc) *lyc = NULL;
	size_t llen = vwdraw(lyc_load)(&lyc, path);
	size_t len = strlen(path) + 1;
	vwd->path = malloc(len);
	memcpy(vwd->path, path, len);
	assert(llen > 0);
	Dmgrect() output_area;
	whole_lyc_to_damage(&output_area, &lyc[0]);
	do_init(vwd, &output_area);
	for (size_t ldx = 0; ldx < llen; ldx += 1) {
		vwdraw(load_layer)(vwd, ldx, &lyc[ldx]);
	}
	vwdlayout(descset_write)(&vwd->vl, vwd->iv.vks.device);
	vwdlayout(layer_info)(&vwd->vl);
	printf("inserted %zu layers\n", llen);
	printf("adjusting camera\n");

	vwd->vv.camcon.x = (float)lyc[0].offset[0] -
		(float)lyc[0].img.width / 2;
	vwd->vv.camcon.y = (float)lyc[0].offset[1] -
		(float)lyc[0].img.height / 2;
	vwd->vv.cb_submit = vwdraw(cb_submit);
	vwd->vv.cb_undo = vwdraw(cb_undo);
	vwd->vv.cb_key = vwdraw(cb_key);
	vwd->vv.data = (void*)vwd;
	free(lyc);

	// 2. initial focus
	vwdraw(focus)(vwd, (int32_t)llen - 1);

	// 3. configure brush
	sib(simple_config)(&vwd->brush);
	vwd->brush.canvas = &vwd->paint;
	dmgrect(init)(&vwd->patchdmg);
	dmgrect(init)(&vwd->submitundo);
	dmgrect(init)(&vwd->brush.pending);
	vwd->vv.brush = (void*)&vwd->brush;
	vwd->vv.ifdraw = sib(simple_ifdraw)();
}
