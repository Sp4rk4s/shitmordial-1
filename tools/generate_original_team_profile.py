"""Retain exact original team comparison; describe only field-cell repairs."""
import hashlib
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build/analysis-python'))
from capstone import Cs,CS_ARCH_X86,CS_MODE_32

def main():
    raw=(ROOT/'primordial.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest()=='e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    functions=[(0x36B2B0,0x10C),(0x36ACC0,0x13D),(0xA06C0,0x37),(0x36ABF0,0x66)]
    mapping={0x2DB22972:'resource_team',0x2D92D648:'team',0x2DB5D56F:'survival_team',0x2D8F9170:'life',0x2D9B7EE7:'resource_alive',0x2DA9DC0C:'observer_mode',0x2DAF0360:'observer_target'}
    profiles=[];operands=[];calls=[]
    for start,size in functions:
        fields=[];refs=[];coverage=0
        for ins in Cs(CS_ARCH_X86,CS_MODE_32).disasm(raw[start:start+size],start):
            coverage+=ins.size
            if ins.mnemonic.startswith('j'):assert start<=int(ins.op_str,16)<start+size
            if ins.bytes[0]==0xE8:
                target=ins.address+5+struct.unpack_from('<i',ins.bytes,1)[0]
                assert target in [r for r,n in functions],hex(target)
                refs.append(f'{{0x{ins.address-start+1:X},0x{target:X}}}')
            for address,name in mapping.items():
                index=ins.bytes.find(struct.pack('<I',address))
                if index>=0:fields.append(f'{{0x{ins.address-start+index:X},0x{address:X},&Fields::{name}}}')
        assert coverage==size
        digest=','.join(f'0x{x:02X}' for x in hashlib.sha256(raw[start:start+size]).digest())
        profiles.append(f'{{0x{start:X},0x{size:X},{{{digest}}}}}')
        operands.append('{'+','.join(fields+['{~0u,0,nullptr}'])+'}')
        calls.append('{'+','.join(refs+['{~0u,0}'])+'}')
    text='// Generated exact dump profile; no feature logic translated.\n'
    text+='constexpr Profile profiles[]={'+','.join(profiles)+'};\n'
    text+='constexpr Operand operands[4][12]={'+','.join(operands)+'};\n'
    text+='constexpr Relocation relocations[4][16]={'+','.join(calls)+'};\n'
    (ROOT/'primordial_loader/src/original_team_profile.inc').write_text(text)
    print('Generated original team/observer/local/alive profiles with guarded field and rel32 repairs.')
if __name__=='__main__':main()
