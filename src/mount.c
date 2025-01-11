#include <notstd/core.h>
#include <notstd/str.h>

#include <hestia/inutility.h>
#include <hestia/config.h>
#include <hestia/mount.h>

#include <sys/mount.h>
#include <mntent.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>

__private int mount_cmp(const void* A, const void* B){
	return strcmp(B, A);
}

__private char** mount_list(const char* match){
	unsigned const len = strlen(match);
	struct mntent *ent;
	FILE* mf = setmntent("/proc/mounts", "r");
	if( !mf ) die("unable to get list of mount: %m");
	char** lst = MANY(char*, 24);
	while( (ent = getmntent(mf)) ){
		if( strncmp(ent->mnt_dir, match, len) ) continue;
		unsigned ni = mem_ipush(&lst);
		lst[ni] = str_dup(ent->mnt_dir, 0);
	}
	mem_qsort(lst, mount_cmp);
	endmntent(mf);
	return lst;
}

__private void overlay_rmdir(const char* destdir){
	DIR* d = opendir(destdir);
	if( !d ) die("unable to open destdir");
	struct dirent* ent;
	while( (ent=readdir(d)) ){
		char* name = strrchr(ent->d_name, '.');
		if( !name ) continue;
		if( !strcmp(name, ".upper") || !strcmp(name, ".work") ){
			__free char* path = str_printf("%s/%s", destdir, ent->d_name);
			dbg_info("overlay.rm %s", path);
			rm(path);
		}
	}
	closedir(d);
}

int hestia_umount(const char* destdir){
	char** lstmnt = mount_list(destdir);
	mforeach(lstmnt, i){
		dbg_info("umount %s", lstmnt[i]);
		umount(lstmnt[i]);
		rm(lstmnt[i]);
		mem_free(lstmnt[i]);
	}
	mem_free(lstmnt);
	overlay_rmdir(destdir);
	return 0;
}


