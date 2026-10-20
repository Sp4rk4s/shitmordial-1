"""Retain partial menu-input dependencies without claiming full callback recovery."""
import json
from prepare_menu_preview import ROOT,ida

NOTES={
 '0x44350DBC':'Bounded layout excerpt in partial updater3504B0: stores window+5C=x(+3C),+60=y(+40)+titleHeight(+D0),+64=width(+44),+68=height(+48)-titleHeight-(D4==D8?8:55). Constructor does not initialize these fields. Live38484 values were invalid and hid content. Scoped preview reproduces these stores; interactive preview repeats geometry on drag/tab change, without calling full3504B0. Pure34DB30 copies this rectangle into the selected tab.',
 '0x4434CE73':'Bounded tab-updater layout tail34CE73..34CEAE: selected group via tab+84 vector of20-byte entries and tab+80 index. Rectangle from tab+6C/+70/+74/+78 removes130-pixel sidebar (x+=130,width-=130), then calls pure34B860. Next34CEC8 calls34A4B0 forwarding blocked-input flag. Full updater includes input and remains partial; preview reproduces geometry and settled animation data only.',
 '0x4434A4B0':'Partial group input/layout updater, ECX=group, stack blocked-input flag, RET4 at34AB6C. Geometry34A8B0..34AA9E chooses1..4 columns with minimum276 width and16 gutters, calls width setter3372A0 and child position virtual3. Child input virtual1 gets true when caller flag set (34AB00..19). Top scroll branch may enter protected helpers if group+68 nonzero; contract unresolved. Working interactive preview first sets+68=0 to skip that branch, calls layout with blocked=true on initial layout/drag/tab changes, then ticks only known observer-free value controls separately. Gameplay callbacks/full3582C0 poll not invoked.',
 '0x4433B631':'Actual menu content test38484 faulted at runtime4467B646 readingA0. This branch loads missing input global4481E160 and invokes virtual40(vtable+A0) with button codeEDX to obtain a key name. Corrected preview binds independently verified current InputSystemVersion001 before layout and verifies72->INS. Contents visible and confirmed in25980; interactive follow-up10836. Full containing keybind routine remains partial. Evidence menu-content-fault-38484.log/dmp.',
 '0x443582C0':'Partial original input poll/dispatch. Source input4481E160 virtual56 populates cursor44B47F00/+4, virtual15 polls114 button codes; states44B47F08:0 up,1 first down,2 held,3 first up. Updates durations44B48460 and callback/toggle registries, and queries engine4481E174 virtual26/124 plus input analog18(3) for wheel. Full callback/lifetime contract unresolved. Interactive menu preview does NOT call this dispatcher: it supplies only Win32 client cursor and left-button107 transitions, avoiding feature callback dispatch.',
 '0x4432EF80':'Partial native checkbox UI updater, ECX=control, one blocked-input stack flag, RET4. Geometry/attachment and hover updates precede blocked branch; unblocked rectangle hit plus input_button_pressed_this_frame(107) toggles local bytecontrol+138 then control_notify_value_observers335F10. Interactive preview permits this native updater only for empty observer vectorCC/D0 and empty attached-child vector124/128. Actual10836 UI click toggled object448202A0 byte0->1->0 and user confirmed controls work. Shared tooltip/attachment helpers remain partial; no claim of recovered gameplay binding.',
 '0x4432DBC0':'Partial action-button input updater. When unblocked and hit/left edge, paths call callback objectcontrol+154 virtual2, or enter confirmation statecontrol+8=2 when byte+158 set. Confirmation branch invokes same callback after hit test. These callback contracts are not recovered; current scoped interactive preview leaves action-button updates disabled while tabs/drag/basic values work. Do not equate callback rendering with validated config/gameplay actions.'
}

def main():
    paths=[ROOT/'analysis/menu/hook-helper-annotations-pending.json',ROOT/'analysis/ida/project-index.json']
    documents=[json.loads(p.read_text()) for p in paths]
    for address,comment in NOTES.items():
        ida('set_comment',address=address,comment=comment)
        for document in documents:
            document['data_comments']=[r for r in document.get('data_comments',[]) if r['address'].lower()!=address.lower()]+[dict(address=address,comment=comment)]
    for path,document in zip(paths,documents):path.write_text(json.dumps(document,indent=2)+'\n')
    (ROOT/'analysis/menu/menu-input-notes-applied.json').write_text(json.dumps(dict(
        scope='Partial-dependency address comments applied and indexed; full routines remain partial; latest IDA disk save unverified',notes=NOTES),indent=2)+'\n')
    print('Seven bounded input/layout notes retained in live IDA and replay index.')

if __name__=='__main__':main()
