#include <vulkan/vulkan.h>

#ifndef INCLUDEGUARD_VWDRAW
#define INCLUDEGUARD_VWDRAW

#include "../../simpleimg/include/simpleimg.h"
#include "../../imgview/include/imgview.h"
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
void vwdraw_lyc_clear_png(char *path);
size_t vwdraw_lyc_load(VwdrawLyc **lycp, char* path);
void vwdraw_lyc_deinit(VwdrawLyc *lycp);

typedef struct {
	Vector buf; // Vector<VwdrawPatch>
	size_t idx; // VwdrawPatch *p more efficient but not important here
} VwdrawPlist;
typedef struct {
	char *path;
	SibSimple brush;
	Imgview iv;
	Vwdview vv;
	Vwdlayout vl;
	Vwdedit ve;
	Simpleimg paint;
	Simpleimg layer;
	Vwdlayer *player;
	Dmgrect patchdmg;
	Dmgrect submitundo;
	VwdrawPlist plist;
	ChronoTimer timer;
	int32_t focus; // focus
} Vwdraw;
void vwdraw_init(Vwdraw *vwd, char *path);
void vwdraw_focus(Vwdraw *vwd, int32_t ldx);
void vwdraw_deinit(Vwdraw *vwd);
void vwdraw_go(Vwdraw *vwd);
void vwdraw_plist_debug(VwdrawPlist *plist);
void vwdraw_flush_pending_paint(Vwdraw *vwd, VkCommandBuffer cbuf);
#endif
