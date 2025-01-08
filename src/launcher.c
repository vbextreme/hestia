#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sched.h>
#include <pwd.h>
#include <grp.h>

#include <notstd/core.h>
#include <notstd/str.h>
#include <notstd/opt.h>

#include <hestia/launcher.h>
#include <hestia/inutility.h>
#include <hestia/mount.h>

typedef struct overwriteArgs{
	rootHierarchy_s* root;
	uid_t        uid;
	gid_t        gid;
	const char*  path;
	option_s*    osa;
	char**       argv;
}overwriteArgs_s;

__private int pivot_root(const char *new_root, const char *put_old){
    return syscall(SYS_pivot_root, new_root, put_old);
}

__private int change_root(const char* path){
	__free char* oldroot = str_printf("%s/" HESTIA_MOUNT_OLD_ROOT, path);
	mk_dir(oldroot, 0777);
	
	dbg_info("newroot: '%s'", path);
	dbg_info("oldroot: '%s'", oldroot);

	if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
		dbg_error("mount --make-private failed");
		return -1;
	}

	if( pivot_root(path, oldroot) ){
		dbg_error("pivot root fail: %m");
		return -1;
	}

	chdir("/");
	if( umount2("/" HESTIA_MOUNT_OLD_ROOT, MNT_DETACH) != 0){
        dbg_error("fail to umount oldroot: %m");
        return -1;
    }
	rm("/" HESTIA_MOUNT_OLD_ROOT);
	return 0;
}

__private int privilege_drop(uid_t uid, gid_t gid ){
	dbg_info("drop privilege to: %u@%u", uid, gid);
	gid_t groups[1024];
	int   totalgroups = 0;

	struct passwd* pw = getpwuid(uid);
	if(pw == NULL){
		dbg_error("getpwuid error: m");
		return -1;
	}
	getgrouplist(pw->pw_name, pw->pw_gid, NULL, &totalgroups);
	dbg_info("getgrouplist count: %u", totalgroups);
		
	if( totalgroups > 1024 ){
		dbg_error("to many groups");
		return -1;
	}
	if( getgrouplist(pw->pw_name, pw->pw_gid, groups, &totalgroups) == -1 ){
		dbg_error("getdrouplist error");
		return -1;
	}
	
	if( setgroups(totalgroups,groups) < 0 ){
		dbg_error("setgroups: %m");
		return -1;
	}
	if( setgid(gid) ){
		dbg_error("fail to set gid: %m");
		return -1;
	}
	if( setuid(uid) ){
		dbg_error("fail to set uid");
		return -1;
	}
	
	return 0;
}

__private rootHierarchy_s* command_find(rootHierarchy_s* root, const char* cmd){
	mforeach(root->child, i){
		if( !strcmp(root->child[i].target, cmd) ) return &root->child[i];
	}
	return NULL;
}

__private int command_script_run(overwriteArgs_s* arg, const char* cmd){
	rootHierarchy_s* script = command_find(arg->root, cmd);
	iassert(script);

	mforeach(script->child, i){
		const char* sa = i < arg->osa->set ? arg->osa->value[i].str : "";
		__free char* cmd = str_printf("%s %s %u %u %s", script->child[i].src, arg->path, arg->uid, arg->gid, sa);
		if( shell(cmd) ) return -1;
	}
	return 0;
}

__private int overwrite(void* parg){
	overwriteArgs_s* arg = parg;
	__free char* mountpointRoot = str_printf("%s/root", arg->path);
	__free char* oldroot = str_printf("%s/" HESTIA_MOUNT_OLD_ROOT, mountpointRoot);

	if( !dir_exists(mountpointRoot) ){
		mk_dir(oldroot, 0777);	
		if( mount(oldroot, oldroot, "bind", MS_BIND | MS_PRIVATE, NULL) ){
			dbg_error("creating oldroot");
			return -1;
		}
		if( hestia_mount(arg->path, arg->root) ){
			dbg_error("fail mount %s/root", arg->path);
			return 1;
		}
		if( command_script_run(arg, HESTIA_SCRIPT_ENT) ) return 1;

		if( change_root(mountpointRoot) ) return 1;	
		if( privilege_drop(arg->uid, arg->gid) ) return 1;
		
		rootHierarchy_s* chr = command_find(arg->root, HESTIA_CHDIR_ENT);
		iassert(chr);
		if( chr->src ){
			if( chdir(chr->src) ){
				dbg_error("chdir %s fail: %m", chr->src);
				return -1;
			}
		}
	}
	dbg_info("good bye %u@%u", getuid(), getgid());
	execv(arg->argv[0], arg->argv);
	dbg_error("exec %s: %m", arg->argv[0]);
	return 1;
}

int hestia_launch(const char* destdir, uid_t uid, gid_t gid, rootHierarchy_s* root, option_s* oex, option_s* osa){
	dbg_info("launch %s/root", destdir);

	overwriteArgs_s arg = {
		.path   = destdir,
		.osa    = osa,
		.root   = root,
		.gid    = gid,
		.uid    = uid,
		.argv   = MANY(char*, oex->set+2)
	};

	dbg_info("set: %u", oex->set);
	unsigned const nex = oex->set;
	for( unsigned i = 0; i < nex; ++i ){
		arg.argv[i] = (char*)oex->value[i].str;
		dbg_info("argv[%u]::%s", i, arg.argv[i]);
	}
	arg.argv[nex] = NULL;
	mem_header(arg.argv)->len = nex;

	void* newStack = mmap(NULL, SUBSTACKSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
	pid_t pid = clone(overwrite, newStack + SUBSTACKSIZE, CLONE_NEWNS | CLONE_NEWPID | SIGCHLD, &arg);
	if( pid == -1 ){
		dbg_error("clone fail: %m");
		goto ONERR;
	}
	
	int status;
	if( waitpid(pid, &status, 0) == -1 ){
		dbg_error("waitpid fail: %m");
		goto ONERR;
	}
	if( WEXITSTATUS(status) ){
		dbg_error("execd app return error");
		goto ONERR;
	}

	if( command_script_run(&arg, HESTIA_ATEXIT_ENT) ) goto ONERR;

	munmap(newStack, SUBSTACKSIZE);
	mem_free(arg.argv);
	return 0;
ONERR:
	hestia_umount(destdir, root);
	munmap(newStack, SUBSTACKSIZE);
	mem_free(arg.argv);
	return -1;
}

