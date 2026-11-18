#!/usr/bin/env python3
"""android16-6.12 SELinux kernel_read_file / kernel_load_data parity.

Phone test 14: under 7.3 every firmware read was denied:
  avc: denied { firmware_load } scontext=u:r:kernel tcontext=u:object_r:firmware_file
  tclass=system  (ipa_fws.mdt, ...)
7.3 checks system:{firmware_load,kexec_image_load,kexec_initramfs_load,policy_load,
x509_certificate_load}; 6.12 checked only module_load and allowed everything else.
The phone's Android 16 policy (built for 6.12) defines but never grants the new
permissions, so IPA/ADSP/CDSP/modem/GPU/WLAN/camera/video firmware all fail.
Keep 6.12 semantics: only module loads are checked.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
p = SRC / 'security/selinux/hooks.c'
s = p.read_text()
M = '/* ANDROID: 6.12 parity: only module loads are checked; the other system:*_load\n\t * permissions are not granted by Android 16 policies written for 6.12 */\n'


def sub(old, new):
    global s
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'anchor count {s.count(old)}: {old[:50]!r}')
    s = s.replace(old, new)


sub('''	switch (id) {
	case READING_FIRMWARE:
		rc = selinux_kernel_load_from_file(file, SYSTEM__FIRMWARE_LOAD);
		break;
	case READING_MODULE:
	case READING_MODULE_COMPRESSED:
		rc = selinux_kernel_load_from_file(file, SYSTEM__MODULE_LOAD);
		break;
	case READING_KEXEC_IMAGE:
		rc = selinux_kernel_load_from_file(file,
						   SYSTEM__KEXEC_IMAGE_LOAD);
		break;
	case READING_KEXEC_INITRAMFS:
		rc = selinux_kernel_load_from_file(file,
						SYSTEM__KEXEC_INITRAMFS_LOAD);
		break;
	case READING_POLICY:
		rc = selinux_kernel_load_from_file(file, SYSTEM__POLICY_LOAD);
		break;
	case READING_X509_CERTIFICATE:
		rc = selinux_kernel_load_from_file(file,
						SYSTEM__X509_CERTIFICATE_LOAD);
		break;
	default:
		break;
	}''', '''	''' + M + '''	switch (id) {
	case READING_MODULE:
	case READING_MODULE_COMPRESSED:
		rc = selinux_kernel_load_from_file(file, SYSTEM__MODULE_LOAD);
		break;
	default:
		break;
	}''')
sub('''	switch (id) {
	case LOADING_FIRMWARE:
		rc = selinux_kernel_load_from_file(NULL, SYSTEM__FIRMWARE_LOAD);
		break;
	case LOADING_MODULE:
		rc = selinux_kernel_load_from_file(NULL, SYSTEM__MODULE_LOAD);
		break;
	case LOADING_KEXEC_IMAGE:
		rc = selinux_kernel_load_from_file(NULL,
						   SYSTEM__KEXEC_IMAGE_LOAD);
		break;
	case LOADING_KEXEC_INITRAMFS:
		rc = selinux_kernel_load_from_file(NULL,
						SYSTEM__KEXEC_INITRAMFS_LOAD);
		break;
	case LOADING_POLICY:
		rc = selinux_kernel_load_from_file(NULL,
						   SYSTEM__POLICY_LOAD);
		break;
	case LOADING_X509_CERTIFICATE:
		rc = selinux_kernel_load_from_file(NULL,
						SYSTEM__X509_CERTIFICATE_LOAD);
		break;
	default:
		break;
	}''', '''	''' + M + '''	switch (id) {
	case LOADING_MODULE:
		rc = selinux_kernel_load_from_file(NULL, SYSTEM__MODULE_LOAD);
		break;
	default:
		break;
	}''')
p.write_text(s)
print('applied SELinux load-check parity to', SRC)
