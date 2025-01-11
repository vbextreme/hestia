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
#include <hestia/system.h>

__private char* config_load(const char* confname){
	__free char* path = str_printf("%s/%s", HESTIA_CONFIG_PATH, confname);
	struct stat info;
	if( stat(path, &info) ) die("unable to get info on file '%s'::%m", path);
	if( info.st_uid != 0 || info.st_gid != 0 ) die("config '%s' required root owner for uid and gid", path);
	if( info.st_mode & S_IWOTH ) die("config '%s' can't share write privilege with others", path);
	return mem_nullterm(load_file(path, 1));
}

//cgroup
//	cgroup.new group
//	cgroup.rule group, [*+-], property, rule
//	cgroup.apply group


__private int vm_mount(configvm_s* vm){
	const char*    src  = vm->current->arg[0].s;
	const char*    dst  = vm->current->arg[1].s;
	const char*    type = vm->current->arg[2].s;
	unsigned       flag = vm->current->arg[3].u;
	const char*    mode = vm->current->arg[4].s;
	unsigned const prv  = vm->current->arg[5].u;
	unsigned const uid  = vm->current->arg[6].u;
	unsigned const gid  = vm->current->arg[7].u;
	dbg_info("mount %s %s %s %X %s (%u:%u::%X)", src, dst, type, flag, mode, uid, gid, prv);
	mk_dir(dst, prv);
	unsigned bindflags = 0;
	if( flag & MS_BIND ){
		bindflags = flag & (~MS_BIND);
		flag = MS_BIND;
	}
	if( mount(src, dst, type, flag, mode) ){
		dbg_error("mount.error src:'%s' '%s'::%s %X [%s]::%m", src, dst, type, flag, mode);
		return -1;
	}
	if( bindflags ){
		if( mount( NULL, dst, NULL, bindflags, NULL) ){
			dbg_error("remount.error src:'%s' '%s'::%s %X [%s]::%m", src, dst, type, bindflags, mode);
		}
	}
	chmod(dst, prv);
	if( uid || gid ) chown(dst, uid, gid);
	return 0;
}

__private int vm_overlay(configvm_s* vm){
	const char*    src  = vm->current->arg[0].s;
	const char*    dst  = vm->current->arg[1].s;
	const char*    dd   = vm->current->arg[2].s;
	const char*    root = vm->current->arg[3].s;
	unsigned const flag = vm->current->arg[4].u;
	const char*    mode = vm->current->arg[5].s;
	unsigned const prv  = vm->current->arg[6].u;
	unsigned const uid  = vm->current->arg[7].u;
	unsigned const gid  = vm->current->arg[8].u;
	
	__free char* upperdir = str_printf("%s/%s.upper", dd, dst);
	__free char* workdir  = str_printf("%s/%s.work", dd, dst);
	__free char* ttarget  = str_printf("%s/%s.merge", dd, dst);
	__free char* overmode = str_printf("%smetacopy=off,lowerdir=%s,upperdir=%s,workdir=%s", (mode?mode:""), src, upperdir, workdir);
	__free char* target   = str_printf("%s/%s", root, dst);
	dbg_info("overlay %s->%s %X %s (%u:%u::%X)", src, target, flag, mode, uid, gid, prv);
	mk_dir(upperdir, prv);
	mk_dir(workdir , prv);
	mk_dir(ttarget , prv);
	mk_dir(target, prv);
	
	if( mount("overlay", ttarget, "overlay", flag, overmode) ){
		dbg_error("mount.error src:'%s' '%s'::%s [%s@%X]::%m", src, dst, "overlay", overmode, flag);
		return -1;
	}
	if( mount(ttarget, target, "bind", MS_BIND, NULL) ){
		dbg_error("mount.error src:'%s' '%s'::%s [%s]::%m", ttarget, target, "bindOverlay", "");
		return -1;
	}
	chmod(target, prv);
	if( uid || gid ) chown(dst, uid, gid);
	return 0;
}

