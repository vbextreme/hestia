#include <notstd/core.h>
#include <notstd/str.h>

#include <hestia/inutility.h>
#include <hestia/mount.h>

#include <dirent.h>

__private const char* dtname(unsigned dt){
	__private const char* DTNAME[] = {
		[DT_REG]  = "reg",
		[DT_DIR]  = "dir",
		[DT_FIFO] = "fifo",
		[DT_SOCK] = "sock",
		[DT_CHR]  = "chr",
		[DT_BLK]  = "blk",
		[DT_LNK]  = "link"
	};
	return DTNAME[dt];
}

__private void dump_overlay(const char* path){
	DIR* d = opendir(path);
	struct dirent* ent;
	if( !d ){
		dbg_error("%s: %m", path);
		return;
	}
	while( (ent=readdir(d)) ){
		if( !strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") ) continue;
		__free char* tmp = str_printf("%s/%s", path, ent->d_name);
		printf("[%s]%s\n",dtname(ent->d_type), tmp);
		if( ent->d_type == DT_DIR ) dump_overlay(tmp);
	}
	closedir(d);
}

__private void analyze_dump(rootHierarchy_s* h, const char* pathParent, const char* destdir){
	__free char* target = str_printf("%s/%s", pathParent, h->target);
	
	if( !strcmp(h->type, "overlay") ){
		__free char* upperdir = str_printf("%s/%s.upper", destdir, h->target);
		dump_overlay(upperdir);
	}
	
	mforeach(h->child, i){
		analyze_dump(&h->child[i], target, destdir);
	}
}

void hestia_analyze_root(const char* destdir, rootHierarchy_s* root){
	__free char* path = str_printf("%s/root", destdir);
	printf("@analyzer@%s\n", path);
	mforeach(root->child, i){
		if( root->child[i].target[0] == HESTIA_CMD_CHR ) continue;
		analyze_dump(&root->child[i], path, destdir);
	}
}
