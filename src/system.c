#include <notstd/core.h>
#include <notstd/str.h>

#include <hestia/inutility.h>
#include <hestia/system.h>

#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/syscall.h>

#define CODE_BPF_STMT(code, k) ((struct sock_filter){ code, 0, 0, k })
#define CODE_BPF_JUMP(code, k, jt, jf) ((struct sock_filter){ code, jt, jf, k })

#define _LD(VAL)  CODE_BPF_STMT(BPF_LD  | BPF_W   | BPF_ABS,  VAL)
#define _JNE(VAL) CODE_BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, VAL, 0, 1)
#define _JEQ(VAL) CODE_BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, VAL, 1, 0)
#define _RET(VAL) CODE_BPF_STMT(BPF_RET | BPF_K, VAL)

extern char* SYSTEMCALLNAME[];
extern const unsigned SYSTEMCALLNAMECOUNT;

__private long syscall_name_to_nr(const char* name){
	for( unsigned i = 0; i < SYSTEMCALLNAMECOUNT; ++i ){
		if( !SYSTEMCALLNAME[i] ) continue;
		if( !strcmp(SYSTEMCALLNAME[i], name) ) return i;
	}
	return -1;
}

struct sock_filter* syscall_ctor(void){
	struct sock_filter* filter = MANY(struct sock_filter, 16);
	unsigned isys = 0;
	filter[isys++] = _LD(offsetof(struct seccomp_data, arch));
	filter[isys++] = _JNE(offsetof(struct seccomp_data, arch));
	filter[isys++] = _RET(SECCOMP_RET_KILL);
	filter[isys++] = _LD(offsetof(struct seccomp_data, nr));
	mem_header(filter)->len = isys;
	return filter;
}

int syscall_add(struct sock_filter** filter, const char* name, unsigned allowDeny){
	long nr = syscall_name_to_nr(name);
	if( nr == -1 ){
		dbg_error("syscall %s not exists", name);
		return -1;
	}
	*filter = mem_upsize(*filter, 2);
	unsigned isys = mem_header(*filter)->len;
	if( allowDeny ){
		(*filter)[isys++] = _JNE(nr);
		(*filter)[isys++] = _RET(SECCOMP_RET_KILL_PROCESS);
	}
	else{
		(*filter)[isys++] = _JEQ(nr);
		(*filter)[isys++] = _RET(SECCOMP_RET_ALLOW);
	}
	mem_header(*filter)->len = isys;
	return 0;
}

void syscall_end(struct sock_filter** filter, int allowDeny){
	unsigned last = mem_ipush(filter);
	(*filter)[last] = allowDeny ? _RET(SECCOMP_RET_ALLOW) : _RET(SECCOMP_RET_KILL_PROCESS);
}

int syscall_apply(struct sock_filter* filter){
	struct sock_fprog prog = {
		.len    = mem_header(filter)->len,
		.filter = filter,
	};
	if( prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) ){
		dbg_error("prctl(SECCOMP): %m");
		return -1;
	}
	return 0;
}

int systemcall_deny(char** sys, unsigned const count){
	__free struct sock_filter* filter = MANY(struct sock_filter, 4 + count*2 + 1);
	unsigned isys = 0;
	filter[isys++] = _LD(offsetof(struct seccomp_data, arch));
	filter[isys++] = _JNE(offsetof(struct seccomp_data, arch));
	filter[isys++] = _RET(SECCOMP_RET_KILL);
	filter[isys++] = _LD(offsetof(struct seccomp_data, nr));
	for( unsigned i = 0; i < count; ++i ){
		long nr = syscall_name_to_nr(sys[i]);
		if( nr == -1 ){
			dbg_error("unknow systemcall  %s", sys[i]);
			return -1;
		}
		dbg_info("deny %s", sys[i]);
		filter[isys++] = _JNE(nr);
		filter[isys++] = _RET(SECCOMP_RET_KILL_PROCESS);
	}
	filter[isys++] = _RET(SECCOMP_RET_ALLOW);
	
	struct sock_fprog prog = {
		.len    = isys,
		.filter = filter,
	};

	if( prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) ){
		dbg_error("prctl(SECCOMP): %m");
		return -1;
	}
	return 0;
}

//sudo mount -t cgroup2 none /sys/fs/cgroup
//cat /sys/fs/cgroup/cgroup.controllers

//mkdir /sys/fs/cgroup/disable_disks
//echo '+devices' > /sys/fs/cgroup/disable_disks/cgroup.subtree_control
//
//echo "b 8:* rwm" > /sys/fs/cgroup/disable_disks/devices.deny
//echo <PID> > /sys/fs/cgroup/disable_disks/cgroup.procs
//rmdir altrimenti non cancella

char* cgroup_new(const char* name){
	char* group = str_printf("/sys/fs/cgroup/%s", name);
	if( !dir_exists(group) ) mk_dir(group, 755);
	return group;
}

int cgroup_delete(char* group){
	if( !group ) return 0;
	if( !dir_exists(group) ){
		mem_free(group);
		return 0;
	}
	if( rmdir(group) ){
		dbg_error("fail to delete cgroup: %m");
		return -1;
	}
	mem_free(group);
	return 0;
}

int cgroup_rule(const char* group, const char* dest, const char* rule, int add){
	__free char* dev = str_printf("%s/%s", group, dest);
	const char* mode = add ? "a" : "w";
	FILE* f = fopen(dest, mode);
	if( !f ){
		dbg_error("on open file %s:: %m", dest);
		return -1;
	}
	if( fprintf(f, "%s\n", rule) < 0 ){
		dbg_error("write new rule %s: %m", rule);
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

int cgroup_apply(const char* group, unsigned pid){
	char spid[64];
	sprintf(spid, "%u", pid);
	return cgroup_rule(group, "cgroup.procs", spid, 1);
}

__private int pivot_root(const char *new_root, const char *put_old){
    return syscall(SYS_pivot_root, new_root, put_old);
}

int change_root(const char* path){
	__free char* oldroot = str_printf("%s/" HESTIA_MOUNT_OLD_ROOT, path);

	dbg_info("newroot: '%s'", path);
	dbg_info("oldroot: '%s'", oldroot);
	mk_dir(oldroot, 0777);	
	if( mount(oldroot, oldroot, "bind", MS_BIND, NULL) ){
		dbg_error("creating oldroot %s", oldroot);
		return -1;
	}
	if( mount(NULL, oldroot, NULL, MS_PRIVATE, NULL) ){
		dbg_error("set oldroot private %s", oldroot);
		return -1;
	}
	
	if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
		dbg_error("remount / private failed");
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

int privilege_drop(uid_t uid, gid_t gid ){
	dbg_info("drop privilege to: %u@%u", uid, gid);
	gid_t groups[1024];
	int   totalgroups = 0;

	struct passwd* pw = getpwuid(uid);
	if(pw == NULL){
		dbg_error("getpwuid error: %m");
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



