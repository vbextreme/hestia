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
#include <hestia/config.h>

#include <hestia/inutility.h>
#include <hestia/mount.h>
#include <hestia/system.h>

typedef struct overwriteArgs{
	configvm_s*  vm;
	const char*  destdir;
}overwriteArgs_s;

__private int overwrite(void* parg){
	overwriteArgs_s* arg = parg;
	__free char* mountpointRoot = str_printf("%s/" HESTIA_ROOT, arg->destdir);
	if( dir_exists(mountpointRoot) ) hestia_umount(arg->destdir);
	config_vm_run(arg->vm);
	dbg_error("exec fail: %m");
	return 1;
}

int hestia_launch(const char* destdir, configvm_s* vm){
	overwriteArgs_s arg = {
		.destdir = destdir,
		.vm = vm
	};
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
	config_vm_atexit(vm, 0);
	munmap(newStack, SUBSTACKSIZE);
	return 0;
ONERR:
	config_vm_atexit(vm, -1);
	hestia_umount(destdir);
	munmap(newStack, SUBSTACKSIZE);
	return -1;
}