__private int vm_dir(configvm_s* vm){
	dbg_info("dir %s (%lu:%lu::%lX)", vm->current->arg[0].s, vm->current->arg[2].u, vm->current->arg[3].u, vm->current->arg[1].u);
	mk_dir(vm->current->arg[0].s, vm->current->arg[1].u);
	if( vm->current->arg[2].u || vm->current->arg[3].u ) chown(vm->current->arg[0].s, vm->current->arg[2].u, vm->current->arg[3].u);
	return 0;
}

__private int vm_script(configvm_s* vm){
	dbg_info("shell %s", vm->current->arg[0].s);
	if( shell(vm->current->arg[0].s) ) return -1;
	return 0;
}

//[s0] rootdir
__private int vm_change_root(configvm_s* vm){
	dbg_info("changeroot %s", vm->current->arg[0].s);
	return change_root(vm->current->arg[0].s);
}

__private int vm_seccomp(configvm_s* vm){
	syscall_end(&vm->filter, vm->current->arg[0].u);
	return syscall_apply(vm->filter);
}

//[u0] uid [u1] gid
__private int vm_privilege_drop(configvm_s* vm){
	dbg_info("dropprivilege (%lu:%lu)", vm->current->arg[0].u, vm->current->arg[1].u);
	return privilege_drop(vm->current->arg[0].u, vm->current->arg[1].u);
}

__private int vm_chdir(configvm_s* vm){
	dbg_info("chdir %s", vm->current->arg[0].s);
	return chdir(vm->current->arg[0].s);
}

//[s0] argv
__private int vm_exec(configvm_s* vm){
	dbg_info("exec %s", vm->current->arg[0].as[0]);
	execv(vm->current->arg[0].as[0], vm->current->arg[0].as);
	dbg_error("exec %s: %m", vm->current->arg[0].as[0]);
	return 1;
}

__private cbc_s* cbc_new(void){
	cbc_s* bc = NEW(cbc_s);
	ld_ctor(bc);
	return bc;
}

__private configvm_s* vm_new(void){
	configvm_s* vm = NEW(configvm_s);
	vm->current = NULL;
	vm->filter  = NULL;
	vm->flags   = 0;
	vm->stage   = NULL;
	return vm;
}

__private int vm_run(configvm_s* vm, cbc_s* stage){
	ldforeach(stage, it){
		vm->current = it;
		if( vm->current->fn(vm) ) return -1;
	}
	return 0;
}

int config_vm_run(configvm_s* vm){
	return vm_run(vm, vm->stage);
}

int config_vm_atexit(configvm_s* vm, int ret){
	return vm_run(vm, ret ? vm->onfail : vm->atexit );
}

typedef struct configp{
	char*       destdir;
	char*       rootdir;
	char*       homedir;
	cbc_s*      mountpoint;
	cbc_s*      scriptMount;
	cbc_s*      scriptRoot;
	cbc_s*      scriptAtExit;
	cbc_s*      scriptOnFail;
	cbc_s*      chdir;
	unsigned    allowDeny;
	unsigned    guid;
	unsigned    ggid;
	unsigned    uid;
	unsigned    gid;
	unsigned    prv;
	configvm_s* vm;
	const char* scrArg;
	option_s*   execArg;
}configp_s;

typedef void(*parse_f)(configp_s* conf, unsigned count, char* token[MAX_TOKEN]);

//	cmd arg,arg,arg -> [cmd,arg,arg,arg,...]
//	parse_[0]([])
//	addvm
__private void token_required(unsigned const required, unsigned const count, char* token[MAX_TOKEN]){
	if( count < required ) die("required %u arguments", count);
	for( unsigned i = 0; i < required; ++i ){
		if(!token[i] || !*token[i] ) die("argument %u is not optional", i);
	}
}

