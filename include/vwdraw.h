#include <vulkan/vulkan.h>

#include "../../simpleimg/build/simpleimg.h"
#include "../../imgview/build/imgview.h"
#include "../../vwdedit/build/vwdedit.h"
#include "../../vector/build/vector.h"
#include "../../vwdlayer/build/vwdlayer.h"
#include "../../vwdlayout/build/vwdlayout.h"
#include "../../vwdview/build/vwdview.h"
#include "../../chrono/build/chrono.h"
#include "../../sib/build/sib.h"

// LaYerCollection
typedef struct {
	int32_t offset[2];
	Simpleimg() img;
} Vwdraw(Lyc);
void vwdraw(lyc_clear_png)(char *path);
size_t vwdraw(lyc_load)(Vwdraw(Lyc) **lycp, char* path);
void vwdraw(lyc_deinit)(Vwdraw(Lyc) *lycp);

typedef struct {
	Com_6e5dVector buf; // Vector<Vwdraw(Patch)>
	size_t idx; // Vwdraw(Patch) *p more efficient but not important here
} Vwdraw(Plist);
typedef struct {
	char *path;
	Sib(Simple) brush;
	Imgview() iv;
	Vwdview() vv;
	Vwdlayout() vl;
	Vwdedit() ve;
	Simpleimg() paint;
	Simpleimg() layer;
	Vwdlayer() *player;
	Dmgrect() patchdmg;
	Dmgrect() submitundo;
	Vwdraw(Plist) plist;
	Com_6e5dChronoTimer timer;
	int32_t focus; // focus
} Vwdraw();
void vwdraw(init)(Vwdraw() *vwd, char *path);
void vwdraw(focus)(Vwdraw() *vwd, int32_t ldx);
void vwdraw(deinit)(Vwdraw() *vwd);
void vwdraw(go)(Vwdraw() *vwd);
void vwdraw(plist_debug)(Vwdraw(Plist) *plist);
void vwdraw(flush_pending_paint)(Vwdraw() *vwd, VkCommandBuffer cbuf);
