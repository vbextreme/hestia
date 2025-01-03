#ifndef __LAUNCHER_H__
#define __LAUNCHER_H__

#define SUBSTACKSIZE (1024*1024*4)

#include <unistd.h>
#include <notstd/opt.h>
#include <hestia/config.h>

int hestia_launch(const char* destdir, const char* target, uid_t uid, gid_t gid, rootHierarchy_s* root, option_s* oex);

#endif
