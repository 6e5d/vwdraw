#include "../include/vwdraw.h"

int main(int argc, char **argv) {
	Vwdraw vwd = {0};
	assert(argc == 2);
	vwdraw_init(&vwd, argv[1]);
	while(!vwd.vv.quit) {
		vwdraw_go(&vwd);
	}
	vwdraw_deinit(&vwd);
	return 0;
}
