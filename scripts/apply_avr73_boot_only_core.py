#!/usr/bin/env python3
"""Boot-only 7.3: move what tools/ksu-avr73-support did at runtime into the kernel.

With these core changes plus a vendor_boot that autoloads the full 7.3 module set
(GPU/display/camera/EVA, fingerprint, ADIOS, ntsync), the 7.3 kernel works on this
ROM with only boot + vendor_boot modified (no KernelSU module, nothing written to
/system, /vendor or /data). Every change is inert on 6.12 because it lives in the
7.3 kernel only.

1. kernel/module: report -EEXIST for a module whose name is already loaded before the
   struct-module / vermagic checks. vendor_dlkm and /vendor/lib/modules stay 6.12;
   vendor_modprobe.sh, system_dlkm_modprobe.sh and init.oem.fingerprint2.sh try those
   6.12 files. libmodprobe treats EEXIST as "already loaded", so vendor_modprobe.sh
   now sets vendor.all.modules.ready itself (init.qti.kernel.target.rc waits for it in
   post-fs-data) and the fingerprint script keeps the 7.3 driver loaded in the first
   stage. Loading a second copy under an existing name fails either way; only the
   error code changes.
2. fs/read_write: when netbpfload reads /system/etc/init/netbpfload.rc at offset 0,
   present its API line "# 2026 2 37 0 0" as "# 2027 2 38 0 0". The Connectivity
   netd BPF programs for kernel >= 7.3 exist only as 27Q2 variants; with the 26Q2
   line netbpfload loads none and netd1shot fails (reboot,netd1shot-fail). Same
   length, only that process and file, nothing on disk changes.
3. KernelSU-Next built-in rc (appended to init.rc by the kernel): safety net for
   vendor.all.modules.ready, /dev/ntsync 0666 (Rakkashin/ntsync-perm parity), and
   keep OPLUS boot_monitor_service stopped (its repair ladder ends in reboot to
   recovery; it misjudged 7.3 bring-up boots).
"""
from pathlib import Path
import sys

K = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'kernel')


def edit(rel, old, new):
    p = K / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old, new))


edit('kernel/module/main.c', '''	err = rewrite_section_headers(info, flags);
	if (err)
		return err;

	/* Check module struct version now, before we try to use module. */''', '''	err = rewrite_section_headers(info, flags);
	if (err)
		return err;

	/*
	 * ANDROID: avr boot-only 7.3: a module of this name is already live (the 7.3
	 * copy from vendor_boot); answer "exists" before rejecting the 6.12 file that
	 * vendor scripts still try from vendor_dlkm, so libmodprobe counts it as loaded.
	 */
	mutex_lock(&module_mutex);
	if (find_module(info->name)) {
		mutex_unlock(&module_mutex);
		return -EEXIST;
	}
	mutex_unlock(&module_mutex);

	/* Check module struct version now, before we try to use module. */''')

edit('kernel/module/main.c', '''	if (shdr->sh_size != sizeof(struct module)) {
		pr_err("module %s: .gnu.linkonce.this_module section size must match''', '''	if (shdr->sh_size != sizeof(struct module)) {
		bool live;

		/* ANDROID: avr boot-only 7.3: the 6.12 copy of a module already loaded from vendor_boot */
		mutex_lock(&module_mutex);
		live = info->name && find_module(info->name);
		mutex_unlock(&module_mutex);
		if (live)
			return -EEXIST;
		pr_err("module %s: .gnu.linkonce.this_module section size must match''')

edit('fs/read_write.c', '''ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;
''', '''/*
 * ANDROID: avr boot-only 7.3: netd's BPF programs for kernel >= 7.3 are shipped only
 * as 27Q2 variants; this ROM's netbpfload.rc declares 26Q2, so netbpfload loads none
 * and netd1shot fails. Present the API line as 27Q2 to netbpfload only.
 */
static noinline void avr73_netbpfload_api(struct file *file, char __user *buf,
					  ssize_t ret, loff_t start)
{
	static const char from[] = "# 2026 2 37 0 0", to[] = "# 2027 2 38 0 0";
	char head[sizeof(from) - 1], pbuf[64], *p;

	if (start != 0 || ret < (ssize_t)sizeof(head))
		return;
	if (strcmp(current->comm, "netbpfload") ||
	    strcmp(file->f_path.dentry->d_name.name, "netbpfload.rc"))
		return;
	p = d_path(&file->f_path, pbuf, sizeof(pbuf));
	if (IS_ERR(p) || strcmp(p, "/system/etc/init/netbpfload.rc"))
		return;
	if (copy_from_user(head, buf, sizeof(head)) || memcmp(head, from, sizeof(head)))
		return;
	if (!copy_to_user(buf, to, sizeof(head)))
		pr_info_once("avr73: netbpfload.rc API 26Q2 -> 27Q2 (netd BPF variants for 7.x)\\n");
}

ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;
	loff_t start = pos ? *pos : -1;
''')
edit('fs/read_write.c', '''	if (ret > 0) {
		fsnotify_access(file);
		add_rchar(current, ret);
	}
	inc_syscr(current);
	return ret;
}

static ssize_t new_sync_write(''', '''	if (ret > 0) {
		fsnotify_access(file);
		add_rchar(current, ret);
		if (unlikely(start == 0 && current->comm[0] == 'n'))
			avr73_netbpfload_api(file, buf, ret, start);
	}
	inc_syscr(current);
	return ret;
}

static ssize_t new_sync_write(''')

edit('drivers/kernelsu/runtime/ksud_integration.c', '''    "on property:sys.boot_completed=1\\n"
    "    exec u:r:" KERNEL_SU_DOMAIN ":s0 root -- " KSUD_PATH " boot-completed\\n"
    "\\n"
    "\\n";''', '''    "on property:sys.boot_completed=1\\n"
    "    exec u:r:" KERNEL_SU_DOMAIN ":s0 root -- " KSUD_PATH " boot-completed\\n"
    "\\n"
    // ANDROID: avr boot-only 7.3 (this kernel only; 6.12 never sees these lines)
    "on post-fs-data\\n"
    "    setprop vendor.all.modules.ready 1\\n"
    "\\n"
    "on boot\\n"
    "    chmod 0666 /dev/ntsync\\n"
    "\\n"
    "on property:init.svc.boot_monitor_service=running\\n"
    "    stop boot_monitor_service\\n"
    "\\n"
    "\\n";''')
print('applied avr boot-only 7.3 core changes to', K)
