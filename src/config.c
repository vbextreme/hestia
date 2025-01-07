#include "notstd/memory.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>

#include <notstd/core.h>
#include <notstd/str.h>

#include <hestia/inutility.h>
#include <hestia/config.h>

/*
 *
 * s MS_NOSUID
 * x MS_NOEXEC
 * d MS_NODEV
 * r MS_RDONLY
 * t MS_STRICTATIME
 * p MS_PRIVATE
 *
 *
 * system
 * prv 0755
 * sysfs    sys     sxdr
 * devtmpfs dev     s
 * devpts   dev/pts sx   mode=0620,gid=5
 * tmpfs    tmp     sdt
 * prv 0777
 * tmpfs    dev/shm sd mode=1777

 * os.base
 * prv 0755
 * bind /run run p
 * overlay /bin bin p
 * overlay /lib lib p
 * overlay /lib64 lib64 p
 * overlay /opt opt p
 * overlay /usr usr p
 * overlay /var var p
 *
 * os.root
 * prv 0755
 * overlay /root root p
 * overlay /sbin sbin p
 * 
 * os.config
 * prv 0755
 * overlay /etc etc p 
 *
 * auror
 * prv 0777
 * bind build build p
 * bind home home p
 * prv 0755
 * uid %u
 * gid %u
 * dir %D
 *
 * auror.exec
 * exec /build/scriptbuild
 *
 * auror.normal
 * use system.sb
 * use os.base.sb
 * use auror.sb
 * use auror.exec.sb
 *
 * auror.full
 * use system
 * use os.base
 * use os.root
 * use os.config
 * use auror
 * use auror.exec
 *
*/

__private char* config_load(const char* confname){
	__free char* path = str_printf("%s/%s", HESTIA_CONFIG_PATH, confname);
	struct stat info;
	if( stat(path, &info) ) die("unable to get info on file '%s'::%m", path);
	if( info.st_uid != 0 || info.st_gid != 0 ) die("config '%s' required root owner for uid and gid", path);
	if( info.st_mode & S_IWOTH ) die("config '%s' can't share write privilege with others", path);
	return mem_nullterm(load_file(path, 1));
}

__private char* script_check(const char* scriptname){
	char* path = str_printf("%s/%s", HESTIA_SCRIPT_PATH, scriptname);
	struct stat info;
	if( stat(path, &info) ) die("unable to get info on file '%s'::%m", path);
	if( info.st_uid != 0 || info.st_gid != 0 ) die("config '%s' required root owner for uid and gid", path);
	if( info.st_mode & S_IWOTH ) die("config '%s' can't share write privilege with others", path);
	return path;
}

__private char* option_get(const char** parse, int optional, const char* homedir){
	const char* p = *parse;
	p = str_skip_h(p);
	const char* stname = p;
	while( *p && *p != ' ' && *p != '\t' && *p != '\n' ) ++p;
	if( p == stname ){
		if( optional ) return NULL;
		die("aspected option in config");
	}
	*parse = p;
	
	if( p-stname == 2 && !strncmp(stname, "%D", 2) ){
		return str_dup(&homedir[1], 0);
	}
	return str_dup(stname, p - stname);
}

__private unsigned long option_num(const char** parse, int base, uid_t uid, gid_t gid){
	*parse = str_skip_h(*parse);
	const char* p = *parse;
	if( !strncmp(p, "%u", 2) ){
		unsigned long ret = uid;
		p += 2;
		if( *p != ' ' && *p != '\t' && *p != '\n' ) die("invalid char after %%u");
		*parse = p;
		return ret;
	}
	else if( !strncmp(p, "%g", 2) ){
		unsigned long ret = gid;
		p += 2;
		if( *p != ' ' && *p != '\t' && *p != '\n' ) die("invalid char after %%u");
		*parse = p;
		return ret;
	}
	
	errno = 0;
	char* next = NULL;
	unsigned long ret = strtoul(*parse, &next, base);
	if( errno || !next || next == *parse || (*next != ' ' && *next != '\t' && *next != '\n') )
		die("aspected number value");
	*parse = next;
	return ret;
}

