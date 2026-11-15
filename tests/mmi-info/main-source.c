// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include "test-api.h"
#pragma push_macro("module_init")
#pragma push_macro("module_exit")
#undef module_init
#undef module_exit
#define module_init(fn) static typeof(fn) *const __maybe_unused init_reference = fn
#define module_exit(fn) static typeof(fn) *const __maybe_unused exit_reference = fn
#define platform_device_register_simple avr_info_register_device
#include "../../ports/moto-info/mmi_info/mmi_info_main.c"
#pragma pop_macro("module_exit")
#pragma pop_macro("module_init")
static bool test_started;
int avr_info_start(void){int ret=mmi_hw_info_start();if(!ret)test_started=true;return ret;}
void avr_info_stop(void){if(test_started){mmi_hw_info_exit();test_started=false;}}
struct platform_device *avr_info_component(unsigned int index){return index<4?mmi_info_devices[index]:NULL;}
int avr_info_lookup(const char *key,char **value,const char *property,bool lines){return mmi_get_bootarg_dt(key,value,property,lines?"\n":" ");}