__private unsigned long token_unum(const char* token, int base){
	errno = 0;
	char* next = NULL;
	unsigned long ret = strtoul(token, &next, base);
	if( errno || !next || next == token || *next ) die("aspected number");
	return ret;
}

__private unsigned long token_id(const char* token, unsigned id){
	if( !token  ) return id;
	if( !*token ) return id;
	if( !strcmp(token, "%i") ) return id;
	return token_unum(token, 10);
}

__private const char* token_systype(const char* token){
	if( !token ) die("required valid systype");
	__private const char* systemfs[] = {
		"proc",
		"sysfs",
		"devtmpfs",
		"devpts",
		"tmpfs"
	};
	__private const char* systemname[] = {
		"proc",
		"sys",
		"dev",
		"devpts",
		"tmpfs"
	};
	for( unsigned i = 0; i < sizeof_vector(systemfs); ++i ){
		if( !strcmp(token, systemfs[i]) ) return systemname[i];
	}
	die("required valid systype");
}

__private unsigned token_mountflags(const char* token){
	__private char* flagname = "sxdrtp";
	__private unsigned flagvalue[] = { MS_NOSUID, MS_NOEXEC, MS_NODEV, MS_RDONLY, MS_STRICTATIME, MS_PRIVATE };
	if( !token ) return 0;
	unsigned ret = 0;
	const char* f;
	while( *token && (f=strchr(flagname, *token)) ){
		ret |= flagvalue[f-flagname];
		++token;
	}
	return ret;
}

__private unsigned sub_rwx(const char* p){
	unsigned ret = 0;
	if( *p == 'r' ) ret |= 0x4;
	else if( *p != '-' ) die("invalid privilege, apsected r or -");
	++p;
	if( *p == 'w' ) ret |= 0x2;
	else if( *p != '-' ) die("invalid privilege, apsected w or -");
	++p;
	if( *p == 'x' ) ret |= 0x1;
	else if( *p != '-' ) die("invalid privilege, apsected x or -");
	return ret;
}

__private unsigned token_privilege(const char* token, unsigned prv){
	if( !token  ) return prv;
	if( !*token ) return prv;
	if( *token == '0' ) return token_unum(token, 8);
	prv = 0;
	prv |= sub_rwx(token) << 6;
	token += 3;
	if( !*token ) return prv;
	prv |= sub_rwx(token) << 3;
	token += 3;
	if( !*token ) return prv;
	prv |= sub_rwx(token);
	token += 3;
	if( *token ) die("unaspected char after privilege");
	return prv;
}

__private char* token_script(const char* token){
	if( !token || !*token ) die("aspected script name");
	char* path = str_printf("%s/%s", HESTIA_SCRIPT_PATH, token);
	struct stat info;
	if( stat(path, &info) ) die("unable to get info on file '%s'::%m", path);
	if( info.st_uid != 0 || info.st_gid != 0 ) die("config '%s' required root owner for uid and gid", path);
	if( info.st_mode & S_IWOTH ) die("config '%s' can't share write privilege with others", path);
	return path;
}

__private void build_file(configp_s* conf, const char* lines);

__private void p_use(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	if( count != 2 ) die("use: invalid numbers of args, aspected 2 args give %u", count);
	__free char* path = str_printf("%s/%s", HESTIA_CONFIG_PATH, token[1]);
	struct stat info;
	if( stat(path, &info) ) die("unable to get info on file '%s'::%m", path);
	if( info.st_uid != 0 || info.st_gid != 0 ) die("config '%s' required root owner for uid and gid", path);
	if( info.st_mode & S_IWOTH ) die("config '%s' can't share write privilege with others", path);
	__free char* buf = mem_nullterm(load_file(path, 1));
	uid_t    oldu = conf->uid;
	gid_t    oldg = conf->gid;
	unsigned oldp = conf->prv;
	build_file(conf, buf);
	conf->uid = oldu;
	conf->gid = oldg;
	conf->prv = oldp;

}