__private unsigned option_flags(const char** parse){
	__private char* flagname = "sxdrtp";
	__private unsigned flagvalue[] = { MS_NOSUID, MS_NOEXEC, MS_NODEV, MS_RDONLY, MS_STRICTATIME, MS_PRIVATE };
	
	unsigned ret = 0;
	const char* p = str_skip_h(*parse);
	const char* f;
	while( *p && *p != ' ' && *p != '\t' && *p != '\n' && (f=strchr(flagname, *p)) ){
		ret |= flagvalue[f-flagname];
		++p;
	}
	*parse = p;
	return ret;
}

__private const char* option_end(const char* parse){
	parse = str_skip_h(parse);
	if( *parse != '\n' ) die("aspected end of line at end of line");
	return str_skip_hn(parse);
}

int hestia_is_systemfs(const char* opt){
	__private const char* systemfs[] = {
		"proc",
		"sysfs",
		"devtmpfs",
		"devpts",
		"tmpfs"
	};
	for( unsigned i = 0; i < sizeof_vector(systemfs); ++i ){
		if( !strcmp(opt, systemfs[i]) ) return 1;
	}
	return 0;
}

__private rootHierarchy_s* rh_findchild(rootHierarchy_s* rh, const char* name, unsigned len){
	dbg_info("    findchild '%.*s'", len, name);
	mforeach(rh->child, i){
		if( strlen(rh->child[i].target) == len && !memcmp(rh->child[i].target, name, len) ) return &rh->child[i];
	}
	dbg_info("    not exists");
	return NULL;
}

__private rootHierarchy_s* new_target(rootHierarchy_s* root, const char* dest){
	if( !dest || !*dest || *dest == '.' || *dest == '/' || *dest == '~' )
		die("config invalid destination '%s'", dest);
		
	unsigned len = 0;
	unsigned next = 0;
	const char* ent = NULL;
   	while( (ent=str_tok(dest, "/", 0, &len, &next)) ){
		rootHierarchy_s* next = rh_findchild(root, ent, len);
		if( !next ) break;
		root = next;
	}
	if( !ent ) die("hierarchy '%s' already exists", dest);
	if( dest[next] != 0 ) die("incomplete hierarchy '%s' missed '%.*s'", dest, len, ent);
	
	dbg_info("    entyty: %s", ent);

	root->child = mem_upsize(root->child, 1);
	rootHierarchy_s* ch = &root->child[mem_header(root->child)->len++];
	memset(ch, 0, sizeof(rootHierarchy_s));
	ch->child = MANY(rootHierarchy_s, 1);
	ch->target = str_dup(ent, len);
	return ch;
}

