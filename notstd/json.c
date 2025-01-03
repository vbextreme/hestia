#include <notstd/memory.h>
#include <notstd/rbtree.h>
#include <notstd/json.h>
#include <notstd/str.h>
#include <notstd/str.h>
#include <notstd/utf8.h>

__private jvalue_s JVERR = { .type = JV_ERR, .parent = NULL, .p = NULL };

__private int jpcmp(const void* a, const void* b){
	const jproperty_s* jpa = a;
	const jproperty_s* jpb = b;
	return strcmp(jpa->name, jpb->name);
}

__private void jvalue_property_clean(void* pnode){
	rbtNode_s* node = pnode;
	jproperty_s* jp = node->data;
	jvalue_dtor(&jp->value);
	mem_free(jp->name);
	mem_free(jp);
}

void jvalue_dtor(jvalue_s* jv){
	switch( jv->type ){
		case JV_ARRAY: 
			mforeach(jv->a, i){
				jvalue_dtor(&jv->a[i]);
			}
			mem_free(jv->a);
		break;

		case JV_OBJECT:
			rbtree_dtor_cbk(jv->o, jvalue_property_clean);
			mem_free(jv->o);
		break;
		
		case JV_STRING: mem_free(jv->s); break;

		default: break;
	}
}

jvalue_s* jvalue_property_new(jvalue_s* jv, char* name){
	if( jv->type != JV_OBJECT ) return &JVERR;
	jproperty_s* property = NEW(jproperty_s);
	property->name         = name;
	property->value.type   = JV_NULL;
	property->value.parent = jv;
	property->value.p      = NULL;
	rbtree_insert(jv->o, rbtNode_ctor(&property->node, property));
	return &property->value;
}

jvalue_s* jvalue_property(jvalue_s* jv, const char* name){
	if( jv->type != JV_OBJECT ) return &JVERR;
	jproperty_s pfind = { .name = (char*)name };
	jproperty_s* jp = rbtree_search(jv->o, &pfind);
	if( !jp ) return &JVERR;
	return &jp->value;
}

jvalue_s* jvalue_property_type(jvalue_s* jv, jvtype_e type, const char* name){
	if( jv->type != JV_OBJECT ) return &JVERR;
	jproperty_s pfind = { .name = (char*)name };
	jproperty_s* jp = rbtree_search(jv->o, &pfind);
	if( !jp ) return &JVERR;
	if( jp->value.type != type ) return &JVERR;
	return &jp->value;
}

jvalue_s* jvalue_object_ctor(jvalue_s* jv, jvalue_s* parent){
   	jv->type     = JV_OBJECT,
	jv->parent   = parent,
	jv->o        = rbtree_ctor(NEW(jobject_t), jpcmp);
	return jv;
}

jvalue_s* jvalue_array_ctor(jvalue_s* jv, jvalue_s* parent){
	jv->parent = parent,
	jv->type   = JV_ARRAY,
	jv->a      = MANY(jvalue_s, 2);
	return jv;
}

jvalue_s* jvalue_array_new(jvalue_s* jv){
	if( jv->type != JV_ARRAY ) return &JVERR;
	jv->a = mem_upsize(jv->a, 1);
	jvalue_s* ret = &jv->a[mem_header(jv->a)->len++];
	ret->type   = JV_NULL;
	ret->p      = (void*)(~(uintptr_t)0);
	ret->parent = jv;
	return ret;
}

int jvalue_array_rollback(jvalue_s* jv){
	if( jv->type != JV_ARRAY ){
		return -1;
	}
	unsigned len = mem_header(jv->a)->len;
	if( len <= 0 ){
		return -1;
	}
	jvalue_s* v = &jv->a[--len];
	if( v->type != JV_NULL ){
		return -1;
	}
	mem_header(jv->a)->len = len;
	return 0;
}

jvalue_s* jvalue_string_ctor(jvalue_s* jv, jvalue_s* parent, char* str){
	jv->type   = JV_STRING;
	jv->parent = parent;
	jv->s      = str;
	return jv;
}

