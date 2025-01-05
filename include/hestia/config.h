#ifndef __HESTIA_CONFIG_H__
#define __HESTIA_CONFIG_H__

#include <sys/stat.h>

#define HESTIA_CONFIG_PATH "/etc/hestia/config.d"
#define HESTIA_SCRIPT_PATH "/etc/hestia/script.d"
#define HESTIA_SCRIPT_ENT  "@SCRIPT@"

typedef struct rootHierarchy{
	struct rootHierarchy* child;
	char*    target;
	char*    type;
	char*    src;
	char*    mode;
	uid_t    uid;
	gid_t    gid;
	unsigned privilege;
	unsigned flags;
}rootHierarchy_s;

int hestia_is_systemfs(const char* opt);
rootHierarchy_s* hestia_load(const char* confname, uid_t uid, gid_t gid);

void hestia_config_analyzer(const char* destdir, const char* target, rootHierarchy_s* sb);

#endif
