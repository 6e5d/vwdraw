#include "../include/vwdraw.h"

// only the VwdrawUpdate need deinit
static void vwdraw_patch_deinit(VwdrawPatch *p) {
	if (p->ty != 0) {
		return;
	}
	simpleimg_deinit(&p->data.update.img);
}

// when add new patch, redo is cleared
static void vwdraw_plist_clear_redo(VwdrawPlist *plist) {
	size_t count = plist->buf.len - plist->idx;
	if (count == 0) { return; }
	printf("dropping %zu redo\n", count);
	for (size_t idx = plist->idx; idx < plist->buf.len; idx += 1) {
		VwdrawPatch *p = vector_offset(&plist->buf, idx);
		vwdraw_patch_deinit(p);
	}
	vector_resize(&plist->buf, plist->idx);
}

static void vwdraw_patch_debug(VwdrawPatch *p) {
	assert(p->ty == 0);
	printf("u%d:", p->ldx);
	VwdrawUpdate *upd = &p->data.update;
	printf("%ux%u ", upd->img.width, upd->img.height);
}

void vwdraw_plist_init(VwdrawPlist *plist) {
	plist->idx = 0;
	vector_init(&plist->buf, sizeof(VwdrawPatch));
}

void vwdraw_plist_deinit(VwdrawPlist *plist) {
	plist->idx = 0;
	vwdraw_plist_clear_redo(plist);
	vector_deinit(&plist->buf);
}

void vwdraw_plist_debug(VwdrawPlist *plist) {
	VwdrawPatch *p = plist->buf.p;
	for (size_t i = 0; i < plist->buf.len; i += 1, p += 1) {
		vwdraw_patch_debug(p);
	}
	printf("\n");
}

void vwdraw_plist_record_update(VwdrawPlist *plist,
	int32_t ldx, uint32_t area[4], Simpleimg *img
) {
	vwdraw_plist_clear_redo(plist);
	VwdrawUpdate upd = { .offset = {area[0], area[1]} };
	simpleimg_new(&upd.img, area[2], area[3]);
	simpleimg_paste(img, &upd.img, area[2], area[3],
		area[0], area[1], 0, 0);
	VwdrawPatch *p = vector_insert(&plist->buf, plist->idx);
	plist->idx += 1;
	*p = (VwdrawPatch) {
		.ty = 0,
		.ldx = ldx,
		.data.update = upd,
	};
}

int32_t vwdraw_plist_walk_layer(VwdrawPlist *plist) {
	VwdrawPatch *p = vector_offset(&plist->buf, plist->idx);
	return p->ldx;
}

// output dmg
int32_t vwdraw_plist_walk_update(Dmgrect *dmg,
	VwdrawPlist *plist, Simpleimg *img, bool undo
) {
	if (
		(plist->idx == 0 && undo) ||
		(plist->idx == plist->buf.len && !undo)
	) {
		printf("cannot undo\n");
		return 1;
	}
	if (undo) { plist->idx -= 1; }
	VwdrawPatch *p = vector_offset(&plist->buf, plist->idx);
	if (!undo) { plist->idx += 1; }

	assert(p->ty == 0); // only support update for now
	VwdrawUpdate *upd = &p->data.update;

	// write dmg
	dmg->size[0] = upd->img.width;
	dmg->size[1] = upd->img.height;
	dmg->offset[0] = (int32_t)upd->offset[0];
	dmg->offset[1] = (int32_t)upd->offset[1];

	{
	// cpu->tmp plist->cpu tmp->plist
	// though swap line by line more efficient
	// undo/redo op are likely to become bottlenecks
		Simpleimg tmp;
		simpleimg_new(&tmp, upd->img.width, upd->img.height);
		simpleimg_paste(img, &tmp, tmp.width, tmp.height,
			upd->offset[0], upd->offset[1], 0, 0);
		simpleimg_paste(&upd->img, img, tmp.width, tmp.height,
			0, 0, upd->offset[0], upd->offset[1]);
		simpleimg_paste(&tmp, &upd->img, tmp.width, tmp.height,
			0, 0, 0, 0);
		simpleimg_deinit(&tmp);
	}
	return 0;
}
