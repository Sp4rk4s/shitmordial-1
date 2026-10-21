"""Offline native133470 replay using actual empty script/phase state plus explicit Source fixtures."""
import sys,struct,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'build/analysis-python')]
from test_menu import Process,one
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
pid=one('csgo.exe');ranges=[(0x44B3B9E8,19*12),(0x44B3BAE8,12),(0x447F3BE0,44),(0x44B1C938,8)]
with Process(pid) as p:actual=[(a,p.read(a,n)) for a,n in ranges]
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x1000);raw=(ROOT/'primordial.bin').read_bytes();base=0x44340000;u.mem_map(base,0x1410000);u.mem_write(base,raw)
for a,d in actual:u.mem_write(a,d)
u.mem_map(0x10000000,0x10000);u.mem_map(0x20000000,0x10000);u.mem_map(0x30000000,0x10000)
def w(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*v))
# Explicit TEST interface fixtures, not values installed into any game.
w(0x4481E170,0x20000000);w(0x20000000,0x20000100);w(0x2000010C,0x10000000);u.mem_write(0x10000000,b'\x31\xc0\xc2\x04\x00')
w(0x4481E1A0,0x20000200);w(0x20000200,0x20000300);w(0x44B1C950,0x10000020);u.mem_write(0x10000020,b'\xc2\x10\x00')
w(0x30000000+0x34,0);w(0x1000FFE0,0x10000100,0,0,0x30000000,0)
u.reg_write(UC_X86_REG_ESP,0x1000FFE0);u.reg_write(UC_X86_REG_EBP,0);events=[];steps=[0]
def hook(uc,a,n,data):
 steps[0]+=1
 if a in [base+0x158B70,base+0x162D60,base+0x1339CD,0x10000000,0x10000020]:events.append(hex(a))
 if a==0x10000100:uc.emu_stop()
u.hook_add(UC_HOOK_CODE,hook);u.emu_start(base+0x133470,0,count=10000)
r=dict(pid=pid,kind='OFFLINE actual original code + read-only current native empty state; explicit fake Source methods only in emulator',steps=steps[0],events=events,eip=hex(u.reg_read(UC_X86_REG_EIP)),esp=hex(u.reg_read(UC_X86_REG_ESP)),native_script_lookup_ran=hex(base+0x158B70) in events,script_registry_search_ran=hex(base+0x162D60) in events,returned=u.reg_read(UC_X86_REG_EIP)==0x10000100)
assert r['returned'] and r['native_script_lookup_ran'] and not r['script_registry_search_ran'];out=ROOT/'analysis/menu/chams-empty-script-native-replay.json';out.write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))
