#include <notstd/core.h>

#include <unistd.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#define CODE_BPF_STMT(code, k) ((struct sock_filter){ code, 0, 0, k })
#define CODE_BPF_JUMP(code, k, jt, jf) ((struct sock_filter){ code, jt, jf, k })

#define _LD(VAL)  CODE_BPF_STMT(BPF_LD  | BPF_W   | BPF_ABS,  VAL);
#define _JNE(VAL) CODE_BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, VAL, 0, 1)
#define _RET(VAL) CODE_BPF_STMT(BPF_RET | BPF_K, VAL);

extern char* SYSTEMCALLNAME[];
extern const unsigned SYSTEMCALLNAMECOUNT;

__private long syscall_name_to_nr(const char* name){
	for( unsigned i = 0; i < SYSTEMCALLNAMECOUNT; ++i ){
		if( !SYSTEMCALLNAME[i] ) continue;
		if( !strcmp(SYSTEMCALLNAME[i], name) ) return i;
	}
	return -1;
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

