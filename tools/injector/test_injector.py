"""Real LoadLibraryA roundtrip in a disposable host; never uses Steam or CS:GO."""
import subprocess
import sys
from pathlib import Path

injector, host, dll = map(Path, sys.argv[1:])
child = subprocess.Popen([str(host)], stdin=subprocess.PIPE)
try:
    missing = subprocess.run([str(injector), str(child.pid), str(dll) + '.missing'], capture_output=True, text=True, timeout=10)
    assert missing.returncode == 1 and 'does not exist' in missing.stderr, missing
    executable = subprocess.run([str(injector), str(child.pid), str(host)], capture_output=True, text=True, timeout=10)
    assert executable.returncode == 1 and 'PE DLL' in executable.stderr, executable
    first = subprocess.run([str(injector), str(child.pid), str(dll)], capture_output=True, text=True, timeout=20)
    assert first.returncode == 0 and 'Loaded:' in first.stdout, (first.stdout, first.stderr)
    again = subprocess.run([str(injector), str(child.pid), str(dll)], capture_output=True, text=True, timeout=10)
    assert again.returncode == 0 and 'Already loaded:' in again.stdout, (again.stdout, again.stderr)
    child.communicate(b'\n', timeout=5)
    assert child.returncode == 0, 'host did not observe loaded DLL'
    print('PASS: missing DLL/non-DLL rejected, remote LoadLibraryA verified, duplicate load avoided, host observed module')
finally:
    if child.poll() is None:
        child.kill()
        child.wait()
