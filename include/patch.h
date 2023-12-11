#include "../../simpleimg/include/simpleimg.h"
#include "../../dmgrect/include/dmgrect.h"

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
void vwdraw_plist_init(VwdrawPlist *plist);
void vwdraw_plist_deinit(VwdrawPlist *plist);
void vwdraw_plist_record_update(VwdrawPlist *plist,
	int32_t ldx, uint32_t area[4], Simpleimg *img);
int32_t vwdraw_plist_walk_layer(VwdrawPlist *plist, Dmgrect *dmg, bool undo);
int32_t vwdraw_plist_walk_update(VwdrawPlist *plist, Simpleimg *img, bool undo);
