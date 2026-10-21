"""Retain closed original particle callbacks and bounded material initializer in IDA."""
import hashlib,json
from prepare_menu_preview import ROOT,ida
RECORDS=[
(0x155F40,0x307,'original_particle_random_color_single',
 'int __fastcall {name}(void *initializer, void *preserved_edx, void *collection, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4);',
 'Complete307-byte native RET14 wrapper, ECX=C_INIT_RandomColor initializer, EDX preserved, collection plus four opaque stack DWORDs. Saves/restores initializer RGB vectors+5C/+68 around saved original slot7FB8F8. Traverses collection parents+88; definition+48/name+134; count+30. Root12 fire hashes excluding seven child smoke hashes use5390C8 enable/RGB539328; two smoke root hashes use mode537420>0/RGB5376A8. Only calls signed-FNV6CF70 and saved original. Current client RTTI C_INIT_RandomColor vtable D09110 slot29=9597A0 independently matches vector/layout. All21 hashes matched installed PCF names. Native feature decisions unchanged; production adapter/tests being prepared, no gameplay success claimed.'),
(0x155C30,0x30A,'original_particle_random_color_block',
 'int __fastcall {name}(void *initializer, void *preserved_edx, void *collection, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4, unsigned int arg5);',
 'Complete30A-byte native RET18 wrapper, ECX=C_INIT_RandomColor initializer, EDX preserved, collection plus five opaque stack DWORDs. Same original fire/smoke gates, hash lists, RGB min/max writes and restoration as155F40; calls saved original slot7FB8FC. Only other calls are6CF70. Current client RTTI C_INIT_RandomColor vtable D09110 slot30=959B30; actual code reads initializer+5C..70 and collection four-wide attributes. No old Source field operand cells appear in this wrapper. Native decisions unchanged; adapter/native tests under preparation, actual effect unconfirmed.'),
(0x6CF70,0x39,'hash_fnv1a_signed_cstr',
 'unsigned int __cdecl {name}(const char *text);',
 'Complete39-byte leaf, one stack C-string argument, plain RET/caller cleanup. Scans NUL length; starts811C9DC5 and updates h=(h XOR sign_extended_char)*01000193 modulo32bits. MOVSX makes bytes80..FF signed; do not replace with unsigned-byte FNV. Empty string returns seed. No globals/imports/calls. Original particle color wrappers155C30/155F40 call this for definition names; all21 literal hashes independently matched installed particle assets.'),
(0x134810,0xD5,'original_chams_cache_builtin_materials',
 'void __cdecl {name}(void);',
 'CompleteD5-byte no-argument native material-cache initializer, plain RET. XOR-decodes debug/debugambientcube and debug/debugdrawflat from original literals, calls MaterialSystem4DE19C slot84 FindMaterial(name,NULL,false,NULL), saves pointers7DC930/7DC934 and calls each material slot12 AddRef. NativeA287E calls this initializer. No null/error return guard: real verified interfaces/methods/materials required. These two cached pointers are consumed by original132FF0/133C90 model material passes. This bounded initializer is closed; full chams dispatcher152EF0/133470 and player-stage dependencies remain unresolved. No live chams hook or initialization enabled by retention.')]
PARTIAL=(0x152EF0,'PARTIAL model-draw hook: ECX/EDX plus four stack args, RET10; saved original is POINTER SLOT7DC950, not code at runtime44B1C950. Non-pass-through branch calls133470 and clears forced override via4DE1A0 slot1. DrawModelExecute-shaped ABI; current VEngineModel016 identity needs independent proof. Full133470 includes script/Lua callbacks, old owner/resource field operands and class IDs;133719 is an erroneous IDA split inside133470, not a callable entry. Actual native material passes132FF0/133C90 depend on original team/observer/thirdperson state. Do not activate from one interface binding or force7DC938/939 phase flags. See original-chams-recovery-findings.md.')
def retain():
 raw=(ROOT/'primordial.bin').read_bytes();path=ROOT/'analysis/ida/project-index.json';idx=json.loads(path.read_text());meta=ida('get_metadata');assert meta['sha256']==idx['binary_sha256']==hashlib.sha256(raw).hexdigest();assert int(meta['base'],16)==0x44000000
 receipts=[]
 for rva,size,want,template,note in RECORDS:
  a=hex(0x44000000+rva);before=ida('get_function_by_address',address=a);name=before['name'];assert int(before['address'],16)==int(a,16)
  if name.startswith('sub_'):ida('rename_function',function_address=a,new_name=want);name=want
  proto=template.format(name=name);ida('set_function_prototype',function_address=a,prototype=proto)
  asm=ida('disassemble_function',start_address=a);entry=asm['lines'][0];prev='\n'.join(entry.get('comments',[]));target=a if not prev or note in prev else asm['lines'][1]['address'];line=next(x for x in asm['lines'] if int(x['address'],16)==int(target,16));prev='\n'.join(line.get('comments',[]));merged=prev if note in prev else (prev+'\n\n'+note).strip();assert len(merged)<1024
  ida('set_comment',address=target,comment=merged);after=ida('disassemble_function',start_address=a);assert note in json.dumps(after);assert after['name']==name
  row=next((r for r in idx['functions'] if int(r['address'],16)==int(a,16)),None)
  if row is None:row={'address':a,'aliases':[before['name']]};idx['functions'].append(row)
  row.update(name=name,prototype=proto,end_address=hex(int(a,16)+size),byte_sha256=hashlib.sha256(raw[rva:rva+size]).hexdigest())
  if target==a:row['comment']=merged
  else:
   row2=next((r for r in idx['data_comments'] if int(r['address'],16)==int(target,16)),None)
   if row2 is None:idx['data_comments'].append({'address':target,'comment':merged})
   else:row2['comment']=merged
  receipts.append(dict(address=a,name=name,prototype=proto,comment_address=target,comment=merged,readback_verified=True))
 rva,note=PARTIAL;a=hex(0x44000000+rva);asm=ida('disassemble_function',start_address=a)
 target=next(x for x in asm['lines'][:8] if not x.get('comments'))['address'];ida('set_comment',address=target,comment=note);assert note in json.dumps(ida('disassemble_function',start_address=a));idx['data_comments']=[r for r in idx['data_comments'] if int(r['address'],16)!=int(target,16)]+[dict(address=target,comment=note)]
 receipts.append(dict(address=a,comment_address=target,comment=note,status='partial uncertainty note only; name/prototype preserved',readback_verified=True))
 path.write_text(json.dumps(idx,indent=2)+'\n');out=ROOT/'analysis/menu/original-particle-ida-applied.json';out.write_text(json.dumps(dict(binary_sha256=meta['sha256'],database_disk_save_verified=False,records=receipts),indent=2)+'\n');print('Four closed functions named/prototyped/commented/read back/indexed; partial model hook uncertainty retained. Latest database disk save UNVERIFIED.')
if __name__=='__main__':retain()
