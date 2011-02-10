/*
 * This file is part of stargate.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 */

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysmem_kernel.h>
#include <pspthreadman_kernel.h>
#include <pspdebug.h>
#include <pspinit.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "systemctrl.h"
#include "systemctrl_private.h"
#include "kubridge.h"
#include "utils.h"
#include "printk.h"
#include "strsafe.h"
#include "libs.h"
#include "stargate.h"

PSP_MODULE_INFO("stargate", 0x1007, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

static STMOD_HANDLER previous;

#define MAX_MODULE_NUMBER 256

static void fix_weak_imports(void)
{
	SceUID *modids;
	int ret;
	int i, count;
	int k1;

	k1 = pspSdkGetK1();
	pspSdkSetK1(0);
	modids = oe_malloc(MAX_MODULE_NUMBER * sizeof(SceUID));

	if(modids == NULL) {
		printk("%s: allocating modids failed\n", __func__);
		goto exit;
	}

	memset(modids, 0, MAX_MODULE_NUMBER * sizeof(SceUID));
	ret = sceKernelGetModuleIdList(modids, MAX_MODULE_NUMBER * sizeof(SceUID), &count);
	
	if (ret < 0) {
		printk("%s: sceKernelGetThreadmanIdList -> 0x%08x\n", __func__, ret);
		oe_free(modids);
		goto exit;
	}

	for(i=0; i<count; ++i) {
		SceModule2 *pMod;

		pMod = (SceModule2*)sceKernelFindModuleByUID(modids[i]);

		if (pMod != NULL && (pMod->attribute & 0x1000) == 0) {
			patch_drm_imports((SceModule*)pMod);
		}
	}

	oe_free(modids);
exit:
	pspSdkSetK1(k1);
}

static int stargate_module_chain(SceModule2 *mod)
{
	if (previous)
		(*previous)(mod);

	// for MHP3rd: a 6.36 game
	hook_import_bynid((SceModule*)mod, "scePauth", 0x98B83B5D, myPauth_98B83B5D, 1);
	patch_drm_imports((SceModule*)mod);
	patch_utility((SceModule*)mod);
	patch_load_module((SceModule*)mod);

	// m33 mode: until npdrm loaded
	if(0 == strcmp(mod->modname, "scePspNpDrm_Driver")) {
		int ret;

		fix_weak_imports();
		ret = nodrm_get_npdrm_functions();

#ifdef DEBUG
		if (ret < 0) {
			printk("%s: nodrm_get_npdrm_functions -> %d\n", __func__, ret);
		}
#endif
	}
	
	return 0;
}

int module_start(SceSize args, void *argp)
{
	printk_init("ms0:/log_stargate.txt");
	printk("stargate started\n");
	patch_sceMesgLed();
	myPauth_init();
	load_module_get_function();
	nodrm_init();
	nodrm_get_normal_functions();
	nodrm_get_npdrm_functions(); // np9660 mode: npdrm already loaded
	
	previous = sctrlHENSetStartModuleHandler(&stargate_module_chain);

	return 0;
}
