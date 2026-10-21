"""OFFLINE ONLY: original cache producer block live-ins/output proof.

No process access. Explicit test fixtures supply original category-helper and
Renderable origin dependency results. Never import these fixtures into a DLL.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'build/analysis-python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_XMM0
BASE=0x44340000
STACK=0x10000000
DATA=0x20000000
RECORD=0x448C4120
BLOCKS={'category':(0xF0D1F,0xF0D39),'active_origin':(0xF10E2,0xF1123),'active_opacity':(0xF13BF,0xF147A),'health_history':(0xF147A,0xF1581),'identity_reset':(0xF0C35,0xF0D1F)}

def run(raw,kind,*,enemy=False,origin=(32.,64.,12.),time=2.,dormant_time=0.,dormant_opacity=0.,health=75,previous_health=100,handle=123,old_handle=123,spawn=2.,old_spawn=2.,animation_state=False):
    uc=Uc(UC_ARCH_X86,UC_MODE_32)
    uc.mem_map(BASE,0x1500000);uc.mem_write(BASE,raw)
    uc.mem_map(STACK,0x20000);uc.mem_map(DATA,0x40000)
    uc.mem_map(0x2DD06000,0x1000)
    def word(a,v):uc.mem_write(a,struct.pack('<I',v))
    def floating(a,v):uc.mem_write(a,struct.pack('<f',v))
    def ret(value,cleanup):
        sp=uc.reg_read(UC_X86_REG_ESP);pc=struct.unpack('<I',uc.mem_read(sp,4))[0]
        uc.reg_write(UC_X86_REG_EAX,value);uc.reg_write(UC_X86_REG_ESP,sp+4+cleanup);uc.reg_write(UC_X86_REG_EIP,pc)
    uc.reg_write(UC_X86_REG_ESP,STACK+0x18000);uc.reg_write(UC_X86_REG_EBP,STACK+0x19000)
    uc.reg_write(UC_X86_REG_EDI,0x95E0)
    word(BASE+0x4DE164,DATA);floating(DATA,time)
    word(RECORD+4,DATA+0x100)
    word(DATA+0x100,DATA+0x2200);word(DATA+0x2208,DATA+0x3000)
    word(DATA+0x100+0x28C,handle);floating(DATA+0x100+0x103C0,spawn)
    word(DATA+0x100+0x9960,DATA+0x2400 if animation_state else 0)
    word(RECORD+8,old_handle);floating(RECORD+28,old_spawn)
    if kind=='identity_reset':
        # Only the reset's scalar statuses are intentionally marked. The
        # history container remains an explicit TEST empty native layout.
        for off in [12,16,32,33,38288,38348,38352]:word(RECORD+off,0x77777777)
        for off in [24868,24872,24876,24880]:word(RECORD+off,0)
    word(DATA+0x104,DATA+0x200);word(DATA+0x204,DATA+0x1000)
    word(DATA+0x108,DATA+0x2100);word(DATA+0x2100+40,DATA+0x2000)
    uc.mem_write(DATA+0x300,struct.pack('<3f',*origin))
    word(BASE+0x4DE1B0,DATA+0x40);word(DATA+0x40,DATA+0x4000)
    word(BASE+0x7DB924,1);word(BASE+0x7DB920,0x1184)
    word(DATA+0x4000+0x1184+4,health&0xFFFFFFFF);word(RECORD+84,previous_health)
    floating(RECORD+96,dormant_time);floating(RECORD+100,dormant_opacity)
    for frame_offset,native_constant in [(0x80,0x447A0D78),(0x88,0x447A0D58),(0xC0,0x44753E40)]:
        uc.mem_write(STACK+0x19000-frame_offset,raw[native_constant-BASE:native_constant-BASE+8])
    writes=[];calls=[]
    def on_write(uc,access,address,size,value,user):
        if RECORD<=address<RECORD+0x95E0:writes.append({'offset':hex(address-RECORD),'size':size,'value':hex(value)})
    def on_code(uc,address,size,user):
        if address==BASE+0x36ACC0:
            calls.append('EXPLICIT category helper TEST fixture');ret(int(enemy),4)
        elif address==DATA+0x1000:
            calls.append('EXPLICIT Renderable GetRenderOrigin TEST fixture');ret(DATA+0x300,0)
        elif address==DATA+0x2000:
            calls.append('EXPLICIT Networkable EntIndex TEST fixture');ret(1,0)
        elif address==DATA+0x3000:
            calls.append('EXPLICIT GetRefEHandle TEST fixture');ret(DATA+0x100+0x28C,0)
        elif address==0x2DD06CA0:
            assert uc.reg_read(UC_X86_REG_ECX)==DATA+0x2400
            calls.append('EXPLICIT captured Source animation reset TEST fixture');ret(0,0)
        elif address==BASE+0x3CA7A3:
            # Explicit ordinary math-import model. It is neither a live feature
            # dependency substitute nor proof of current resolved CRT binding.
            value=struct.unpack('<d',uc.reg_read(UC_X86_REG_XMM0).to_bytes(16,'little')[:8])[0]
            result=struct.unpack('<Q',struct.pack('<d',math.sin(value)))[0]
            uc.reg_write(UC_X86_REG_XMM0,result);calls.append('EXPLICIT scalar SSE sin import model');ret(0,0)
    uc.hook_add(UC_HOOK_MEM_WRITE,on_write);uc.hook_add(UC_HOOK_CODE,on_code)
    start,end=BLOCKS[kind];uc.emu_start(BASE+start,BASE+end,count=500)
    assert uc.reg_read(UC_X86_REG_EIP)==BASE+end
    return {'writes':writes,'calls':calls,'category':struct.unpack('<I',uc.mem_read(RECORD+148,4))[0],
            'origin':struct.unpack('<3f',uc.mem_read(RECORD+36,12)),
            'opacity':struct.unpack('<f',uc.mem_read(RECORD+104,4))[0],
            'health_history':struct.unpack('<fII',uc.mem_read(RECORD+76,12)),
            'identity':{'handle':struct.unpack('<I',uc.mem_read(RECORD+8,4))[0],
                        'spawn':struct.unpack('<f',uc.mem_read(RECORD+28,4))[0]}}

def main():
    raw=(ROOT/'primordial.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest()=='e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    cases=[]
    for enemy in [False,True]:
        result=run(raw,'category',enemy=enemy);assert result['category']==int(enemy)
        assert {w['offset'] for w in result['writes']}=={'0x94'}
        cases.append({'case':'category','input':enemy,**result})
    for origin in [(32.,64.,12.),(-32.,0.,120.)]:
        result=run(raw,'active_origin',origin=origin);assert result['origin']==origin
        assert {w['offset'] for w in result['writes']}=={'0x5c','0x58','0x24','0x28','0x2c'}
        cases.append({'case':'origin','input':origin,**result})
    for time,old_alpha in [(2.,0.),(0.,0.),(0.125,0.),(0.25,0.),(0.125,0.5)]:
        result=run(raw,'active_opacity',time=time,dormant_opacity=old_alpha)
        assert 0<=result['opacity']<=1
        assert {w['offset'] for w in result['writes']}=={'0x68'}
        if time>0.25:assert result['opacity']==1
        if time==0:assert result['opacity']<0.000001
        cases.append({'case':'opacity','time':time,'old_dormant_alpha':old_alpha,**result})
    for health in [-5,0,75,100,150]:
        result=run(raw,'health_history',health=health)
        expected={-5:0,0:0,75:75,100:100,150:100}[health]
        assert result['health_history'][2]==expected
        if expected==100:assert not result['writes']
        else:
            assert result['health_history']==(2.,100,expected)
            assert {w['offset'] for w in result['writes']}=={'0x4c','0x50','0x54'}
        cases.append({'case':'health_history','input':health,**result})
    for old_handle,old_spawn,anim in [(123,2.,False),(122,2.,False),(123,1.,False),(123,float('nan'),False),(122,1.,True)]:
        result=run(raw,'identity_reset',old_handle=old_handle,old_spawn=old_spawn,animation_state=anim)
        if old_handle==123 and old_spawn==2.:assert not result['writes']
        else:
            assert result['identity']=={'handle':123,'spawn':2.}
            assert {'0x8','0x1c','0x20','0x21','0xc','0x10'}.issubset({w['offset'] for w in result['writes']})
        cases.append({'case':'identity_reset','old_handle':old_handle,'old_spawn':str(old_spawn),'animation':anim,**result})
    output={'scope':'OFFLINE original blocks, explicit dependency fixtures; no full ESP or production readiness',
            'binary_sha256':hashlib.sha256(raw).hexdigest(),
            'blocks':{name:{'start':hex(a),'end_exclusive':hex(b),'sha256':hashlib.sha256(raw[a:b]).hexdigest()} for name,(a,b) in BLOCKS.items()},'cases':cases}
    (Path(__file__).with_name('original-active-esp-block-proof.json')).write_text(json.dumps(output,indent=2))
    print(f'{len(cases)} original producer-block cases passed; no process access.')
if __name__=='__main__':main()