__private void config_parse(rootHierarchy_s* rh, const char* parse, uid_t setuid, gid_t setgid, const char* homedir){
	uid_t uid     = 0;
	gid_t gid     = 0;
	unsigned prv  = 0;
	char** exec = NULL;

	parse = str_skip_hn(parse);
	while( *parse ){
		__free char* cmd = option_get(&parse, 0, homedir);
		dbg_info("cmd: '%s'", cmd);
		if( !strcmp(cmd, "prv") ){
			prv = option_num(&parse, 8, 0, 0);
		}
		else if( !strcmp(cmd, "uid") ){
			uid = option_num(&parse, 10, setuid, setgid);
		}
		else if( !strcmp(cmd, "gid") ){
			gid = option_num(&parse, 10, setuid, setgid);
		}
		else if( hestia_is_systemfs(cmd) ){
			dbg_info("  is system");
			__free char* dest  = option_get(&parse, 0, homedir);
			dbg_info("  new target for '%s'", dest);
			rootHierarchy_s* ch = new_target(rh, dest);
			//ch->target    = dest;
			ch->type      = mem_borrowed(cmd);
			dbg_info("  get flags");
			ch->flags     = option_flags(&parse);
			dbg_info("  get mode");
			ch->mode      = option_get(&parse, 1, homedir);
			ch->privilege = prv;
			ch->gid       = gid;
			ch->uid       = uid;
			dbg_info("  ok");
		}
		else if( !strcmp(cmd, "bind") || !strcmp(cmd, "overlay") ){
			char* src  = option_get(&parse, 0, homedir);
			__free char* dst  = option_get(&parse, 0, homedir);
			rootHierarchy_s* ch = new_target(rh, dst);
			//ch->target    = dst;
			ch->src       = src;
			ch->type      = mem_borrowed(cmd);
			ch->flags     = option_flags(&parse);
			ch->mode      = option_get(&parse, 1, homedir);
			ch->privilege = prv;
			ch->gid       = gid;
			ch->uid       = uid;
		}
		else if( !strcmp(cmd, "dir") ){
			__free char* dst  = option_get(&parse, 0, homedir);
			rootHierarchy_s* ch = new_target(rh, dst);
			//ch->target    = dst;
			ch->type      = mem_borrowed(cmd);
			ch->privilege = prv;
			ch->gid       = gid;
			ch->uid       = uid;
		}
		else if( !strcmp(cmd, "exec") ){
			if( exec ) die("multiple exec, only one exec in sandbox");
			exec = MANY(char*, 4);
			char* argv;
			while( (argv=option_get(&parse, 1, homedir)) ){
				exec = mem_upsize(exec, 1);
				exec[mem_header(exec)->len++] = argv;
			}
		}
		else if( !strcmp(cmd, "use") ){
			__free char* confname = option_get(&parse, 0, homedir);
			__free char* conf = config_load(confname);
			config_parse(rh, conf, setuid, setgid, homedir);
		}
		else if( !strcmp(cmd, "script") ){
			__free char* scriptname  = option_get(&parse, 0, homedir);
			__free char* scriptent   = str_printf("%s/%s", HESTIA_SCRIPT_ENT, scriptname);
			rootHierarchy_s* ch = new_target(rh, scriptent);
			ch->type = mem_borrowed(cmd);
			ch->src  = script_check(scriptname);
		}
		else{
			die("unknown option '%s'", cmd);
		}
		parse = option_end(parse);
	}

	dbg_info("end");
}

rootHierarchy_s* hestia_load(const char* confname, uid_t uid, gid_t gid){
	struct passwd* pwd = getpwuid(uid);
	__free char* home = str_dup(pwd->pw_dir, 0);
	rootHierarchy_s* rh = NEW(rootHierarchy_s);
	memset(rh, 0, sizeof(rootHierarchy_s));
	rh->child = MANY(rootHierarchy_s, 4);
	new_target(rh, HESTIA_SCRIPT_ENT);
	__free char* conf = config_load(confname);
	config_parse(rh, conf, uid, gid, home);
	return rh;
}

__private void hierarchy_analyzer(rootHierarchy_s* h, const char* path, unsigned tab){
	__free char* target = str_printf("%s/%s", path, h->target);
	print_repeats(tab, "  ");
	printf("'%s'::%s %u@%u 0%o ", target, h->type, h->uid, h->gid, h->privilege);
	if( h->flags & MS_NOSUID      ) fputc('s', stdout);
	if( h->flags & MS_NOEXEC      ) fputc('x', stdout);
	if( h->flags & MS_NODEV       ) fputc('d', stdout);
	if( h->flags & MS_RDONLY      ) fputc('r', stdout);
	if( h->flags & MS_STRICTATIME ) fputc('t', stdout);
	if( h->flags & MS_PRIVATE     ) fputc('p', stdout);
	if( h->mode ) printf(" %s", h->mode);
	fputc('\n', stdout);

	mforeach(h->child, i){
		hierarchy_analyzer(&h->child[i], target, tab+1);
	}
}

void hestia_config_analyzer(const char* destdir, rootHierarchy_s* root){
	__free char* target = str_printf("%s/root", destdir);
	printf("%s\n", target);
	mforeach(root->child, i){
		hierarchy_analyzer(&root->child[i], target, 1);
	}
}
















