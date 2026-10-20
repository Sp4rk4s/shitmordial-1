"""Retain the actual tested menu results and artifact identities without a rebuild."""
import hashlib
import json
from pathlib import Path
from test_menu import ROOT

def artifact(path):
    return dict(path=str(path.resolve()),sha256=hashlib.sha256(path.read_bytes()).hexdigest())

def main():
    cases=[('menu-preview-1',38484,'Title/frame visible; user reported absent contents',False),
           ('menu-preview-2',25980,'User confirmed original tabs/text/checkboxes/options visible',True),
           ('menu-interactive-1',10836,'Refused stale already-detoured PaintTraverse profile before mutation',False),
           ('menu-interactive-2',10836,'User confirmed dragging, tab switching and basic checkbox interaction work',True)]
    for name,pid,outcome,success in cases:
        folder=ROOT/'build'/name
        tests_folder=ROOT/'build'/('menu-interactive-1' if name=='menu-interactive-2' else name)
        tests=[]
        for arch,expected in [('primordial-x86-native',14),('steam-x64-native',6)]:
            logfile=tests_folder/arch/'Testing/Temporary/LastTest.log'
            if logfile.exists():
                text=logfile.read_text(errors='replace')
                passed=text.count('Test Passed.')
                assert passed==expected and 'Test Failed.' not in text
                tests.append(dict(log=str(logfile),passed=passed))
        dll=folder/'primordial-x86-native/menu_preview.dll'
        value=dict(scope='Actual separate menu-preview validation; gameplay bootstrap unresolved',pid=pid,
            user_observation=outcome,confirmed_scoped_success=success,artifact=artifact(dll),production_checks=tests,
            limitations=['Action callbacks, original gameplay startup and full menu input/scroll lifecycle are not validated.',
                         'Interactive-2 was rebuilt with only clean Source guard regeneration; unchanged production sources passed20 checks in interactive-1. Interactive-2 itself was tested live.']
                        if name.startswith('menu-interactive') else ['Menu-only read-only preview; gameplay/interactive controls not established.'])
        log=folder/'primordial-x86-native/primordial_menu_preview.log'
        if log.exists():
            text=log.read_text(errors='replace')
            value['preview_log']=artifact(log)
            value['live_evidence']={key:[line for line in text.splitlines() if key in line][-10:]
                for key in ['constructor returned','group initial layout returned','DRAW RETURNED','drag begin','drag end','selected tab=','UI value changed','REFUSED','STOP','FAILED']}
            (ROOT/f'analysis/menu/{name}-runtime-log-{pid}.txt').write_text(text)
        screenshot=ROOT/f'analysis/menu/{"menu-interactive" if "interactive" in name else "menu-preview"}-{pid}.png'
        if screenshot.exists():value['screenshot']=artifact(screenshot)
        (folder/'validation.json').write_text(json.dumps(value,indent=2)+'\n')
    print('Actual menu test results and hashes retained for four preview builds.')

if __name__=='__main__':main()
