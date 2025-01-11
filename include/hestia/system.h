#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <unistd.h>
#include <linux/filter.h>

#define HESTIA_MOUNT_OLD_ROOT "old_root"

struct sock_filter* syscall_ctor(void);
int syscall_add(struct sock_filter** filter, const char* name, unsigned allowDeny);
void syscall_end(struct sock_filter** filter, int allowDeny);
int syscall_apply(struct sock_filter* filter);

int systemcall_deny(char** sys, unsigned const count);
char* cgroup_new(const char* name);
int cgroup_delete(char* group);
int cgroup_rule(const char* group, const char* dest, const char* rule, int add);
int cgroup_apply(const char* group, unsigned pid);

int change_root(const char* path);
int privilege_drop(uid_t uid, gid_t gid );




#endif
