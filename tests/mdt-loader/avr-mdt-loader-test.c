// SPDX-License-Identifier: GPL-2.0-only
/* Real exported native loader, actual saved firmware and guarded RAM only. */
#include <kunit/test.h>
#include <linux/device.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/vmalloc.h>
static void check_image(struct kunit *test, const char *name, phys_addr_t base,
			size_t reserved, ssize_t predicted)
{
	const struct firmware *fw = NULL;
	struct device *dev = root_device_register("avr_mdt_ram_test");
	const struct elf32_hdr *eh;
	const struct elf32_phdr *ph;
	u8 *guarded = NULL, *expected = NULL;
	void *metadata;
	size_t metadata_size = 0, hash = 0;
	phys_addr_t reloc = 0;
	int ret, i;
	KUNIT_ASSERT_FALSE(test, IS_ERR(dev));
	ret = request_firmware(&fw, name, dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	if (ret) goto out;
	eh = (const void *)fw->data;
	ph = (const void *)(fw->data + eh->e_phoff);
	KUNIT_EXPECT_EQ(test, eh->e_ident[EI_CLASS], (u8)ELFCLASS32);
	KUNIT_EXPECT_EQ(test, eh->e_machine, (u16)EM_RISCV);
	KUNIT_EXPECT_EQ(test, qcom_mdt_get_size(fw), predicted);
	for (i=1;i<eh->e_phnum;i++)
		if ((ph[i].p_flags & QCOM_MDT_TYPE_MASK)==QCOM_MDT_TYPE_HASH) {hash=i;break;}
	if (!hash) {KUNIT_FAIL(test,"expected hash segment");goto out;}
	metadata=qcom_mdt_read_metadata(fw,&metadata_size,name,dev);
	KUNIT_EXPECT_FALSE(test,IS_ERR(metadata));
	if (IS_ERR(metadata)) goto out;
	KUNIT_EXPECT_EQ(test,metadata_size,(size_t)ph[0].p_filesz+ph[hash].p_filesz);
	KUNIT_EXPECT_MEMEQ(test,metadata,fw->data,ph[0].p_filesz);
	KUNIT_EXPECT_MEMEQ(test,(u8 *)metadata+ph[0].p_filesz,fw->data+ph[hash].p_offset,ph[hash].p_filesz);
	kfree(metadata);
	guarded=vmalloc(reserved+64); expected=vmalloc(reserved);
	if (!guarded || !expected) {KUNIT_FAIL(test,"RAM allocation");goto out;}
	memset(guarded,0xa5,reserved+64);memset(expected,0xa5,reserved);
	for(i=0;i<eh->e_phnum;i++) {
		size_t offset;
		if(ph[i].p_type!=PT_LOAD || !ph[i].p_memsz || (ph[i].p_flags&QCOM_MDT_TYPE_MASK)==QCOM_MDT_TYPE_HASH)continue;
		offset=(u64)ph[i].p_paddr-base;
		if(offset>reserved || ph[i].p_memsz>reserved-offset || ph[i].p_filesz>ph[i].p_memsz || (u64)ph[i].p_offset+ph[i].p_filesz>fw->size) {
			KUNIT_FAIL(test,"captured input outside reviewed region");goto out;
		}
		memcpy(expected+offset,fw->data+ph[i].p_offset,ph[i].p_filesz);
		memset(expected+offset+ph[i].p_filesz,0,ph[i].p_memsz-ph[i].p_filesz);
	}
	ret=qcom_mdt_load_no_init(dev,fw,name,guarded+32,base,reserved,&reloc);
	KUNIT_EXPECT_EQ(test,ret,0);
	KUNIT_EXPECT_EQ(test,reloc,base);
	KUNIT_EXPECT_MEMEQ(test,guarded+32,expected,reserved);
	KUNIT_EXPECT_PTR_EQ(test,memchr_inv(guarded,0xa5,32),NULL);
	KUNIT_EXPECT_PTR_EQ(test,memchr_inv(guarded+reserved+32,0xa5,32),NULL);
out:
	vfree(expected);vfree(guarded);
	if(fw) release_firmware(fw);
	root_device_unregister(dev);
}
static void actual_soccp_main(struct kunit *test) {check_image(test,"avr-test/soccp.mbn",0xa4400000,6*1024*1024,3596288);}
static void actual_soccp_dtb(struct kunit *test) {check_image(test,"avr-test/soccp_dtb.mbn",0xa4a00000,256*1024,86016);}
static void short_headers_rejected(struct kunit *test)
{
	u8 raw[52]={};struct firmware fw={.data=raw,.size=sizeof(raw)};size_t count;
	KUNIT_EXPECT_EQ(test,qcom_mdt_get_size(&fw),(ssize_t)-EINVAL);
	KUNIT_EXPECT_PTR_EQ(test,qcom_mdt_read_metadata(&fw,&count,"unused",NULL),ERR_PTR(-EINVAL));
	memcpy(raw,ELFMAG,SELFMAG);fw.size=4;
	KUNIT_EXPECT_EQ(test,qcom_mdt_get_size(&fw),(ssize_t)-EINVAL);
}
static void invalid_destination_preserves_ram(struct kunit *test)
{
	const struct firmware *fw=NULL;u8 bytes[96];int ret;
	struct device *dev=root_device_register("avr_mdt_bounds_test");
	KUNIT_ASSERT_FALSE(test,IS_ERR(dev));
	ret=request_firmware(&fw,"avr-test/soccp.mbn",dev);
	KUNIT_EXPECT_EQ(test,ret,0);
	if(!ret) {
		memset(bytes,0xa5,sizeof(bytes));
		KUNIT_EXPECT_EQ(test,qcom_mdt_load_no_init(dev,fw,"unused",bytes+32,0xa4400000,1,NULL),-EINVAL);
		KUNIT_EXPECT_PTR_EQ(test,memchr_inv(bytes,0xa5,sizeof(bytes)),NULL);
		KUNIT_EXPECT_EQ(test,qcom_mdt_load_no_init(dev,fw,"unused",bytes+32,0xa4400001,32,NULL),-EINVAL);
		KUNIT_EXPECT_PTR_EQ(test,memchr_inv(bytes,0xa5,sizeof(bytes)),NULL);
		release_firmware(fw);
	}
	root_device_unregister(dev);
}
static void metadata_must_fit_advertised_firmware(struct kunit *test)
{
	/* Padded backing memory makes the old over-read safe to reproduce. */
	u8 raw[256] __aligned(8) = {};
	struct elf32_hdr *eh=(void *)raw;
	struct elf32_phdr *ph=(void *)(raw+sizeof(*eh));
	struct firmware fw={.data=raw,.size=128};
	void *metadata;size_t size=0;
	memcpy(eh->e_ident,ELFMAG,SELFMAG);
	eh->e_ident[EI_CLASS]=ELFCLASS32;eh->e_ident[EI_DATA]=ELFDATA2LSB;
	eh->e_phoff=sizeof(*eh);eh->e_phentsize=sizeof(*ph);eh->e_phnum=2;
	ph[0].p_filesz=129;
	ph[1].p_flags=QCOM_MDT_TYPE_HASH;ph[1].p_offset=120;ph[1].p_filesz=4;
	metadata=qcom_mdt_read_metadata(&fw,&size,"unused",NULL);
	KUNIT_EXPECT_PTR_EQ(test,metadata,ERR_PTR(-EINVAL));
	if(!IS_ERR(metadata)) kfree(metadata);
	KUNIT_EXPECT_EQ(test,size,(size_t)0);
	ph[0].p_filesz=sizeof(*eh);
	metadata=qcom_mdt_read_metadata(&fw,&size,"unused",NULL);
	KUNIT_EXPECT_PTR_EQ(test,metadata,ERR_PTR(-EINVAL));
	if(!IS_ERR(metadata)) kfree(metadata);
}
static void null_and_wrong_class_rejected(struct kunit *test)
{
	u8 raw[128] __aligned(8)={};
	struct elf32_hdr *eh=(void *)raw;
	struct elf32_phdr *ph=(void *)(raw+sizeof(*eh));
	struct firmware fw={.data=raw,.size=sizeof(raw)};
	size_t size=0;void *metadata;
	KUNIT_EXPECT_EQ(test,qcom_mdt_get_size(NULL),(ssize_t)-EINVAL);
	KUNIT_EXPECT_PTR_EQ(test,qcom_mdt_read_metadata(NULL,&size,"unused",NULL),ERR_PTR(-EINVAL));
	memcpy(eh->e_ident,ELFMAG,SELFMAG);
	eh->e_ident[EI_CLASS]=ELFCLASS64;eh->e_ident[EI_DATA]=ELFDATA2LSB;
	eh->e_phoff=sizeof(*eh);eh->e_phentsize=sizeof(*ph);eh->e_phnum=2;
	ph[0].p_filesz=116;ph[1].p_flags=QCOM_MDT_TYPE_HASH;ph[1].p_offset=116;ph[1].p_filesz=4;
	metadata=qcom_mdt_read_metadata(&fw,&size,"unused",NULL);
	KUNIT_EXPECT_PTR_EQ(test,metadata,ERR_PTR(-EINVAL));
	if(!IS_ERR(metadata)) kfree(metadata);
	eh->e_ident[EI_CLASS]=ELFCLASS32;
	KUNIT_EXPECT_PTR_EQ(test,qcom_mdt_read_metadata(&fw,NULL,"unused",NULL),ERR_PTR(-EINVAL));
}
static void physical_end_addition_is_wide(struct kunit *test)
{
	u8 raw[128] __aligned(8)={};
	struct elf32_hdr *eh=(void *)raw;
	struct elf32_phdr *ph=(void *)(raw+sizeof(*eh));
	struct firmware fw={.data=raw,.size=sizeof(raw)};
	memcpy(eh->e_ident,ELFMAG,SELFMAG);
	eh->e_ident[EI_CLASS]=ELFCLASS32;eh->e_ident[EI_DATA]=ELFDATA2LSB;
	eh->e_phoff=sizeof(*eh);eh->e_phentsize=sizeof(*ph);eh->e_phnum=1;
	ph[0].p_type=PT_LOAD;ph[0].p_paddr=0xfffff000U;ph[0].p_memsz=0x2000;
	KUNIT_EXPECT_EQ(test,qcom_mdt_get_size(&fw),(ssize_t)8192);
}
static struct kunit_case cases[]={KUNIT_CASE(actual_soccp_main),KUNIT_CASE(actual_soccp_dtb),KUNIT_CASE(short_headers_rejected),KUNIT_CASE(invalid_destination_preserves_ram),KUNIT_CASE(metadata_must_fit_advertised_firmware),KUNIT_CASE(null_and_wrong_class_rejected),KUNIT_CASE(physical_end_addition_is_wide),{}};
static struct kunit_suite suite={.name="avr-mdt-loader-real-inputs",.test_cases=cases};
kunit_test_suite(suite);
MODULE_DESCRIPTION("AVR actual SoCCP firmware native RAM loader tests");
MODULE_LICENSE("GPL");
