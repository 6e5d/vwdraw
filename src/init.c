#include "../include/vwdraw.h"
#include "../include/callback.h"
#include "../include/patch.h"

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
	vwdraw_focus(vwd, (int32_t)ldx);
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	printf("copying lyc image to cpu buffer\n");
	memcpy(vwd->layer.data, lyc->img.data, 4 * dmg.size[0] * dmg.size[1]);
	printf("uploading cpu buffer to paint\n");
	dmg.offset[0] = 0; dmg.offset[1] = 0;
	vwdedit_upload_layer(&vwd->ve, cbuf, &dmg);
	printf("blend paint to focus layer\n");
	vwdedit_blend(&vwd->ve, cbuf, &dmg);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);
	printf("deinit lyc\n");
	vwdraw_lyc_deinit(lyc);
}

void vwdraw_focus(Vwdraw *vwd, int32_t ldx) {
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

void vwdraw_deinit(Vwdraw *vwd) {
	assert(0 == vkDeviceWaitIdle(vwd->iv.vks.device));
	vwdedit_deinit(&vwd->ve, vwd->iv.vks.device);
	vwdlayout_deinit(&vwd->vl, vwd->iv.vks.device);
	imgview_deinit(&vwd->iv);
	vwdview_deinit(&vwd->vv);
	vwdraw_plist_deinit(&vwd->plist);
	free(vwd->path);
}

static void do_init(Vwdraw *vwd, Dmgrect *canvasvp) {
	vwdview_init(&vwd->vv);
	imgview_init(&vwd->iv, vwd->vv.wew.wl.display,
		vwd->vv.wew.wl.surface, canvasvp);
	vwdlayout_init(&vwd->vl, &vwd->iv.vks, canvasvp);
	vwdedit_init(&vwd->ve, vwd->iv.vks.device);
	vwdraw_plist_init(&vwd->plist);
}

void vwdraw_init(Vwdraw *vwd, char *path) {
	// 1. load Lyc
	vwd->focus = -1; // during layer loading layers should be focused 1 by 1
	VwdrawLyc *lyc = NULL;
	size_t llen = vwdraw_lyc_load(&lyc, path);
	vwd->path = strdup(path);
	assert(llen > 0);
	Dmgrect output_area;
	whole_lyc_to_damage(&output_area, &lyc[0]);
	do_init(vwd, &output_area);
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
	vwd->vv.cb_submit = vwdraw_cb_submit;
	vwd->vv.cb_undo = vwdraw_cb_undo;
	vwd->vv.cb_key = vwdraw_cb_key;
	vwd->vv.data = (void*)vwd;
	free(lyc);

	// 2. initial focus
	vwdraw_focus(vwd, 0);

	// 3. configure brush
	sib_simple_config(&vwd->brush);
	vwd->brush.canvas = &vwd->paint;
	dmgrect_init(&vwd->patchdmg);
	dmgrect_init(&vwd->submitundo);
	dmgrect_init(&vwd->brush.pending);
	vwd->vv.brush = (void*)&vwd->brush;
	vwd->vv.ifdraw = sib_simple_ifdraw();
}
