#include <notstd/core.h>
#include <notstd/str.h>
#include <notstd/opt.h>

#include <hestia/hestia.h>
#include <hestia/inutility.h>
#include <hestia/mount.h>
#include <hestia/launcher.h>
#include <hestia/config.h>
#include <hestia/analyzer.h>

/*
 *	TODO
 *	testare mount
 *	testare umount
 *
 *	rifare launcher
 *
*/

option_s OPT[] = {
	{'d', "--destdir"     , "sandbox location"        , OPT_PATH, 0, 0},
	{'t', "--target"      , "sandbox name"            , OPT_STR, 0, 0},
	{'c', "--config"      , "use config"              , OPT_STR, 0, 0},
	{'u', "--uid"         , "use user id"             , OPT_NUM, 0, 0},
	{'g', "--gid"         , "use group id"            , OPT_NUM, 0, 0},
	{'P', "--preserve"    , "no remove sandbox at end", OPT_NOARG, 0, 0},
	{'z', "--clean"       , "clean previous sandbox"  , OPT_NOARG, 0, 0},
	{'a', "--analyzer"    , "show important change"   , OPT_NOARG, 0, 0},
	{'A', "--script-arg"  , "set script arguments"    , OPT_ARRAY | OPT_STR  , 0, 0},
	{'\0', ""             , "execute"                 , OPT_SLURP, 0, 0},
	{'h', "--help"        , "display this"            , OPT_END | OPT_NOARG, 0, 0}
};

void test(uid_t );

int main(int argc, char** argv){
	notstd_begin();
	
	__argv option_s* opt = argv_parse(OPT, argc, argv);
	if( opt[O_h].set ) argv_usage(opt, argv[0]);

	argv_default_num(opt, O_u, 1000);
	argv_default_num(opt, O_g, 1000);

	if( !opt[O_d].set ) die("required destdir");
	if( !opt[O_t].set ) die("required package name");
	if( !opt[O_c].set ) die("required config name");

	__free char* destdir   = path_explode(opt[O_d].value->str);

	rootHierarchy_s* root = hestia_load(opt[O_c].value->str, opt[O_u].value->ui, opt[O_g].value->ui);

	hestia_config_analyzer(destdir, opt[O_t].value->str, root);

	if( opt[O_z].set ){
		hestia_umount(destdir, opt[O_t].value->str, root);
		return 0;
	}

	if( hestia_launch(destdir, opt[O_t].value->str, opt[O_u].value->ui, opt[O_g].value->ui, root, &opt[O_exec], &opt[O_A]) ) return 1;
	
	if( opt[O_a].set ) hestia_analyze_root(destdir, opt[O_t].value->str, root);

	if( opt[O_P].set ) return 0;

	hestia_umount(destdir, opt[O_t].value->str, root);

	
	return 0;
}





















