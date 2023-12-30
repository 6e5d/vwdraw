#include "../../simpleimg/build/simpleimg.h"
#include "../../dmgrect/build/dmgrect.h"

typedef struct {
	uint32_t offset[2];
	Simpleimg() img;
} Vwdraw(Update);
typedef union {
	Vwdraw(Update) update;
	Dmgrect() attr;
	bool layout; // is_insert
} Vwdraw(Type);
typedef struct {
	// 0-4 update, attr, layout
	uint32_t ty;
	int32_t ldx;
	Vwdraw(Type) data;
} Vwdraw(Patch);
void vwdraw(plist_init)(Vwdraw(Plist) *plist);
void vwdraw(plist_deinit)(Vwdraw(Plist) *plist);
void vwdraw(plist_record_update)(Vwdraw(Plist) *plist,
	int32_t ldx, uint32_t area[4], Simpleimg() *img);
int32_t vwdraw(plist_walk_layer)(Vwdraw(Plist) *plist, Dmgrect() *dmg, bool undo);
int32_t vwdraw(plist_walk_update)(Vwdraw(Plist) *plist, Simpleimg() *img, bool undo);
