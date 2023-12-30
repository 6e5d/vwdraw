#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "../include/vwdraw.h"
#include "../../vector/build/vector.h"
#include "../../ppath/build/ppath.h"

typedef struct {
	int id;
	int ox;
	int oy;
	char *path;
} Info;

static int compare(void *a, void *b) {
	Info *ia = a;
	Info *ib = b;
	return ia->id - ib->id;
}

void vwdraw(lyc_clear_png)(char *path) {
	DIR *dp;
	struct dirent *ep;
	char pngfile[4096];
	printf("path: %s\n", path);
	assert((dp = opendir(path)));
	while ((ep = readdir(dp))) {
		char *p = strrchr(ep->d_name, '.');
		if (p == NULL) { continue; }
		if (0 != strcmp(".png", p)) { continue; }
		printf("rm %s\n", ep->d_name);
		snprintf(pngfile, 4096, "%s/%s", path, ep->d_name);
		assert(0 == unlink(pngfile));
	}
}

size_t vwdraw(lyc_load)(Vwdraw(Lyc) **lycp, char* path) {
	char *abspath = com_6e5d_ppath_abs_new(path);
	DIR *dp;
	struct dirent *ep;
	assert((dp = opendir(path)));
	Vector() infos;
	vector(init)(&infos, sizeof(Info));
	while ((ep = readdir(dp))) {
		char *p = strrchr(ep->d_name, '.');
		if (p == NULL) { continue; }
		if (0 != strcmp(".png", p)) { continue; }
		char *saveptr;
		size_t len = strlen(ep->d_name) + 1;
		char *stok = malloc(len);
		memcpy(stok, ep->d_name, len);
		Info info = {0};
		char *endptr = NULL;
		char *idx = strtok_r(stok, "_", &saveptr);
		info.id = (int32_t)strtol(idx, &endptr, 10);
		if (*endptr != '\0') {
			printf("skipping %s\n", ep->d_name);
			continue;
		}
		idx = strtok_r(NULL, "_", &saveptr);
		info.ox = atoi(idx);
		idx = strtok_r(NULL, "_.", &saveptr);
		info.oy = atoi(idx);
		free(stok);
		info.path = ep->d_name;
		vector(pushback)(&infos, (void *)&info);
	}
	qsort(infos.p, infos.len, infos.size, compare);
	*lycp = malloc(infos.len * sizeof(Vwdraw(Lyc)));
	Vwdraw(Lyc) *pl = *lycp;
	Info *pi = infos.p;
	char *imgpath = NULL;
	for (size_t i = 0; i < infos.len; i += 1, pl += 1, pi += 1) {
		pl->offset[0] = pi->ox;
		pl->offset[1] = pi->oy;
		printf("%s\n", abspath);
		com_6e5d_ppath_rel(&imgpath, abspath, pi->path);
		simpleimg(load)(&pl->img, imgpath);
	}
	free(imgpath);
	free(abspath);

	printf("read %zu layers\n", infos.len);
	vector(deinit)(&infos);
	closedir(dp);
	return infos.len;
}

void vwdraw(lyc_deinit)(Vwdraw(Lyc) *lyc) {
	simpleimg(deinit)(&lyc->img);
}
