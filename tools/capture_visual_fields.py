"""Read current Source receive-table fields for original camera dependencies.

No target calls or writes; reference catalog offsets are not used as bindings.
"""
import argparse
import json
import struct
from capture_source_interfaces import snapshot
from test_menu import Process,ROOT,one
from audit_feature_patterns import executable_sections,matches


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--pid',type=int);args=parser.parse_args()
    pid=args.pid or one('csgo.exe');facts=snapshot(pid)
    entry=next(e for e in facts['factories']['client.dll']['entries'] if e['name']=='VClient018')
    fields={'m_bIsScoped','m_hActiveWeapon','m_hObserverTarget','m_iObserverMode','m_lifeState','m_iItemDefinitionIndex','m_bAlive'}
    rows=[]
    with Process(pid) as p:
        def word(a):return struct.unpack('<I',p.read(a,4))[0]
        def string(a):return p.read(a,160).split(b'\0')[0].decode('ascii')
        vt=word(int(entry['singleton'],16));method=word(vt+8*4);code=p.read(method,6)
        if code[0]!=0xA1 or code[5]!=0xC3:raise RuntimeError('GetAllClasses getter profile differs')
        current=word(struct.unpack_from('<I',code,1)[0]);seen=set()
        def walk(table,parent=0,path=(),depth=0):
            if depth>16:raise RuntimeError('Receive-table recursion bound')
            props,count,decoder,name=struct.unpack('<4I',p.read(table,16))
            if count>4096:raise RuntimeError('Receive-table count bound')
            table_name=string(name)
            for i in range(count):
                prop=p.read(props+60*i,60);name_ptr,kind=struct.unpack_from('<2I',prop)
                child,offset=struct.unpack_from('<2I',prop,40)
                if offset>0x20000 or parent+offset>0x20000:raise RuntimeError('Receive-table offset bound')
                name=string(name_ptr)
                if name in fields:yield dict(name=name,offset=hex(parent+offset),type=kind,path=[*path,table_name,name])
                if kind==6 and child:yield from walk(child,parent+offset,(*path,table_name),depth+1)
        while current:
            if current in seen or len(seen)>1024:raise RuntimeError('Class-list bound/cycle')
            seen.add(current);data=p.read(current,24);name=string(struct.unpack_from('<I',data,8)[0]);table,current=struct.unpack_from('<2I',data,12)
            if name in ['CCSPlayer','CWeaponCSBase','CBaseCombatWeapon','CBaseAttributableItem','CPlayerResource','CCSPlayerResource']:
                rows.append(dict(class_name=name,fields=list(walk(table))))
        # Existing diagnostic pattern is a lead. Corroborate the resolved object
        # through native RTTI instead of treating a unique match as sufficient.
        pattern=[0x8B,0x3D,None,None,None,None,0x85,0xFF,0x0F,0x84,None,None,None,None,0x81,0xC7]
        hits=matches(executable_sections(p,int(facts['factories']['client.dll']['base'],16)),pattern)
        resource=dict(pattern_matches=[hex(h) for h in hits],scope='Candidate only until singleton RTTI agrees')
        if len(hits)==1:
            slot=word(hits[0]+2);object_pointer=word(slot)
            resource.update(slot=hex(slot),object=hex(object_pointer),instruction=p.read(hits[0],16).hex())
            if object_pointer:
                vtable=word(object_pointer);locator=word(vtable-4);descriptor=word(locator+12)
                type_name=string(descriptor+8)
                resource.update(vtable=hex(vtable),rtti=type_name,rtti_verified=type_name=='.?AVC_CSPlayerResource@@')
    result=dict(pid=pid,scope='Read-only current field evidence; original consumer mapping/live binding remains separate',client_sha256=facts['factories']['client.dll']['disk_sha256'],get_all_classes=hex(method),classes=rows,player_resource=resource)
    (ROOT/'analysis/menu/visual-current-fields.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
