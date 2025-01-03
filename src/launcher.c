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
	const char*  target;
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

__private int overwrite(void* parg){
	overwriteArgs_s* arg = parg;
	__free char* mountpointRoot = str_printf("%s/%s", arg->path, arg->target);

	__free char* oldroot = str_printf("%s/" HESTIA_MOUNT_OLD_ROOT, mountpointRoot);
	mk_dir(oldroot, 0777);	
	if( mount(oldroot, oldroot, "bind", MS_BIND | MS_PRIVATE, NULL) ){
		dbg_error("creating oldroot");
		return -1;
	}
	if( hestia_mount(arg->path, arg->target, arg->root) ){
		dbg_error("fail mount %s/%s", arg->path, arg->target);
		return 1;
	}
	
	if( change_root(mountpointRoot) ) return 1;
	
	if( privilege_drop(arg->uid, arg->gid) ) return 1;
	
	dbg_info("good bye %u@%u", getuid(), getgid());
	execv(arg->argv[0], arg->argv);
	dbg_error("exec %s: %m", arg->argv[0]);
	return 1;
}

int hestia_launch(const char* destdir, const char* target, uid_t uid, gid_t gid, rootHierarchy_s* root, option_s* oex){
	dbg_info("launch %s/%s", destdir, target);

	overwriteArgs_s arg = {
		.path    = destdir,
		.target  = target,
		.root    = root,
		.gid     = gid,
		.uid     = uid,
		.argv    = MANY(char*, oex->set+2)
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
	munmap(newStack, SUBSTACKSIZE);
	mem_free(arg.argv);
	return 0;
ONERR:
	hestia_umount(destdir, target, root);
	munmap(newStack, SUBSTACKSIZE);
	mem_free(arg.argv);
	return -1;
}

/*
int hestia_build_script(const char* buildpath, const char* pkgname, option_s* makedeps, uid_t uid, gid_t gid){
	__free char* buildscript = str_printf("%s/buildscript", buildpath);
	FILE* f = fopen(buildscript, "w");
	if( !f ){
		dbg_error("on open %s: %m", buildscript);
		return -1;
	}
	
	fputs("#!/bin/bash\n", f);
	fputs("cd /build\n", f);
	if( makedeps->set ){
		fputs("echo 'install make dependency, need root password for call pacman'\n", f);
		fputs("sudo pacman --needed --asdeps -S", f);
		mforeach(makedeps->value, i){
			fprintf(f, " %s", makedeps->value[i].str);
		}
		fputc('\n', f);
	}
	fputs("echo 'download'\n", f);
	fprintf(f, "git clone https://aur.archlinux.org/%s.git\n", pkgname);
	fprintf(f, "cp %s/PKGBUILD ./PKGBUILD\n", pkgname);
	fputs("echo 'create package'\n", f);
	fputs("makepkg\n", f);
	
	fclose(f);
	chown(buildscript, uid, gid);
	chmod(buildscript, 0770);

	return 0;
}

int hestia_install_pkg(const char* path){
	__free const char* command = str_printf("makepkg -D %s --install", path);
	int ret = system(command);
	if( ret == -1 ) return -1;
	if( !WIFEXITED(ret) || WEXITSTATUS(ret) != 0) return -1;
	return 0;
}
*/