jvalue_s* jvalue_null_ctor(jvalue_s* jv, jvalue_s* parent){
	jv->parent = parent;
	jv->type   = JV_NULL;
	jv->p      = NULL;
	return jv;
}

/*
jvalue_s jvalue_num(jvalue_s* parent, long num){
	jvalue_s jv = {
		.parent = parent,
		.type   = JV_NUM,
		.n      = num
	};
	return jv;
}

jvalue_s jvalue_float(jvalue_s* parent, double num){
	jvalue_s jv = {
		.parent = parent,
		.type   = JV_FLOAT,
		.f      = num
	};
	return jv;
}

jvalue_s jvalue_string(char* str){
	jvalue_s jv = {
		.type = JV_STRING,
		.s    = str
	};
	return jv;
}

jvalue_s jvalue_null(void){
	jvalue_s jv = {
		.type = JV_NULL,
		.p    = NULL
	};
	return jv;
}

jvalue_s jvalue_bool(int val){
	jvalue_s jv = {
		.type = JV_BOOLEAN,
		.b    = val
	};
	return jv;
}
*/
__private const char* json_parse_to_element(const char* parse){
	while( *parse == ' ' || *parse == '\t' || *parse == '\n' || *parse == '\r' ) ++parse;
	return parse;
}

__private int json_parse_validate_num(const char** outparse, const char** err){
	const char* parse = *outparse;
	int ret = 0;
	
	if( parse[0] == '0' && parse[1] >= '0' && parse[1] <= '9' ){
		*err = "leading zero in number";
		return -1;
	}
	
	if( *parse == '-' ){
		++parse;
		if( parse[0] == '0' && parse[1] >= '0' && parse[1] <= '9' ){
			*outparse = &parse[1];
			*err = "negative double zero";
			return -1;
		}
	}
	
	while( *parse >= '0' && *parse <= '9' ) ++parse;
	if( *parse == '.' || *parse == 'e'  || *parse == 'E' ){
		ret = 1;
		++parse;
		if( !(*parse >= '0' && *parse <= '9') && *parse != '+' && *parse != '-' ){
			*outparse = parse;
			*err = "invalid floating value, aspected [0-9+-]";
			return -1;
		}
		while( ((*parse >= '0' && *parse <= '9') || *parse == '+' || *parse == '-' || *parse == 'e') ) ++parse;
	}
	
	*outparse = parse;
	return ret;
}

__private int json_parse_num(const char** outparse, const char** err, jvalue_s* value){
	const char* stnum = *outparse;
	char* end = NULL;
	errno = 0;
	
	switch( json_parse_validate_num(outparse, err) ){
		case -1: return -1;
		case 0 :
			value->type = JV_NUM;
			value->n = strtol(stnum, &end, 10);
		break;
		case 1 :
			value->type = JV_FLOAT;
			value->f = strtod(stnum, &end);
		break;
	}
	
	if( errno ){
		*err = strerror(errno);
		*outparse = stnum;
		return -1;
	}
	if( end != *outparse ){
		*err = "invalid numerical char";
		*outparse = end;
		return -1;
	}
	return 0;
}

__private ucs4_t json_escape_unicode(const char** outparse, const char** err){
	char hex[5];
	const char* parse = *outparse;
	for( unsigned i = 0; i < 4; ++i ){
		if( !*parse ) goto ONERR;
		hex[i] = *parse++;
	}
	hex[4] = 0;
	char* end;
	ucs4_t ret = strtoul(hex, &end, 16);
	if( end != &hex[4] ) goto ONERR;
	*outparse = parse;
	return ret;
ONERR:
	*err = "invalid unicode, aspected \\uXXXX";
	*outparse = parse;
	return 0;
}

__private long json_string_len(const char* parse){
	const char* start = parse;
	while( *parse && *parse != '"' ){
		if( *parse == '\\' ) ++parse;
		++parse;
	}
	if( !*parse ) return -1;
	return parse - start;
}

