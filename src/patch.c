#include "../include/vwdraw.h"
#include "../include/patch.h"

// only the Vwdraw(Update) need deinit
static void vwdraw(patch_deinit)(Vwdraw(Patch) *p) {
	if (p->ty != 0) {
		return;
	}
	simpleimg(deinit)(&p->data.update.img);
}

// when add new patch, redo is cleared
static void vwdraw(plist_clear_redo)(Vwdraw(Plist) *plist) {
	size_t count = plist->buf.len - plist->idx;
	if (count == 0) { return; }
	printf("dropping %zu redo\n", count);
	for (size_t idx = plist->idx; idx < plist->buf.len; idx += 1) {
		Vwdraw(Patch) *p = com_6e5d_vector_offset(&plist->buf, idx);
		vwdraw(patch_deinit)(p);
	}
	com_6e5d_vector_resize(&plist->buf, plist->idx);
}

void vwdraw(plist_init)(Vwdraw(Plist) *plist) {
	plist->idx = 0;
	com_6e5d_vector_init(&plist->buf, sizeof(Vwdraw(Patch)));
}

void vwdraw(plist_deinit)(Vwdraw(Plist) *plist) {
	plist->idx = 0;
	vwdraw(plist_clear_redo)(plist);
	com_6e5d_vector_deinit(&plist->buf);
}

void vwdraw(plist_debug)(Vwdraw(Plist) *plist) {
	fprintf(stderr, "%zu/%zu\n", plist->idx, plist->buf.len);
}

void vwdraw(plist_record_update)(Vwdraw(Plist) *plist,
	int32_t ldx, uint32_t area[4], Simpleimg() *img
) {
	vwdraw(plist_clear_redo)(plist);
	Vwdraw(Update) upd = { .offset = {area[0], area[1]} };
	simpleimg(new)(&upd.img, area[2], area[3]);
	simpleimg(paste)(img, &upd.img, area[2], area[3],
		area[0], area[1], 0, 0);
	Vwdraw(Patch) *p = com_6e5d_vector_insert(&plist->buf, plist->idx);
	plist->idx += 1;
	*p = (Vwdraw(Patch)) {
		.ty = 0,
		.ldx = ldx,
		.data = {
			.update = upd,
		},
	};
}

static void vwdraw(update_patch)(Vwdraw(Update) *upd, Dmgrect() *dmg) {
	dmg->size[0] = upd->img.width;
	dmg->size[1] = upd->img.height;
	dmg->offset[0] = (int32_t)upd->offset[0];
	dmg->offset[1] = (int32_t)upd->offset[1];
}

int32_t vwdraw(plist_walk_layer)(Vwdraw(Plist) *plist, Dmgrect() *dmg, bool undo) {
	if (
		(plist->idx == 0 && undo) ||
		(plist->idx == plist->buf.len && !undo)
	) {
		printf("No more undo/redo\n");
		return(-1);
	}
	if (undo) { plist->idx -= 1; }
	Vwdraw(Patch) *p = com_6e5d_vector_offset(&plist->buf, plist->idx);
	assert(p->ty == 0); // only support update for now
	Vwdraw(Update) *upd = &p->data.update;
	vwdraw(update_patch)(upd, dmg);
	return p->ldx;
}

// take dmg produced by walk_layer
int32_t vwdraw(plist_walk_update)(
	Vwdraw(Plist) *plist, Simpleimg() *img, bool undo
) {
	Vwdraw(Patch) *p = com_6e5d_vector_offset(&plist->buf, plist->idx);
	if (!undo) { plist->idx += 1; }
	Vwdraw(Update) *upd = &p->data.update;
	Dmgrect() dmg;
	vwdraw(update_patch)(upd, &dmg);

	{
	// cpu->tmp plist->cpu tmp->plist
	// though swap line by line more efficient
	// undo/redo op are likely to become bottlenecks
		Simpleimg() tmp;
		simpleimg(new)(&tmp, upd->img.width, upd->img.height);
		simpleimg(paste)(img, &tmp, tmp.width, tmp.height,
			upd->offset[0], upd->offset[1], 0, 0);
		simpleimg(paste)(&upd->img, img, tmp.width, tmp.height,
			0, 0, upd->offset[0], upd->offset[1]);
		simpleimg(paste)(&tmp, &upd->img, tmp.width, tmp.height,
			0, 0, 0, 0);
		simpleimg(deinit)(&tmp);
	}
	return 0;
}
