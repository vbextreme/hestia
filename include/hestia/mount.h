#ifndef __HESTIA_MOUNT_H__
#define __HESTIA_MOUNT_H__

#include <sys/stat.h>

#include <hestia/config.h>

#define HESTIA_MOUNT_OLD_ROOT "old_root"

int hestia_mount(const char* destdir, rootHierarchy_s* root);
int hestia_umount(const char* destdir);






















#endif
