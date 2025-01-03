#include <notstd/core.h>
#include <notstd/str.h>

#include <hestia/ini.h>

__private int ini_sec_str_cmp(const void* a, const void* b){
	const iniSection_s* secB = b;
	//dbg_info("    A:'%s' B:'%s'", (char*)a, secB->name);
	return strcmp(a, secB->name);
}

__private int ini_sec_sec_cmp(const void* a, const void* b){
	const iniSection_s* secA = a;
	const iniSection_s* secB = b;
	return strcmp(secA->name, secB->name);
}

__private int ini_sec_k_str_cmp(const void* a, const void* b){
	const iniKV_s* kvB = b;
	//dbg_info("    A:'%s' B:'%s'", (char*)a, kvB->k);
	return strcmp(a, kvB->k);
}

__private int ini_sec_k_k_cmp(const void* a, const void* b){
	const iniKV_s* kvA = a;
	const iniKV_s* kvB = b;
	return strcmp(kvA->k, kvB->k);
}

void ini_cleanup(void* pini){
	ini_s* ini = pini;
	mforeach(ini->section, i){
		mem_free(ini->section[i].name);
		mforeach(ini->section[i].kv, j){
			mem_free(ini->section[i].kv[j].k);
			mem_free(ini->section[i].kv[j].v);
		}
		mem_free(ini->section[i].kv);
	}
	mem_free(ini->section);
}

int ini_unpack(ini_s* ini, const char* data){
	dbg_info("");
	ini->section = MANY(iniSection_s, 6);

	while( *data ){
		data = str_skip_hn(data);
		if( *data == '#' ){
			data = str_next_line(data);
			continue;
		}
		if( *data++ != '['){
			errno = EIO;
			dbg_error("not find begin [");
			return -1;
		}
		const char* startsection = data;
		const char* endsection   = strchrnul(data, ']');
		if( *endsection != ']' || endsection - startsection < 1 ){
			errno = EIO;
			dbg_error("not find end ]");
			return -1;
		}
		dbg_info("find section: '%.*s'", (int)(endsection-startsection), startsection);
		
		__free char* sectionName = str_dup(startsection, endsection-startsection);
		iniSection_s* section = mem_bsearch(ini->section, sectionName, ini_sec_str_cmp);
		int sortsection = 0;

		if( !section ){
			dbg_info("section '%s' not exists create new", sectionName);
			ini->section = mem_upsize(ini->section, 1);
			section = &ini->section[mem_header(ini->section)->len++];
			section->name  = mem_borrowed(sectionName);
			section->kv = MANY(iniKV_s, 1);
			sortsection = 1;
		}
		
		data = str_next_line(endsection + 1);
		data = str_skip_hn(data);

		while( *data && *data != '[' ){
			if( *data == '#' ){
				data = str_skip_hn(str_next_line(data));
				continue;
			}
			
			const char* k = data;
			const char* ek = str_anyof(k, " =\n\t");
			const char* v = "true";
			const char* ev = &v[4];

			data = str_skip_h(ek);
			if( *data == '=' ){
				data = str_skip_h(data+1);
				char el = '\n';
				if( *data == '\'' || *data == '"' ) el = *data++;
				v = data++;
				while( *data && *data != el ) ++data;
				ev = data;
				if( el == '\n' ){
					while( ev > v && ev[-1] == ' ' ) --ev;
				}
			}

			//dbg_info("K='%.*s' V='%.*s'", (int)(ek-k), k, (int)(ev-v), v);
			__free char* kname = str_dup(k, ek-k);
	
			iniKV_s* kv = mem_bsearch(section->kv, kname, ini_sec_k_str_cmp);
			
			if( !kv ){
				dbg_info("%s->kv[%s] not exists, create new", section->name, kname);
				section->kv = mem_upsize(section->kv, 1);

				kv = &section->kv[mem_header(section->kv)->len++];
				kv->k = mem_borrowed(kname);
				kv->v = str_dup(v, ev-v);
				mem_qsort(section->kv, ini_sec_k_k_cmp);
			}
			else{
				dbg_info("%s->kv[%s] exists, replace", section->name, kname);
				mem_free(kv->v);
				kv->v = str_dup(v, ev-v);
			}

			data = str_skip_hn(data);
		}
		
		if( sortsection ){
			dbg_info("sort section");
			mem_qsort(ini->section, ini_sec_sec_cmp);
		}
	}
	
	return 0;
}

iniSection_s* ini_section(ini_s* ini, const char* name){
	return mem_bsearch(ini->section, (char*)name, ini_sec_str_cmp);
}

const char* ini_value(iniSection_s* sec, const char* key){
	if( !sec ) return NULL;
	iniKV_s* kv = mem_bsearch(sec->kv, (char*)key, ini_sec_k_str_cmp);
	return kv ? kv->v : NULL;
}