__private char* json_parse_string(const char** outparse, const char** err){
	const char* parse = *outparse;
	const char* start = parse;
	long        jslen;
	
	if( *parse != '"' ){
		*err = "string not begin with \"";
		return NULL;
	}
	++parse;
	jslen = json_string_len(parse);
	if( jslen < 0 ){
		*err = "string not terminated with \"";
		return NULL;
	}
	
	char* str = MANY(char, jslen+1);
	unsigned len = 0;
	ucs4_t u4;
	
	//dbg_info("%.*s",(unsigned)jslen, parse);
	
	while( *parse != '"' ){
		if( *parse == '\n' || *parse == '\r' || *parse == '\t' || *parse == '\b' || *parse == '\f' ){
			*err = "newline, tab, etc need escape in string";
			*outparse = parse;
			mem_free(str);
			return NULL;
		}
		if( *parse == '\\' ){
			++parse;
			switch( *parse ){
				default  : *outparse = parse; *err = "invalid escape"; mem_free(str); return NULL;
				case '\\': str[len++] = '\\'; break;
				case '/' : str[len++] = '/'; break;
				case '"' : str[len++] = '"'; break;
				case 'b' : str[len++] = '\b'; break;
				case 'f' : str[len++] = '\f'; break;
				case 'n' : str[len++] = '\n'; break;
				case 'r' : str[len++] = '\r'; break;
				case 't' : str[len++] = '\t'; break;
				case 'u' :
					++parse;
					u4 = json_escape_unicode(&parse, err);
					if( !u4 && *err ) return NULL;
					
					if( (u4 >= 0x20 && u4 <= 0x21) ||
						(u4 >= 0x23 && u4 <= 0x5B) ||
						(u4 >= 0x5D && u4 <= 0x10FFFF )
					){
						if( u4 >= 0xD800 && u4 <= 0xDFFF ){
							if( !(parse[0] == '\\' && parse[1] == 'u') ){
								mem_free(str);
								*err = "invalid unicode, aspeted surrugate";
								*outparse = parse-4;
								return NULL;
							}
							parse += 2;
							ucs4_t surrugate = json_escape_unicode(&parse, err);
							if( !(surrugate >= 0xDC00 && surrugate <= 0xDFFF) ){
								mem_free(str);
								*err = "invalid unicode surrugate";
								*outparse = parse-4;
								return NULL;
							}
							u4 = ((u4 & 0x3ff) << 10) + (surrugate & 0x3ff) + 0x10000;
							--parse;
						}
						len += ucs4_to_utf8(u4, (utf8_t*)&str[len]);
						--parse;
					}
					else{
						mem_free(str);
						*err = "invalid unicode";
						*outparse = parse;
						return NULL;
					}
				break;
			}
			++parse;
		}
		else{
			if( (long)len >= jslen ){
				dbg_info("%.*s", (unsigned)jslen+1,start);
				die("len>=strlen (%ld)", jslen);
			}
			str[len++] = *parse++;
		}
	}
	if( *parse != '"' ){
		mem_free(str);
		*outparse = start;
		*err = "unterminated string";
		return NULL;
	}
	*outparse = parse+1;
	str[len] = 0;
	mem_header(str)->len = len;
	return str;
}

__private const char* checkup_term(const char** par, const char** err){
	const char* parse = *par;
	parse = json_parse_to_element(parse);
	if( *parse && *parse != ']' && *parse != '}' && *parse != ',' ){
		*err = "invalid char at this state";
		return parse;
	}
	*par = parse;
	return NULL;
}