__private void p_uid(configp_s* conf, __unused unsigned count, char* token[MAX_TOKEN]){
	conf->uid = token_id(token[1], conf->guid);
}

__private void p_gid(configp_s* conf, __unused unsigned count, char* token[MAX_TOKEN]){
	conf->gid = token_id(token[1], conf->ggid);
}

__private void p_prv(configp_s* conf, __unused unsigned count, char* token[MAX_TOKEN]){
	conf->prv = token_privilege(token[1], conf->prv);
}

__private void p_mount(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	token_required(3, count, token);
	if( *token[2] == '.' || *token[2] == '/' || *token[2] == '~' ) die("config invalid destination '%s'", token[2]);
	cbc_s* bc = cbc_new();
	bc->fn = vm_mount;
	bc->arg[0].s = (char*)token_systype(token[1]);
	bc->arg[1].s = str_printf("%s/%s", conf->rootdir, token[2]);
	bc->arg[2].s = mem_borrowed(token[1]);
	bc->arg[3].u = token_mountflags(token[3]);
	bc->arg[4].s = mem_borrowed(token[4]);
	bc->arg[5].u = token_privilege(token[5], conf->prv);
	bc->arg[6].u = token_id(token[6], conf->uid);
	bc->arg[7].u = token_id(token[7], conf->gid);
	ld_before(conf->mountpoint, bc);
}

__private void p_bind(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	token_required(3, count, token);
	if( *token[2] == '.' || *token[2] == '/' || *token[2] == '~' ) die("config invalid destination '%s'", token[2]);
	cbc_s* bc = cbc_new();
	bc->fn = vm_mount;
	bc->arg[0].s = path_explode(token[1]);
	bc->arg[1].s = str_printf("%s/%s", conf->rootdir, token[2]);
	bc->arg[2].s = "bind";
	bc->arg[3].u = token_mountflags(token[3]) | MS_BIND;
	bc->arg[4].s = mem_borrowed(token[4]);
	bc->arg[5].u = token_privilege(token[5], conf->prv);
	bc->arg[6].u = token_id(token[6], conf->uid);
	bc->arg[7].u = token_id(token[7], conf->gid);
	ld_before(conf->mountpoint, bc);
}

__private void p_dir(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	token_required(2, count, token);
	if( *token[1] == '.' || *token[1] == '/' || *token[1] == '~' ) die("config invalid destination '%s'", token[1]);
	cbc_s* bc = cbc_new();
	bc->fn = vm_dir;
	bc->arg[0].s = str_printf("%s/%s", conf->rootdir, (strcmp(token[1],"%D") ? token[1] : conf->homedir));
	bc->arg[1].u = token_privilege(token[2], conf->prv);
	bc->arg[2].u = token_id(token[3], conf->uid);
	bc->arg[3].u = token_id(token[4], conf->gid);
	ld_before(conf->mountpoint, bc);
}

__private void p_overlay(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	token_required(3, count, token);
	if( *token[2] == '.' || *token[2] == '/' || *token[2] == '~' ) die("config invalid destination '%s'", token[2]);
	cbc_s* bc = cbc_new();
	bc->fn = vm_overlay;
	bc->arg[0].s = path_explode(token[1]);
	if( !dir_exists(bc->arg[0].s) ) die("overlay.src %s not exists", bc->arg[0].s);
	bc->arg[1].s = mem_borrowed(token[2]);
	bc->arg[2].s = mem_borrowed(conf->destdir);
	bc->arg[3].s = mem_borrowed(conf->rootdir);
	bc->arg[4].u = token_mountflags(token[3]);
	bc->arg[5].s = mem_borrowed(token[4]);
	bc->arg[6].u = token_privilege(token[5], conf->prv);
	bc->arg[7].u = token_id(token[6], conf->uid);
	bc->arg[8].u = token_id(token[7], conf->gid);
	ld_before(conf->mountpoint, bc);
}

