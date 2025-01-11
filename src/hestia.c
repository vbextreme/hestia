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
 *	sandbox need to exists outside sandbox itself
 *
 *	TODO
 *		namespace:
 *		 IPC     wip
 *		 NETWORK need more documentation
 *		 MOUNT   ✓
 *		 PID     ✓
 *		 TIME    wip
 *		 USER    need more documentation
 *		 UTS     need more documentation
 *
 *
*/

option_s OPT[] = {
	{'d', "--destdir"     , "sandbox location"        , OPT_PATH, 0, 0},
	{'c', "--config"      , "use config"              , OPT_STR, 0, 0},
	{'u', "--uid"         , "use user id"             , OPT_NUM, 0, 0},
	{'g', "--gid"         , "use group id"            , OPT_NUM, 0, 0},
	{'P', "--preserve"    , "no remove sandbox at end", OPT_NOARG, 0, 0},
	{'z', "--clean"       , "clean previous sandbox"  , OPT_NOARG, 0, 0},
	{'a', "--analyzer"    , "show important change"   , OPT_NOARG, 0, 0},
	{'A', "--script-arg"  , "set script arguments"    , OPT_STR, 0, 0},
	{'e', "--execute"     , "execute"                 , OPT_SLURP | OPT_STR, 0, 0},
	{'h', "--help"        , "display this"            , OPT_END | OPT_NOARG, 0, 0}
};

void test(uid_t );

int main(int argc, char** argv){
	notstd_begin();
	
	__argv option_s* opt = argv_parse(OPT, argc, argv);
	argv_default_num(opt, O_u, 1000);
	argv_default_num(opt, O_g, 1000);
	if( opt[O_h].set ) argv_usage(opt, argv[0]);

	if( !opt[O_d].set ) die("required destdir");
	__free char* destdir   = path_explode(opt[O_d].value->str);
	
	if( opt[O_z].set ){
		hestia_umount(destdir);
		return 0;
	}

	if( !opt[O_c].set ) die("required config name");
	configvm_s* cvm = config_vm_build("os", destdir, opt[O_u].value->ui, opt[O_g].value->ui, opt[O_A].value->str, &opt[O_e]);
	
	if( opt[O_e].set && hestia_launch(destdir, cvm)) return 1;
	
	if( opt[O_a].set ) hestia_analyze_root(destdir);
	
	if( !opt[O_P].set ) hestia_umount(destdir);
	return 0;
}