int json_decode_partial(jvalue_s* out, const char** par, const char** err){
	const char* parse = *par;
	char* str;
	int type;
	jvalue_s* jv = out;
	parse = json_parse_to_element(parse);

	while( *parse ){
		if( !jv ){
			*err = "aspected end of json";
			*par = parse;
			return -1;
		}
		
		switch( *parse ){
			default:
				*err = "invalid charater, aspected element(num, string, array or object)"; 
				*par = parse;
			return -1;
			
			case '{':
				jv = jvalue_object_ctor(jv, jv->parent);
				parse = json_parse_to_element(parse+1);
			break;
			
			case '}':
				if( jv->type != JV_OBJECT ){
					*err = "object not openend or missing element after ,";
					*par = parse;
					return -1;
				}
				jv = jv->parent;
				parse = json_parse_to_element(parse+1);
			break;
			
			case '[':
				jv = jvalue_array_ctor(jv, jv->parent);
				jv = jvalue_array_new(jv);
				parse = json_parse_to_element(parse+1);
			break;
			
			case ']':
				if( jv->type != JV_ARRAY ){
					if( !jv->parent || jv->parent->type != JV_ARRAY || jv->p != (void*)(~(uintptr_t)0) || jvalue_array_rollback(jv->parent) ){
						*err = "array not openend or missing element after ,";
						*par = parse;
						return -1;
					}
					jv = jv->parent;
				}
				jv = jv->parent;
				parse = json_parse_to_element(parse+1);
			break;
			
			case ',':
				switch( jv->type ){
					case JV_OBJECT:
						if( jv->o->count <= 0 ){
							*err = "object next without previus element";
							*par = parse;
							return -1;
						}
						parse = json_parse_to_element(parse+1);
						if( *parse == '}' ){
							*err = "close object after next";
							*par = parse;
							return -1;
						}
					break;
				
					case JV_ARRAY:
						if( mem_header(jv->a)->len <= 0 ){
							*err = "array next without previus element";
							*par = parse;
							return -1;
						}
						parse = json_parse_to_element(parse+1);
						if( *parse == ']' ){
							*err = "close array after next";
							*par = parse;
							return -1;
						}
						jv = jvalue_array_new(jv);
					break;
					
					default: 
						*err = "unaspected next";
						*par = parse;
					return -1;
				}
			break;
			
			case '"':
				if( !(str=json_parse_string(&parse, err)) ){
					*par = parse;
					return -1;
				}
				switch( jv->type ){
					case JV_NULL  :
						jvalue_string_ctor(jv, jv->parent, str);
						jv = jv->parent;
						if( (*par=checkup_term(&parse, err)) ) return -1;
					break;
					
					case JV_OBJECT:{
						parse = json_parse_to_element(parse);
						if( *parse++ != ':' ){
							*err = "aspected assign(:)";
							*par = --parse;
							mem_free(str);
							return -1;
						}
						jvalue_s* jp = jvalue_property(jv, str);
						if( jp->type == JV_ERR ){
							jp = jvalue_property_new(jv, str);
						}
						else{
							jvalue_dtor(jp);
							jp->type   = JV_NULL;
							jp->p      = NULL;
							jp->parent = jv;
							mem_free(str);
						}
						jv = jp;
						parse = json_parse_to_element(parse);
					}
					break;
					
					default:
						*err = "unaspected string at this state";
						*par = parse;
					return -1;
				}
			break;
			
			case '-':
			case '0'...'9':
				if( jv->type != JV_NULL ){
					*err = "unaspected number at this state";
					*par = parse;
					return -1;
				}
				if( (type=json_parse_num(&parse, err, jv)) < 0 ){
					*par = parse;
					return -1;
				}
				jv = jv->parent;
				if( (*par=checkup_term(&parse, err)) ) return -1;
			break;

			case 'f':
				if( jv->type != JV_NULL ){
					*err = "unaspected literal at this state";
					*par = parse;
					return -1;
				}
				if( strncmp(parse, "false", 5) ){
					*err = "invalid constant, accept only false/true/null";
					*par = parse;
					return -1;
				}
				parse += 5;
				jv->type = JV_BOOLEAN;
				jv->b    = 0;
				jv = jv->parent;
				if( (*par=checkup_term(&parse, err)) ) return -1;
			break;

			case 't':
				if( jv->type != JV_NULL ){
					*err = "unaspected literal at this state";
					*par = parse;
					return -1;
				}
				if( strncmp(parse, "true", 4) ){
					*err = "invalid constant, accept only false/true/null";
					*par = parse;
					return -1;
				}
				parse += 4;
				jv->type = JV_BOOLEAN;
				jv->b    = 1;
				jv = jv->parent;
				if( (*par=checkup_term(&parse, err)) ) return -1;
			break;

			case 'n':
				if( jv->type != JV_NULL ){
					*err = "unaspected literal at this state";
					*par = parse;
					return -1;
				}
				if( strncmp(parse, "null", 4) ){
					*err = "invalid constant, accept only false/true/null";
					return -1;
				}
				parse += 4;
				jv = jv->parent;
				if( (*par=checkup_term(&parse, err)) ) return -1;	
			break;
		}
	}
	*par = parse;
	return jv? 1: 0;
}

