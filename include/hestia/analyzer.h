#ifndef __ANALYZER_H__
#define __ANALYZER_H__

#include <hestia/config.h>

#define HESTIA_ANALYZER "@analyzer@"

typedef struct analEnt{
	unsigned type;
	char*    path;
}analEnt_s;

const char* dtname(unsigned dt);
analEnt_s* hestia_analyze_list(const char* destdir);
void hestia_analyze_root(const char* destdir);

#endif
