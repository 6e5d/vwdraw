#include "../include/vwdraw.h"

int main(int argc, char **argv) {
	Vwdraw() vwd = {0};
	assert(argc == 2);
	vwdraw(init)(&vwd, argv[1]);
	while(!vwd.vv.quit) {
		vwdraw(go)(&vwd);
	}
	vwdraw(deinit)(&vwd);
	return 0;
}