jvalue_s* json_decode(const char* str, const char** endstr, const char **outErr){
	const char* err = NULL;
	jvalue_s* jv = NEW(jvalue_s);
	mem_header(jv)->cleanup = (mcleanup_f)jvalue_dtor;
	jvalue_null_ctor(jv, NULL);
	int ret = json_decode_partial(jv, &str, &err);
	if( !ret ){
		dbg_info("decode successfull");	
		return jv;
	}
	if( ret > 0 ) err = "incomplete json";
	if( endstr ) *endstr = str;
	if( outErr ) *outErr = err;
	dbg_error("%s", err);
	dbg_info("%s", str);
	mem_free(jv);
	return NULL;
}

void jvalue_dump(jvalue_s* jv){
	switch( jv->type ){
		default: die("internal error, report this issue, %d but not supported", jv->type); break;
		
		case JV_ERR:
			printf("err");
		break;
		
		case JV_NULL: 
			printf("null");	
		break;
		
		case JV_BOOLEAN:
			printf("%s", jv->b ? "true": "false");
		break;
		
		case JV_NUM:
			printf("%ld", jv->n);
		break;

		case JV_FLOAT:
			str_printf("%f", jv->f);
		break;
		
		case JV_STRING:
			printf("'%s'", jv->s);
		break;

		case JV_ARRAY:
			printf("[");
			mforeach(jv->a, i){
				jvalue_dump(&jv->a[i]);
				printf(",");
			}
			printf("]");
		break;

		case JV_OBJECT:{
			rbtreeit_s it;
			rbtreeit_ctor(&it, jv->o, 0);
			jproperty_s* jp;
			printf("\n{\n");
			while( (jp=rbtree_iterate_inorder(&it)) ){
				printf("%s:", jp->name);
				jvalue_dump(&jp->value);
				printf(",\n");
			}
			printf("}\n");
			rbtreeit_dtor(&it);
		}
		break;
	}

}



















typedef struct jenc{
	char*    str;
	unsigned fprec;
	unsigned tab;
}jenc_s;

__private void encode(jvalue_s* jv, jenc_s* je, int hum);

__private void entab(jenc_s* je, unsigned extrasize){
	unsigned n = je->tab;
	je->str = mem_upsize(je->str, n+extrasize);
	unsigned len = mem_header(je->str)->len;
	while( n--> 0 ){
		je->str[len++] = '\t';
	}
	mem_header(je->str)->len = len;
}

__private void jestr(jenc_s* je, const char* str){
	unsigned l = strlen(str);
	unsigned len = mem_header(je->str)->len;
	je->str = mem_upsize(je->str, l*2+3);
	je->str[len++] = '"';
	while( *str ){
		switch( *str ){
			case '\\': je->str[len++] = '\\'; je->str[len++] = '\\'; break;
			case '"' : je->str[len++] = '\\'; je->str[len++] = '"'; break;
			case '\b': je->str[len++] = '\\'; je->str[len++] = 'b'; break;
			case '\f': je->str[len++] = '\\'; je->str[len++] = 'f'; break;
			case '\n': je->str[len++] = '\\'; je->str[len++] = 'n'; break;
			case '\r': je->str[len++] = '\\'; je->str[len++] = 'r'; break;
			case '\t': je->str[len++] = '\\'; je->str[len++] = 't'; break;
			default  : je->str[len++] = *str++;                     break;
		}
	}
	je->str[len++] = '"';
	mem_header(je->str)->len = len;
}