__private void p_script(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	token_required(3, count, token);
	cbc_s* bc = cbc_new();
	bc->fn = vm_script;
	__free char* script = token_script(token[2]);
	bc->arg[0].s = str_printf("%s %s %u %u %s", script, conf->destdir, conf->guid, conf->ggid, conf->scrArg ? conf->scrArg : "");
	if( !strcmp(token[1], "mount") ){
		if( conf->scriptMount ) ld_before(conf->scriptMount, bc);
		else conf->scriptMount = bc;
	}
	else if( !strcmp(token[1], "root") ){
		if( conf->scriptRoot ) ld_before(conf->scriptRoot, bc);
		else conf->scriptRoot = bc;
	}
	else if( !strcmp(token[1], "atexit") ){
		if( conf->scriptAtExit ) ld_before(conf->scriptAtExit, bc);
		else conf->scriptAtExit = bc;
	}
	else if( !strcmp(token[1], "fail") ){
		if( conf->scriptOnFail ) ld_before(conf->scriptOnFail, bc);
		else conf->scriptOnFail = bc;
	}
	else{
		die("script: invalid location %s", token[1]);
	}
}

__private void p_syscall(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	token_required(2, count, token);
	unsigned i = 1;
	if( !conf->vm->filter ){
		if( !strcmp(token[1], "allow") ) conf->allowDeny = 0;
		else if( !strcmp(token[1], "deny") ) conf->allowDeny = 1;
		else die("syscall: required allow/deny before use it");
		conf->vm->filter = syscall_ctor();
		i = 2;
	}
	for(; i < count; ++i ){
		if( syscall_add(&conf->vm->filter, token[i], conf->allowDeny) ) die("invalid systemcall %s", token[i]);
		//dbg_info("syscall %c %s", (conf->allowDeny ? '-':'+'), token[i]);
	}
}

__private void p_chdir(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	token_required(2, count, token);
	if( conf->chdir ) die("chdir: can change only one time chdir");	
	cbc_s* bc = cbc_new();
	bc->fn = vm_chdir;
	bc->arg[0].s = path_explode(token[1]);
	//dbg_info("chdir %s", bc->arg[0].s);
	conf->chdir = bc;
}

__private void parse_line(configp_s* conf, unsigned count, char* token[MAX_TOKEN]){
	__private const char* CMDNAME[] = {
		"use",
		"uid",
		"gid",
		"prv",
		"mount",
		"bind",
		"dir",
		"overlay",
		"script",
		"syscall",
		"chdir",
	};
	__private parse_f CMDFN[] = {
		p_use,
		p_uid,
		p_gid,
		p_prv,
		p_mount,
		p_bind,
		p_dir,
		p_overlay,
		p_script,
		p_syscall,
		p_chdir,
	};
	
	for( unsigned i = 0; i < sizeof_vector(CMDNAME); ++i ){
		if( !strcmp(token[0], CMDNAME[i]) ){
			//dbg_info("parse %s", token[0]);
			CMDFN[i](conf, count, token);
			return;
		}
	}
	die("unknown command: %s", token[0]);
}

__private const char* token_end(const char* line){
	while( *line && *line != ' ' && *line != '\t' && *line != '\n' ) ++line;
	return line;
}

__private char* token_dup(const char* st, const char* en){
	if( en == st+1 && *st == '_' ){
		return str_dup("", 0);
	}
	return str_dup(st, en-st);
}

__private int tokenize_line(const char** line, char* token[MAX_TOKEN]){
	unsigned count = 0;
	//dbg_info("");
	while(1){
		*line = str_skip_hn(*line);
		if( **line == '#' ) *line = str_next_line(*line);
		else break;
	}
	
	for(; count < MAX_TOKEN && **line && **line != '\n'; ++count){
		const char* st = *line;
		*line = token_end(*line);
		token[count] = token_dup(st, *line);
		//dbg_info("get token: %s", token[count]);
		*line = str_skip_h(*line);
	}	

	if( **line && **line != '\n' ){
		die("invalid char, propably you have type too many arguments");
	}
	
	return count;
}

