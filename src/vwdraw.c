#include <vulkan/vulkan.h>

#include "../../vkhelper2/include/vkhelper2.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../include/vwdraw.h"

// transfer barrier
static void tb(VkCommandBuffer cbuf, VkImageLayout layout, Vkhelper2Image *img
) {
	vkhelper2_barrier(cbuf, layout,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		img);
}

static void sync_submit(Vwdraw *vwd, VkCommandBuffer cbuf) {
	// 1
	vwdedit_upload_draw(&vwd->ve, cbuf);
	// 2
	vwdedit_blend(&vwd->ve, cbuf);
	// 4
	Vwdlayer *vllayer = vwdlayout_ldx(&vwd->vl, (size_t)vwd->focus);
	tb(cbuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vwd->ve.layer);
	tb(cbuf, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &vllayer->image);
	vwdedit_copy(cbuf, &vwd->output, &vllayer->image, &vwd->ve.layer);
	tb(cbuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &vllayer->image);
	tb(cbuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &vwd->ve.layer);
}

// GPU-CPU sync part, make it as fast as possible
// maybe separate render thread?
static void sync(Vwdraw *vwd) {
	imgview_render_prepare(&vwd->iv);
	if (vwd->submit) {
		VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
		sync_submit(vwd, cbuf);
		vkstatic_oneshot_end(cbuf, &vwd->iv.vks);
		// 5
		simpleimg_clear(&vwd->paint,
			(uint32_t)vwd->output.offset[0],
			(uint32_t)vwd->output.offset[1],
			vwd->output.size[0],
			vwd->output.size[1]);
		vwd->ve.dmg_paint = vwd->output;
	}
	vwdedit_upload_draw(&vwd->ve, vwd->iv.vks.cbuf);
	vwdedit_blend(&vwd->ve, vwd->iv.vks.cbuf);
	vwdlayout_build_command(&vwd->vl, vwd->iv.vks.device, vwd->iv.vks.cbuf);
	vwdview_build_camera(&vwd->vv, vwd->iv.uniform.view);
	imgview_render(&vwd->iv, &vwd->vl.output.image);
}

static void focus(Vwdraw *vwd, int32_t ldx) {
	printf("focusing %d\n", ldx);
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
	printf("focus: set up %ux%u\n", vwd->paint.width, vwd->paint.height);
}

static void do_init(Vwdraw *vwd) {
	vwdview_init(&vwd->vv);
	imgview_init(&vwd->iv,
		vwd->vv.wew.wl.display, vwd->vv.wew.wl.surface, &vwd->output);
	vwdlayout_init(&vwd->vl, &vwd->iv.vks, &vwd->output);
	vwdedit_init(&vwd->ve, vwd->iv.vks.device);
	vwdraw_plist_init(&vwd->plist);
}

static void submit(void *data) {
	Vwdraw *vwd = data;
	if (vwd->submit) {
		printf("WARN: double submission in 1 frame\n");
	}
	vwd->submit = true;
}

static void whole_lyc_to_damage(Vwdraw *vwd, VwdrawLyc *lyc) {
	int32_t x = lyc->offset[0];
	int32_t y = lyc->offset[1];
	uint32_t w = lyc->img.width;
	uint32_t h = lyc->img.height;
	vwd->output = (Dmgrect) {
		.offset = {x, y},
		.size = {w, h},
	};
}

static void vwdraw_load_layer(Vwdraw *vwd, size_t ldx, VwdrawLyc *lyc) {
	printf("preparing layer %zu\n", ldx);
	whole_lyc_to_damage(vwd, lyc);
	vwdlayout_insert_layer(&vwd->vl, &vwd->iv.vks, ldx,
		vwd->output.offset[0], vwd->output.offset[1],
		vwd->output.size[0], vwd->output.size[1]);
	focus(vwd, (int32_t)ldx);
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&vwd->iv.vks);
	printf("copying lyc image to cpu buffer\n");
	memcpy(vwd->paint.data, lyc->img.data,
		4 * vwd->output.size[0] * vwd->output.size[1]);
	printf("uploading cpu buffer to paint\n");
	vwdedit_damage_all(&vwd->ve);
	vwdedit_upload_draw(&vwd->ve, cbuf);
	// direct copy
	printf("blend paint to focus layer\n");
	vwdedit_blend(&vwd->ve, cbuf);
	vkstatic_oneshot_end(cbuf, &vwd->iv.vks);
	printf("deinit lyc\n");
	vwdraw_lyc_deinit(lyc);
}

void vwdraw_init(Vwdraw *vwd, char *path) {
	// 1. load Lyc
	VwdrawLyc *lyc = NULL;
	size_t llen = vwdraw_lyc_load(&lyc, path);
	assert(llen > 0);
	whole_lyc_to_damage(vwd, &lyc[0]);
	do_init(vwd);
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
	free(lyc);

	// 2. initial focus
	vwd->focus = 0;
	focus(vwd, vwd->focus);

	// 3. configure brush
	sib_simple_config(&vwd->brush);
	vwd->brush.canvas = &vwd->paint;
	vwd->brush.callback = submit;
	vwd->brush.data = (void*)vwd;
	dmgrect_init(&vwd->brush.pending);
	dmgrect_init(&vwd->output);
	vwd->vv.data = (void*)&vwd->brush;
	vwd->vv.ifdraw = sib_simple_ifdraw();
}

void vwdraw_deinit(Vwdraw *vwd) {
	assert(0 == vkDeviceWaitIdle(vwd->iv.vks.device));
	vwdedit_deinit(&vwd->ve, vwd->iv.vks.device);
	vwdlayout_deinit(&vwd->vl, vwd->iv.vks.device);
	imgview_deinit(&vwd->iv);
	vwdview_deinit(&vwd->vv);
	vwdraw_plist_init(&vwd->plist);
}

static uint32_t i2u(int32_t v) {
	assert(v >= 0);
	return (uint32_t)v;
}

void vwdraw_go(Vwdraw *vwd) {
	static const uint64_t FTIME = 10000000;
	chrono_timer_reset(&vwd->timer);
	if (vwdview_flush_events(&vwd->vv)) {
		imgview_resize(&vwd->iv, vwd->vv.wew.wl.surface,
			vwd->vv.window_size[0], vwd->vv.window_size[1]);
	}
	vwd->ve.dmg_paint = vwd->brush.pending;
	dmgrect_union(&vwd->output, &vwd->brush.pending);
	dmgrect_init(&vwd->brush.pending);
	sync(vwd);
	if (vwd->submit) {
		vwd->submit = false;
		uint32_t area[4] = {
			i2u(vwd->output.offset[0]), i2u(vwd->output.offset[1]),
			vwd->output.size[0], vwd->output.size[1],
		};
		vwdraw_plist_record_update(&vwd->plist,
			vwd->focus,
			area, &vwd->layer);
		dmgrect_init(&vwd->output);
		vwdraw_plist_debug(&vwd->plist);
	}

	uint64_t dt = chrono_timer_finish(&vwd->timer);
	if (dt < FTIME) {
		chrono_sleep((uint32_t)(FTIME - dt));
	}
}
