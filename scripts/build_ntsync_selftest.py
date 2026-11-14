#!/usr/bin/env python3
"""Build an optimized upstream test with explicit compiler fences for kernel locks."""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess
ROOT=Path(__file__).resolve().parents[1]
source=ROOT.parent/'avr-upstream-audit/android-7.3/tools/testing/selftests/drivers/ntsync/ntsync.c'
original=source.read_text()
s=original.replace('static unsigned int stress_counter;',
                    'static unsigned int stress_counter;\nstatic volatile int stress_error;')
a=s.index('static void *stress_thread(');b=s.index('TEST(stress_wait)',a)
thread=s[a:b]
needle='ioctl(stress_device, NTSYNC_IOC_WAIT_ANY, &wait_args);'
assert thread.count(needle)==2
thread=thread.replace(needle,'''ret = ioctl(stress_device, NTSYNC_IOC_WAIT_ANY, &wait_args);
		if (ret) { stress_error = 1; return NULL; }
		/* The mutex lives in the kernel, outside the C abstract machine. */
		asm volatile("" ::: "memory");''')
thread=thread.replace('unlock_mutex(stress_mutex, wait_args.owner, &count);',
 '''asm volatile("" ::: "memory");
		ret = unlock_mutex(stress_mutex, wait_args.owner, &count);
		if (ret) { stress_error = 1; return NULL; }''')
s=s[:a]+thread+s[b:]
s=s.replace('pthread_create(&threads[i], NULL, stress_thread, NULL);',
            'ASSERT_EQ(0, pthread_create(&threads[i], NULL, stress_thread, NULL));')
s=s.replace('EXPECT_EQ(STRESS_LOOPS * STRESS_THREADS, stress_counter);',
 '''asm volatile("" ::: "memory");
	EXPECT_EQ(0, stress_error);
	EXPECT_EQ(STRESS_LOOPS * STRESS_THREADS, stress_counter);''')
generated=ROOT/'tests/rakkashin/ntsync-selftest.generated.c'
generated.write_text(s)
rootfs=ROOT.parent/'.andronix/distros/ubuntu/rootfs'
command=['chroot',str(rootfs),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin',
         'cc','-static','-O2','-pthread','-I/work/avr-port/out/native-gki/usr/include',
         '-I/work/avr-src/tools/testing/selftests',
         '-o','/work/avr-port/ports/rakkashin/ntsync-selftest',
         '/work/avr-port/tests/rakkashin/ntsync-selftest.generated.c']
subprocess.run(['su','-c',shlex.join(command)],check=True)
report={'upstream_source_sha256':hashlib.sha256(original.encode()).hexdigest(),
        'test_source_sha256':hashlib.sha256(s.encode()).hexdigest(),
        'binary_sha256':hashlib.sha256((ROOT/'ports/rakkashin/ntsync-selftest').read_bytes()).hexdigest(),
        'command':command,'changes':['compiler fences after acquiring and before releasing kernel mutex',
                                    'check wait/unlock/thread-creation errors'],
        'unchanged':['all 13 test cases','four stress threads','10000 loops per thread',
                     'non-atomic stress counter','ntsync driver']}
(ROOT/'reports/ntsync-optimized-test-build.json').write_text(json.dumps(report,indent=2)+'\n')
print('Built checked -O2 selftest; preserved non-atomic stress counter')