__private void jpenc(jproperty_s* pro, jenc_s* je, int hum){
	if( hum ) entab(je, 0);
	jestr(je, pro->name);
	je->str = mem_upsize(je->str, 1+hum);
	je->str[mem_header(je->str)->len++] = ':';
	if( hum ) je->str[mem_header(je->str)->len++] = ' ';
	encode(&pro->value, je, hum);
	je->str = mem_upsize(je->str, 1+hum);
	je->str[mem_header(je->str)->len++] = ',';
	if( hum ) je->str[mem_header(je->str)->len++] = '\n';
}

__private void encode(jvalue_s* jv, jenc_s* je, int hum){
	switch( jv->type ){
		case JV_ERR: die("internal error"); break;

		case JV_NUM:
			je->str = mem_upsize(je->str, 64);
			mem_header(je->str)->len += sprintf(&je->str[mem_header(je->str)->len], "%ld", jv->n);
		break;

		case JV_FLOAT:
			je->str = mem_upsize(je->str, 64 + je->fprec);
			mem_header(je->str)->len += sprintf(&je->str[mem_header(je->str)->len], "%.*f", je->fprec, jv->f);
		break;

		case JV_BOOLEAN:
			je->str = mem_upsize(je->str, 6);
			mem_header(je->str)->len += sprintf(&je->str[mem_header(je->str)->len], "%s", jv->b ? "true": "false");
		break;

		case JV_NULL:
			je->str = mem_upsize(je->str, 5);
			mem_header(je->str)->len += sprintf(&je->str[mem_header(je->str)->len], "%s", "null");
		break;
		
		case JV_STRING:
			jestr(je, jv->s);
		break;

		case JV_ARRAY:{
			je->str = mem_upsize(je->str, 1);
			je->str[mem_header(je->str)->len++] = '[';
			mforeach(jv->a, it){
				encode(&jv->a[it], je, hum);
				je->str = mem_upsize(je->str, 1+hum);
				je->str[mem_header(je->str)->len++] = ',';
				if( hum ) je->str[mem_header(je->str)->len++] = ' ';
			}
			if( mem_header(jv->a)->len ) --mem_header(je->str)->len;
			je->str = mem_upsize(je->str, 1);
			je->str[mem_header(je->str)->len++] = ']';
		}break;

		case JV_OBJECT:{
			je->str = mem_upsize(je->str, 1+hum);
			je->str[mem_header(je->str)->len++] = '{';
			if( hum ) je->str[mem_header(je->str)->len++] = '\n';
			++je->tab;
			rbtreeit_s it;
			rbtreeit_ctor(&it, jv->o, 0);
			jproperty_s* ojp;
			while( (ojp=rbtree_iterate_inorder(&it)) ){
				jpenc(ojp, je, hum);
			}
			--mem_header(je->str)->len;
			if( hum ) je->str[mem_header(je->str)->len] = '\n';
			--je->tab;
			if( hum ) entab(je, 1);
			je->str = mem_upsize(je->str, 1);
			je->str[mem_header(je->str)->len++] = '}';
			rbtreeit_dtor(&it);
		}
		break;
	}
}

char* json_encode(jvalue_s* jv, unsigned fprec, unsigned human){
	jenc_s je = {
		.fprec = fprec ? fprec : 6,
		.str   = MANY(char, 128),
		.tab   = 0
	};
	
	encode(jv, &je, human);
	je.str = mem_nullterm(je.str);
	je.str = mem_fit(je.str);
	return je.str;
}

