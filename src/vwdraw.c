#include <cglm/cglm.h>

#include "../../vkhelper2/include/vkhelper2.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../include/vwdraw.h"

void vwdraw_flush_pending_paint(Vwdraw *vwd, VkCommandBuffer cbuf) {
	Dmgrect dmg = vwd->brush.pending;
	dmgrect_union(&dmg, &vwd->submitundo);
	vwdedit_upload_paint(&vwd->ve, cbuf, &dmg);
	vwdedit_blend(&vwd->ve, cbuf, &dmg);
	dmgrect_init(&vwd->brush.pending);
	dmgrect_init(&vwd->submitundo);
}

// GPU-CPU sync part, make it as fast as possible
// maybe separate render thread?
static void sync(Vwdraw *vwd) {
	imgview_render_prepare(&vwd->iv);
	vwdraw_flush_pending_paint(vwd, vwd->iv.vks.cbuf);
	vwdlayout_build_command(&vwd->vl, vwd->iv.vks.device, vwd->iv.vks.cbuf);
	vwdview_build_camera(&vwd->vv, vwd->iv.uniform.view);
	imgview_render(&vwd->iv, &vwd->vl.output.image);
}

static void vwdraw_draw_dots(Vwdraw *vwd) {
	float k = vwd->vv.pps[2];
	if (vwd->vv.input_state != 4) { k = 1.0f; }
	float psize = k * vwd->brush.size_k + vwd->brush.size_b;
	psize *= vwd->brush.size_scale * vwd->vv.camcon.k;
	imgview_draw_cursor(&vwd->iv, vwd->vv.pps[0], vwd->vv.pps[1], psize);

	int32_t x1 = vwd->player->offset[0];
	int32_t y1 = vwd->player->offset[1];
	int32_t x2 = x1 + (int32_t)vwd->player->image.size[0];
	int32_t y2 = y1 + (int32_t)vwd->player->image.size[1];
	float wx = (float)vwd->vv.window_size[0] / 2.0f;
	float wy = (float)vwd->vv.window_size[1] / 2.0f;
	vec4 p1 = {(float)x1, (float)y1, 0.0, 1.0};
	vec4 p2 = {(float)x2, (float)y1, 0.0, 1.0};
	vec4 p3 = {(float)x1, (float)y2, 0.0, 1.0};
	vec4 p4 = {(float)x2, (float)y2, 0.0, 1.0};
	glm_mat4_mulv(vwd->iv.uniform.view, p1, p1);
	glm_mat4_mulv(vwd->iv.uniform.view, p2, p2);
	glm_mat4_mulv(vwd->iv.uniform.view, p3, p3);
	glm_mat4_mulv(vwd->iv.uniform.view, p4, p4);
	int32_t q1x = (int32_t)(p1[0] / p1[3] + wx);
	int32_t q1y = (int32_t)(p1[1] / p1[3] + wy);
	int32_t q2x = (int32_t)(p2[0] / p2[3] + wx);
	int32_t q2y = (int32_t)(p2[1] / p2[3] + wy);
	int32_t q3x = (int32_t)(p3[0] / p3[3] + wx);
	int32_t q3y = (int32_t)(p3[1] / p3[3] + wy);
	int32_t q4x = (int32_t)(p4[0] / p4[3] + wx);
	int32_t q4y = (int32_t)(p4[1] / p4[3] + wy);
	imgview_draw_dashed_line(&vwd->iv, q1x, q1y, q2x, q2y);
	imgview_draw_dashed_line(&vwd->iv, q1x, q1y, q3x, q3y);
	imgview_draw_dashed_line(&vwd->iv, q2x, q2y, q4x, q4y);
	imgview_draw_dashed_line(&vwd->iv, q3x, q3y, q4x, q4y);
}

void vwdraw_go(Vwdraw *vwd) {
	static const uint64_t FTIME = 10000000;
	chrono_timer_reset(&vwd->timer);
	if (vwdview_flush_events(&vwd->vv)) {
		imgview_resize(&vwd->iv, vwd->vv.wew.wl.surface,
			vwd->vv.window_size[0], vwd->vv.window_size[1]);
	}
	vwdraw_draw_dots(vwd);
	dmgrect_union(&vwd->patchdmg, &vwd->brush.pending);
	sync(vwd);
	uint64_t dt = chrono_timer_finish(&vwd->timer);
	if (dt < FTIME) {
		chrono_sleep((uint32_t)(FTIME - dt));
	}
}
