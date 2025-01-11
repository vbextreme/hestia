#ifndef __LAUNCHER_H__
#define __LAUNCHER_H__

#define SUBSTACKSIZE (1024*1024*8)

#include <hestia/config.h>

int hestia_launch(const char* destdir, configvm_s* vm);

#endif
