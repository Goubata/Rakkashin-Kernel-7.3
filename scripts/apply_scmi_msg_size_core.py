#!/usr/bin/env python3
"""android16-6.12 SCMI max message size default when the DT does not set one.

6.12 SCMI mailbox/smc transports allowed 128-byte payloads; 7.3 defaults to 104
and only raises it from "arm,max-msg-size". The AVR DT (stock vendor_boot dtb)
has no such property; qcom_scmi_vendor (CPUCP vendor protocol 0x80) requests
128-byte replies and 7.3 rejects them with -ERANGE. Without the property, use
6.12's 128 when the instance's first shmem area can hold it (payload + 24-byte
layout overhead); smaller areas (the PDP perf instance, 0x80 bytes) keep 104.
This replaces ports/device-tree/avr73-fixups.dtso fragment@0 and works with the
unmodified phone DTB.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
p = SRC / 'drivers/firmware/arm_scmi/driver.c'
s = p.read_text()
old = '''	ret = of_property_read_u32(dev->of_node, "arm,max-msg-size",
				   &trans->desc.max_msg_size);
	if (ret && ret != -EINVAL)
		dev_err(dev, "Malformed arm,max-msg-size DT property.\\n");
'''
new = '''	ret = of_property_read_u32(dev->of_node, "arm,max-msg-size",
				   &trans->desc.max_msg_size);
	if (ret && ret != -EINVAL)
		dev_err(dev, "Malformed arm,max-msg-size DT property.\\n");
	/* ANDROID: 6.12 parity: 128-byte payloads when the shmem area fits them */
	if (ret == -EINVAL && trans->desc.max_msg_size < 128) {
		struct device_node *shm = of_parse_phandle(dev->of_node, "shmem", 0);
		struct resource r;

		if (shm && !of_address_to_resource(shm, 0, &r) &&
		    resource_size(&r) >= 128 + 24)
			trans->desc.max_msg_size = 128;
		of_node_put(shm);
	}
'''
if new not in s:
    if s.count(old) != 1:
        raise SystemExit('driver.c anchor')
    s = s.replace(old, new)
    if '#include <linux/of_address.h>' not in s:
        s = s.replace('#include <linux/of.h>\n', '#include <linux/of.h>\n#include <linux/of_address.h>\n', 1)
    p.write_text(s)
print('applied SCMI 6.12 max-msg-size default to', SRC)
