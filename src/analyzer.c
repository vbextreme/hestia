#include <notstd/core.h>
#include <notstd/str.h>

#include <hestia/inutility.h>
#include <hestia/mount.h>
#include <hestia/analyzer.h>

#include <dirent.h>

const char* dtname(unsigned dt){
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

__private void dump_search_mod(char* path, analEnt_s** list){
	DIR* d = opendir(path);
	struct dirent* ent;
	unsigned count = 0;
	if( !d ){
		dbg_error("%s: %m", path);
		return;
	}
	while( (ent=readdir(d)) ){
		if( !strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") ) continue;
		++count;
		__free char* fullname = str_printf("%s/%s", path, ent->d_name);
		if( ent->d_type == DT_DIR ){
			dump_search_mod(fullname, list);
		}
		else{
			unsigned i = mem_ipush(list);
			(*list)[i].type = ent->d_type;
			(*list)[i].path = mem_borrowed(fullname);
		}
	}
	closedir(d);
	if( !count ){
		unsigned i = mem_ipush(list);
		(*list)[i].type = DT_DIR;
		(*list)[i].path = mem_borrowed(path);
	}
}

__private void dump_overlay(const char* path, analEnt_s** list){
	DIR* d = opendir(path);
	struct dirent* ent;
	if( !d ){
		dbg_error("%s: %m", path);
		return;
	}
	while( (ent=readdir(d)) ){
		if( !strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") ) continue;
		__free char* fullname = str_printf("%s/%s", path, ent->d_name);
		if( ent->d_type == DT_DIR ){
			dump_search_mod(fullname, list);
		}
		else{
			unsigned i = mem_ipush(list);
			(*list)[i].type = ent->d_type;
			(*list)[i].path = mem_borrowed(fullname);
		}
	}
	closedir(d);
}

__private void find_overlay(const char* path, analEnt_s** list){
	DIR* d = opendir(path);
	struct dirent* ent;
	if( !d ){
		dbg_error("%s: %m", path);
		return;
	}
	while( (ent=readdir(d)) ){
		if( !strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || !strcmp(ent->d_name, HESTIA_ROOT) ) continue;
		if( ent->d_type != DT_DIR ) continue;
		const char* name = strrchr(ent->d_name, '.');
		if( !name ) continue;
		if( !strcmp(name, ".upper") ){
			__free char* overlay = str_printf("%s/%s", path, ent->d_name);
			dump_overlay(overlay, list);
		}
		else if( !strcmp(name, ".work") || !strcmp(name, ".merge") ){
			continue;
		}
		else{
			__free char* childpath = str_printf("%s/%s", path, ent->d_name);
			find_overlay(childpath, list);
		}
	}
	closedir(d);
}

__private void list_cleanup(void* pan){
	analEnt_s* an = pan;
	mforeach(an, i){
		mem_free(an[i].path);
	}
}

analEnt_s* hestia_analyze_list(const char* destdir){
	analEnt_s* an = MANY(analEnt_s, 32, list_cleanup);
	find_overlay(destdir, &an);
	return an;
}

void hestia_analyze_root(const char* destdir){
	printf(HESTIA_ANALYZER"%s\n", destdir);
	__free analEnt_s* an = hestia_analyze_list(destdir);
	mforeach(an, i){
		printf("[%s]%s\n", dtname(an[i].type), an[i].path);
	}
	puts("");
}


