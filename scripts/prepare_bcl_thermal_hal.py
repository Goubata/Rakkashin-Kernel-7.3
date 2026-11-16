#!/usr/bin/env python3
"""Prepare a pinned local HAL-name draft; never execute/install the modified HAL."""
from pathlib import Path
import hashlib,json,struct
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'device-baseline/thermal-service.qti'
PIN='dc4b68fa0103ca2cefec856b7eef90977f44d6edbfa7f373d4dfcf0d5db0237f'
OUT=ROOT/'artifacts/bcl-thermal-hal-stage91';OUT.mkdir(exist_ok=True)
if not SOURCE.exists():SOURCE=OUT/'thermal-service.qti.original'
raw=SOURCE.read_bytes();sha=lambda b:hashlib.sha256(b).hexdigest();assert sha(raw)==PIN,'Unreviewed HAL build'
assert raw[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',raw,18)[0]==183
shoff=struct.unpack_from('<Q',raw,40)[0];sz,count,idx=struct.unpack_from('<HHH',raw,58)
headers=[struct.unpack_from('<IIQQQQIIQQ',raw,shoff+i*sz) for i in range(count)]
h=headers[idx];names=raw[h[4]:h[4]+h[5]]
sections={names[h[0]:].split(b'\0',1)[0].decode():h for h in headers}
old=b'pmih010x-2s-ibat-lvl0';new=b'pmih010x-2s-ibat0';assert len(old)==21 and len(new)<20
assert raw.count(old+b'\0')==1 and raw.count(b'pmih010x-2s-ibat-lvl1\0')==0
literal=raw.index(old+b'\0');ro=sections['.rodata'];text=sections['.text'];assert ro[4]<=literal<ro[4]+ro[5]
# Pinned disassembly shows ADRP x9,0x6000; ADD x9,x9,#0x239 at
# 0x1e5cc/0x1e5d0; an inlined libc++ short-string constructor follows.
assert literal==0x6239 and ro[3]+literal-ro[4]==0x6239
pcs=[0x1e610,0x1e614,0x1e618,0x1e61c,0x1e628,0x1e62c,0x1e630]
fileoff=lambda pc:text[4]+pc-text[3]
words=[struct.unpack_from('<I',raw,fileoff(pc))[0] for pc in pcs]
word=words[0];assert word & 0xff80001f==0x52800008 and (word>>5)&0xffff==2*len(old)
new_word=(word & ~(0xffff<<5)) | (2*len(new)<<5)
patched=bytearray(raw);patched[literal:literal+len(old)+1]=new+b'\0'*(len(old)+1-len(new));struct.pack_into('<I',patched,fileoff(pcs[0]),new_word)
allowed=set(range(literal,literal+len(old)+1))|set(range(fileoff(pcs[0]),fileoff(pcs[0])+4))
changed=[i for i,(a,b) in enumerate(zip(raw,patched)) if a!=b];assert set(changed)<=allowed and len(patched)==len(raw)
# Retain address/layout/CFI/relocation data. Build-id stays original; hashes,
# not that unchanged note, identify this modified draft.
output=OUT/'thermal-service.qti.name-draft';output.write_bytes(patched);output.chmod(0o600)
asm='/* Exact selected constructor instructions from pinned HAL. */\n.text\n'
for name,code in [('ctor_original',words),('ctor_candidate',[new_word]+words[1:])]:
 asm+='\n.global '+name+'\n.type '+name+', %function\n'+name+':\n stp x26, x30, [sp, #-16]!\n mov x9, x0\n mov x26, x1\n'
 asm+=''.join(' .inst 0x%08x\n'%w for w in code)
 asm+=' ldp x26, x30, [sp], #16\n ret\n.size '+name+', .-'+name+'\n'
(OUT/'constructor.S').write_text(asm)
r={'passed':True,'scope':'Pinned name/SSO-length draft and selected-instruction harness input only; not full HAL/runtime acceptance','source':str(SOURCE.relative_to(ROOT)),'source_sha256':PIN,'output':str(output.relative_to(ROOT)),'output_sha256':sha(patched),'old_name':old.decode(),'new_name':new.decode(),'literal_file_offset':literal,'length_tag_instruction_pc':pcs[0],'original_length_tag_word':hex(word),'candidate_length_tag_word':hex(new_word),'changed_byte_offsets':changed,'file_size_unchanged':True,'elf_headers_sections_and_relocations_unchanged':True,'constructor_pcs':pcs,'constructor_original_words':[hex(w) for w in words],'constructor_asm_sha256':sha(asm.encode()),'modified_hal_executed':False,'installed':False,'partition_written':False,'promoted':False,'build_id_note':'Original note retained; use new SHA256 to distinguish modified file','limitations':['Selected constructor sequence only; no proof of every indirect reference or full binary policy path','HAL found literal lvl0 only; dynamic/other binaries/SELinux/AVB and firmware variants remain unverified','Actual Android service execution and DT pairing need target 7.3 acceptance; never deploy candidate DT alone']}
(ROOT/'reports/bcl-thermal-hal-stage91-patch.json').write_text(json.dumps(r,indent=2)+'\n');print('Prepared pinned HAL name and inline SSO length draft; no install or HAL execution')
