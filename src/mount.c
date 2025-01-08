#include <notstd/core.h>
#include <notstd/str.h>

#include <hestia/inutility.h>
#include <hestia/config.h>
#include <hestia/mount.h>

//CAP_SYS_ADMIN
#include <sys/mount.h>
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

	if( hestia_is_systemfs(h->type) ){
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
		if( mount(NULL, ttarget, h->type, h->flags, overmode) ){
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

__private int hierarchy_umount(rootHierarchy_s* h, const char* path, const char* destdir){
	__free char* target = str_printf("%s/%s", path, h->target);
	mforeach(h->child, i){
		hierarchy_umount(&h->child[i], target, destdir);
	}
	
	if( dir_exists(target) ){
		errno = 0;
		umount(target);
		rm(target);
		dbg_info("umount %s: %m", target);
		if( !strcmp(h->type, "overlay") ){
			__free char* upperdir = str_printf("%s/%s.upper", destdir, h->target);
			__free char* workdir  = str_printf("%s/%s.work", destdir, h->target);
			__free char* ttarget  = str_printf("%s/%s.merge", destdir, h->target);
			umount(ttarget);
			rmdir(ttarget);
			rm(upperdir);
			rm(workdir);
		}
	}
	return 0;
}

int hestia_umount(const char* destdir, rootHierarchy_s* root){
	__free char* path = str_printf("%s/root", destdir);
	dbg_info("umount hierarchy %s", path);
	mforeach(root->child, i){
		if( root->child[i].target[0] == HESTIA_CMD_CHR ) continue;
		hierarchy_umount(&root->child[i], path, destdir);
	}
	umount(path);
	rm(path);
	return 0;
}