__private void build_file(configp_s* conf, const char* lines){
	unsigned count;
	char* token[MAX_TOKEN] = {0};
	while( (count=tokenize_line(&lines, token)) ){
		parse_line(conf, count, token);
		for( unsigned i = 0; i < count; ++i ){
			mem_free(token[i]);
			token[i] = NULL;
		}
	}
}

__private void build_link(configp_s* conf){
	conf->vm->atexit = conf->scriptAtExit;
	conf->vm->onfail = conf->scriptOnFail;
	cbc_s* changeroot = cbc_new();
	changeroot->fn = vm_change_root;
	changeroot->arg[0].s = mem_borrowed(conf->rootdir);

	cbc_s* dropprivilege = cbc_new();
	dropprivilege->fn = vm_privilege_drop;
	dropprivilege->arg[0].u = conf->guid;
	dropprivilege->arg[1].u = conf->ggid;

	cbc_s* exec = cbc_new();
	exec->fn = vm_exec;
	exec->arg[0].as = MANY(char*, conf->execArg->set+2);
	unsigned const nex = conf->execArg->set;
	for( unsigned i = 0; i < nex; ++i ){
		exec->arg[0].as[i] = (char*)conf->execArg->value[i].str;
	}
	exec->arg[0].as[nex] = NULL;
	mem_header(exec->arg[0].as)->len = nex;
	
	conf->vm->stage = conf->mountpoint;
	if( conf->scriptMount ) ld_before(conf->vm->stage, conf->scriptMount);
	ld_before(conf->vm->stage, changeroot);
	if( conf->scriptRoot ) ld_before(conf->vm->stage, conf->scriptRoot);
	if( conf->vm->filter ){
		cbc_s* seccomp = cbc_new();
		seccomp->arg[0].u = conf->allowDeny;
		seccomp->fn = vm_seccomp;
		ld_before(conf->vm->stage, seccomp);
	}
	ld_before(conf->vm->stage, dropprivilege);
	if( conf->chdir ) ld_before(conf->vm->stage, conf->chdir);
	ld_before(conf->vm->stage, exec);
}

configvm_s* config_vm_build(const char* confname, char* destdir, uid_t uid, gid_t gid, const char* scriptArg, option_s* execArg){
	__free char* homedir =  path_home_from_uid(uid);
	configp_s conf = {
		.destdir = destdir,
		.rootdir = str_printf("%s/%s", destdir, HESTIA_ROOT),
		.homedir = &homedir[1],
		.guid    = uid,
		.ggid    = gid,
		.uid     = 0,
		.gid     = 0,
		.prv     = 0,
		.scrArg  = scriptArg,
		.execArg = execArg,
		.vm      = vm_new(),
		.allowDeny = 0,
		.chdir        = NULL,
		.mountpoint   = NULL,
		.scriptAtExit = NULL,
		.scriptMount  = NULL,
		.scriptOnFail = NULL,
		.scriptRoot   = NULL,
	};
	
	conf.mountpoint = cbc_new();
	conf.mountpoint->fn = vm_mount;
	conf.mountpoint->arg[0].s = mem_borrowed(conf.rootdir);
	conf.mountpoint->arg[1].s = mem_borrowed(conf.rootdir);
	conf.mountpoint->arg[2].s = "bind";
	conf.mountpoint->arg[3].u = MS_BIND | MS_PRIVATE | MS_REC;
	conf.mountpoint->arg[4].s = NULL;
	conf.mountpoint->arg[5].u = 0755;
	conf.mountpoint->arg[6].u = 0;
	conf.mountpoint->arg[7].u = 0;
	
	__free char* buf = config_load(confname);
	build_file(&conf, buf);
	build_link(&conf);
	mem_free(conf.rootdir);
	return conf.vm;
}





