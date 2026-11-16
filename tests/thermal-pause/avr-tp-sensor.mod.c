#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xb98e8186, "of_find_node_opts_by_path" },
	{ 0x62d3813f, "platform_device_alloc" },
	{ 0x5e100014, "device_set_node" },
	{ 0x82068328, "platform_device_add" },
	{ 0x4ae224f7, "platform_device_put" },
	{ 0xbb03692e, "devm_thermal_of_zone_register" },
	{ 0x4ae224f7, "platform_device_unregister" },
	{ 0x35f68437, "param_set_int" },
	{ 0x822fadb3, "thermal_zone_device_update" },
	{ 0x4df5efad, "param_get_int" },
	{ 0x0b230c63, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xb98e8186,
	0x62d3813f,
	0x5e100014,
	0x82068328,
	0x4ae224f7,
	0xbb03692e,
	0x4ae224f7,
	0x35f68437,
	0x822fadb3,
	0x4df5efad,
	0x0b230c63,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"of_find_node_opts_by_path\0"
	"platform_device_alloc\0"
	"device_set_node\0"
	"platform_device_add\0"
	"platform_device_put\0"
	"devm_thermal_of_zone_register\0"
	"platform_device_unregister\0"
	"param_set_int\0"
	"thermal_zone_device_update\0"
	"param_get_int\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");

