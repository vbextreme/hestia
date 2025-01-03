#ifndef __SOS_LIB_JSON_H__
#define __SOS_LIB_JSON_H__

#include <notstd/core.h>
#include <notstd/rbtree.h>

#define JVALUE_OBJECT_PROPERTY_PARENT "\rPARENT"

typedef enum {
	JV_ERR = -1,
	JV_NULL,
	JV_BOOLEAN,
	JV_NUM,
	JV_FLOAT,
	JV_STRING,
	JV_OBJECT,
	JV_ARRAY
}jvtype_e;

typedef rbtree_s jobject_t;

typedef struct jvalue{
	struct jvalue* parent;
	union{
		void*          p;
		int            b;
		long           n;
		unsigned long  u;
		double         f;
		char*          s;
		jobject_t*     o;
		struct jvalue* a;
	};
	jvtype_e type;
}jvalue_s;

typedef struct jproperty{
	rbtNode_s  node;
	char*      name;
	jvalue_s   value;
}jproperty_s;

void jvalue_dtor(jvalue_s* jv);
jvalue_s* jvalue_property_new(jvalue_s* jv, char* name);
jvalue_s* jvalue_property(jvalue_s* jv, const char* name);
jvalue_s* jvalue_property_type(jvalue_s* jv, jvtype_e type, const char* name);
jvalue_s* jvalue_object_ctor(jvalue_s* jv, jvalue_s* parent);
jvalue_s* jvalue_array_ctor(jvalue_s* jv, jvalue_s* parent);
jvalue_s* jvalue_array_new(jvalue_s* jv);
int jvalue_array_rollback(jvalue_s* jv);
jvalue_s* jvalue_string_ctor(jvalue_s* jv, jvalue_s* parent, char* str);
jvalue_s* jvalue_null_ctor(jvalue_s* jv, jvalue_s* parent);
void jvalue_dump(jvalue_s* jv);

int json_decode_partial(jvalue_s* out, const char** par, const char** err);
jvalue_s* json_decode(const char* str, const char** endstr, const char **outErr);
char* json_encode(jvalue_s* jv, unsigned fprec, unsigned human);

#endif
