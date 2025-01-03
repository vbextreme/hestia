#ifndef __INI_H__
#define __INI_H__

#include <notstd/core.h>

typedef struct iniKV{
	char* k;
	char* v;
}iniKV_s;

typedef struct iniSection{
	char* name;
	iniKV_s* kv;
}iniSection_s;

typedef struct ini{
	iniSection_s* section;
}ini_s;

#define __ini __cleanup(ini_cleanup)

void ini_cleanup(void* pini);
int ini_unpack(ini_s* ini, const char* data);
iniSection_s* ini_section(ini_s* ini, const char* name);
const char* ini_value(iniSection_s* sec, const char* key);

#endif
