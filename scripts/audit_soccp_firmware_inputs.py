#!/usr/bin/env python3
"""Cross-check read-only SoCCP volume/file identities and selected native loader preconditions."""
from pathlib import Path
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
a=json.loads((ROOT/'reports/soccp-firmware-stage40-readonly.json').read_text())
b=json.loads((ROOT/'reports/soccp-volume-stage40-readonly.json').read_text())
dt=json.loads((ROOT/'reports/pas-alor-stage39-dt-validation.json').read_text())
regions=dt['inputs']['candidate']['selected']['memory_regions']
files={v['path']:v for v in a['images']};rows=[];errors=[]
for index,name in enumerate(['soccp.mbn','soccp_dtb.mbn']):
 path='/vendor/soccp_firmware/image/'+name
 v=files[path]
 c=regions[index]['reg_cells'];base=(c[0]<<32)|c[1];size=(c[2]<<32)|c[3]
 seg=[p for p in v['program_headers'] if p['type']==1 and p['memsz'] and p['flags']&(7<<24)!=(2<<24)]
 inline=all(p['offset']+p['filesz']<=v['size'] for p in v['program_headers'])
 low=min(p['paddr'] for p in seg);high=max((p['paddr']+p['memsz']+4095)&~4095 for p in seg)
 hashes=[p for p in v['program_headers'][1:] if p['flags']&(7<<24)==2<<24]
 metadata_size=v['program_headers'][0]['filesz']+hashes[0]['filesz'] if hashes else None
 copies=[next(p for p in volume.get('files',[]) if p['path']=='/image/'+name) for volume in b['images']]
 matches=all(p['sha256']==v['content_sha256'] for p in copies)
 header_shape=v['elf_class']==32 and v['program_header_size']==32 and v['program_header_offset']+32*v['program_header_count']<=v['size']
 if v['section_header_size'] or v['section_header_count']:header_shape &= v['section_header_size']==40 and v['section_header_offset']+40*v['section_header_count']<=v['size']
 within=all(p['paddr']>=base and p['paddr']+p['memsz']<=base+size and p['filesz']<=p['memsz'] for p in seg)
 if not all([matches,header_shape,within,inline,bool(hashes)]):errors.append(name+': selected preconditions failed')
 rows.append({'file':path,'sha256':v['content_sha256'],'elf_class':v['elf_class'],'machine':v['machine'],'machine_name':'EM_RISCV' if v['machine']==243 else 'unclassified',
 'matches_both_fat_slots':matches,'loadable_segments':len(seg),'selected_header_shape_valid':header_shape,'all_segments_inline':inline,'fits_saved_reserved_region':within,
 'reserved_region':regions[index]['node'],'reserved_base':base,'reserved_size':size,'predicted_native_get_size':high-low,'predicted_metadata_bytes':metadata_size})
runtime=a['firmware_runtime'];force=runtime.get('/proc/sys/kernel/firmware_config/force_sysfs_fallback')
report={'stage':40,'scope':'independent mounted-file/FAT-slot digest agreement and selected native ELF32 loader/DT preconditions only; no actual native loader execution or secure authentication',
 'captured_utc':a['captured_utc'],'files':rows,'fat_volume_types':[v.get('fat_bits') for v in b['images']],
 'live_soccp':[v for v in a['remoteprocs'] if 'soccp' in v.get('name','')],
 'rom_firmware_directory':'/vendor/soccp_firmware/image/', 'live_force_sysfs_fallback':force,'rom_config_evidence':a['rom_config_evidence'],
 'kernel_user_helper_enabled':'CONFIG_FW_LOADER_USER_HELPER=y' in (ROOT/'out/native-gki/.config').read_text(),
 'dump_elf64_is_separate_from_firmware_elf_class':True,'physical_firmware_restart_tested':False,'native_loader_ram_tested':False,'secure_abi_verified':False,'partition_written':False,
 'evidence_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in ['reports/soccp-firmware-stage40-readonly.json','reports/soccp-volume-stage40-readonly.json','reports/pas-alor-stage39-dt-validation.json','ports/qcom-pas-alor/mdt_loader.c']},
 'errors':errors,'selected_audit_passed':not errors,
 'next':['Run the real native loader against exact saved firmware into guarded RAM, without secure calls',
 'Preserve firmware user-helper fallback and ROM mount/ueventd timing for initial boot and reload',
 'Verify secure PAS/metadata/resource-table and full clock/IRQ/DTB/main-firmware error ownership before device artifact inclusion']}
(ROOT/'reports/soccp-firmware-stage40-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':not errors,'files':rows,'live_force_sysfs_fallback':force},indent=2))
raise SystemExit(bool(errors))
