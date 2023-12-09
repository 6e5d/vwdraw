#ifndef INCLUDEGUARD_VWDRAW
#define INCLUDEGUARD_VWDRAW

#include "../../simpleimg/include/simpleimg.h"
#include "../../imgview/include/imgview.h"
#include "../../dmgrect/include/dmgrect.h"
#include "../../vwdedit/include/vwdedit.h"
#include "../../vector/include/vector.h"
#include "../../vwdlayer/include/vwdlayer.h"
#include "../../vwdlayout/include/vwdlayout.h"
#include "../../vwdview/include/vwdview.h"
#include "../../chrono/include/chrono.h"
#include "../../sib/include/sib.h"

// LaYerCollection
typedef struct {
	int32_t offset[2];
	Simpleimg img;
} VwdrawLyc;
size_t vwdraw_lyc_load(VwdrawLyc **lycp, char* path);
void vwdraw_lyc_deinit(VwdrawLyc *lycp);

typedef struct {
	uint32_t offset[2];
	Simpleimg img;
} VwdrawUpdate;
typedef union {
	VwdrawUpdate update;
	Dmgrect attr;
	bool layout; // is_insert
} VwdrawType;
typedef struct {
	// 0-4 update, attr, layout
	uint32_t ty;
	int32_t ldx;
	VwdrawType data;
} VwdrawPatch;
typedef struct {
	Vector buf; // Vector<VwdrawPatch>
	size_t idx; // VwdrawPatch *p more efficient but not important here
} VwdrawPlist;
void vwdraw_plist_init(VwdrawPlist *plist);
void vwdraw_plist_deinit(VwdrawPlist *plist);
void vwdraw_plist_record_update(VwdrawPlist *plist,
	int32_t ldx, uint32_t area[4], Simpleimg *img);
int32_t vwdraw_plist_walk_layer(VwdrawPlist *plist, Dmgrect *dmg, bool undo);
int32_t vwdraw_plist_walk_update(VwdrawPlist *plist, Simpleimg *img, bool undo);

typedef struct {
	SibSimple brush;
	Imgview iv;
	Vwdview vv;
	Vwdlayout vl;
	Vwdedit ve;
	Simpleimg paint;
	Simpleimg layer;
	Vwdlayer *player;
	Dmgrect patchdmg;
	VwdrawPlist plist;
	ChronoTimer timer;
	int32_t focus; // focus
} Vwdraw;
void vwdraw_init(Vwdraw *vwd, char *path);
void vwdraw_deinit(Vwdraw *vwd);
void vwdraw_go(Vwdraw *vwd);
void vwdraw_plist_debug(VwdrawPlist *plist);
#endif
