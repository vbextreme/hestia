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

__private int hierarchy_mount(rootHierarchy_s* h, const char* pathParent, const char* destdir){
	__free char* target = str_printf("%s/%s", pathParent, h->target);
	mk_dir(target, h->privilege);
	const char* src   = h->src;
	__free char* bsrc = NULL;
	if( src && src[0] && src[0] != '/' ){
		bsrc = str_printf("%s/%s", pathParent, h->src);
		src = bsrc;
	}

	if( !src && (src=hestia_is_systemfs(h->type)) ){
		dbg_info("mount(%s, %s, %s, %X, %s", src, target, h->type, h->flags, h->mode);
		if( mount(src, target, h->type, h->flags, h->mode) ){
			dbg_error("mount.error src:'%s' '%s'::%s [%s]::%m", h->src, target, h->type, h->mode);
			return -1;
		}
	}
	else if ( !strcmp(h->type, "bind") ){
		if( mount(src, target, h->type, h->flags | MS_BIND, h->mode) ){
			dbg_error("mount.error src:'%s' '%s'::%s [%s]::%m", h->src, target, h->type, h->mode);
			return -1;
		}
	}
	else if( !strcmp(h->type, "overlay") ){
		__free char* upperdir = str_printf("%s/%s.upper", destdir, h->target);
		__free char* workdir  = str_printf("%s/%s.work", destdir, h->target);
		__free char* ttarget  = str_printf("%s/%s.merge", destdir, h->target);
		__free char* overmode = str_printf("%smetacopy=off,lowerdir=%s,upperdir=%s,workdir=%s", (h->mode?h->mode:""), h->src, upperdir, workdir);
		mk_dir(upperdir, h->privilege);
		mk_dir(workdir , h->privilege);
		mk_dir(ttarget , h->privilege);
		if( mount("overlay", ttarget, h->type, h->flags, overmode) ){
			dbg_error("mount.error src:'%s' '%s'::%s [%s@%X]::%m", h->src, target, h->type, overmode, h->flags);
			return -1;
		}
		if( mount(ttarget, target, "bind", MS_BIND, NULL) ){
			dbg_error("mount.error src:'%s' '%s'::%s [%s]::%m", ttarget, target, "bindOverlay", "");
			return -1;
		}
	}
	else if( !strcmp(h->type, "dir") ){
		
	}
	else{
		dbg_error("internal error, unknown type: %s", h->type);
		return -1;
	}
	
	chmod(target, h->privilege);
	if( h->uid || h->gid ) chown(target, h->uid, h->gid);
	
	dbg_info("mounted '%s'::%s %u@%u 0x%X", target, h->type, h->uid, h->gid, h->flags);
	
	mforeach(h->child, i){
		if( hierarchy_mount(&h->child[i], target, destdir) ) return -1;
	}
	return 0;
}

int hestia_mount(const char* destdir, rootHierarchy_s* root){
	__free char* pathRoot = str_printf("%s/root", destdir);
	dbg_info("mount hierarchy %s", pathRoot);
	mk_dir(pathRoot, 0755);
	if( mount(pathRoot, pathRoot, "bind", MS_BIND, NULL) ){
		rm(pathRoot);
		dbg_error("unable to bind new root: %m");
		return -1;
	}
	if( mount(NULL, pathRoot, NULL, MS_REC | MS_PRIVATE, NULL) ){
		dbg_error("unable to set all private");
		return -1;
	}
	mforeach(root->child, i){
		if( root->child[i].target[0] == HESTIA_CMD_CHR ) continue;
		if( hierarchy_mount(&root->child[i], pathRoot, destdir) ) return -1;
	}
	return 0;
}

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


