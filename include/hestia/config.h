#ifndef __HESTIA_CONFIG_H__
#define __HESTIA_CONFIG_H__

#include <sys/stat.h>
#include <notstd/list.h>
#include <notstd/opt.h>

/*
 * s MS_NOSUID
 * x MS_NOEXEC
 * d MS_NODEV
 * r MS_RDONLY
 * t MS_STRICTATIME
 * p MS_PRIVATE
 *
 * use configname
 * uid num/%u
 * gid num/%g
 * prv 0755/rwxrwxrwx
 *
 * mount type, dest, ?option, ?mode, ?prv, ?uid, ?gid
 * bind   src, dest, ?option, ?mode, ?prv, ?uid, ?gid
 * [s0] src [s1] destdir/dest [s2] type [u3] flags [u4] mode [u5] prv [u6] uid [u7] uid
 *
 * overlay src, dest, ?option, ?mode, ?prv, ?uid, ?gid
 * [s0] src [s1] dest [s2] destdir [u3] flags [u4] mode [u5] prv [u6] uid [u7] uid
 *
 * dir dest/%D(homedir)
 * [s0] path [u1] prv [u2] uid [u3] gid
 *
 * script mout/root/atexit/fail, scriptname
 * [s0] cmd
 *
 * systemcall allow/deny/syscallname
 * [s0] allowDeny
 *
 * chdir pathinsidesandbox, ?prv
 * [s0] dir [u1] prv [u2] uid [u3] gid
 *
 *
 * snapshot snapname
 * [s0] destdir [s1] snapname
 * output /destdir/snapname.snapshot only before change root
*/


#define HESTIA_ROOT        "root"
#define HESTIA_CONFIG_PATH "/etc/hestia/config.d"
#define HESTIA_SCRIPT_PATH "/etc/hestia/script.d"
#define HESTIA_CMD_CHR     '@'
#define HESTIA_SCRIPT_ENT  "@SCRIPT@"
#define HESTIA_ATEXIT_ENT  "@ATEXIT@"
#define HESTIA_CHDIR_ENT   "@CHDIR@"
#define HESTIA_SYSCALL_ENT "@SYSCALL@"
#define HESTIA_CGROUP_ENT  "@CGROUP@"

#define MAX_TOKEN 32

typedef struct configvm configvm_s;
typedef struct cbc cbc_s;

typedef int(*eval_f)(configvm_s* vm);

struct cbc{
	inherit_ld(struct cbc);
	eval_f      fn;
	union{
		char*         s;
		char**        as;
		unsigned long u;
	}arg[16];
};

struct configvm{
	cbc_s*  stage;
	cbc_s*  atexit;
	cbc_s*  onfail;
	cbc_s*  current;
	struct sock_filter* filter;
	unsigned flags;
};

int config_vm_run(configvm_s* vm);
int config_vm_atexit(configvm_s* vm, int ret);
configvm_s* config_vm_build(const char* confname, char* destdir, uid_t uid, gid_t gid, const char* scriptArg, option_s* execArg);

#endif
