"""Provenance checks reject mismatched/ambiguous public-to-private links."""
import sys
from pathlib import Path
import struct
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from audit_feature_patterns import event_link, matches


class Memory:
    def __init__(self):
        self.bytes = bytearray(0x10000)

    def write(self, address, data):
        self.bytes[address:address+len(data)] = data

    def read(self, address, size):
        return bytes(self.bytes[address:address+size])


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.memory = Memory()
        self.memory.write(0x100, b'\xB8'+struct.pack('<I',0x200)+b'\xC3')
        self.memory.write(0x200, struct.pack('<I',0x300))
        self.memory.write(0x320, struct.pack('<II',0x1000,0x2000))
        for address, offset in ((0x1000,0x39),(0x2000,0xA)):
            self.memory.write(address,bytes.fromhex('558bec6a016a00ff7508'))
            self.memory.write(address+offset,b'\xE8'+struct.pack('<i',0x3000-(address+offset+5)))
        self.memory.write(0x3000,bytes.fromhex('558bec83e4f883ec0c8bc15356578db09800'))
        self.facts={'engine.dll':dict(base='0x0',disk_sha256='fixture',
                    entries=[dict(name='GAMEEVENTSMANAGER002',create='0x100')])}

    def link(self):
        return event_link(self.memory,self.facts,[(0x1000,self.memory.read(0x1000,0x4000))])

    def test_overlap_and_wildcards(self):
        self.assertEqual(matches([(0x100,b'AAAA')],[65,65]),[0x100,0x101,0x102])
        self.assertEqual(matches([(0x100,b'A\nB')],[65,None,66]),[0x100])

    def test_linked_target(self):
        linked=self.link()
        self.assertEqual(linked['expected_target'],0x3000)
        self.assertEqual(linked['old_matches'],[])
        self.assertEqual(linked['status'],'target-linked; not activated')

    def test_wrong_second_target(self):
        self.memory.write(0x200B,struct.pack('<i',0x3001-0x200F))
        with self.assertRaisesRegex(RuntimeError,'converge'):
            self.link()

    def test_duplicate_target_prefix(self):
        self.memory.write(0x4000,self.memory.read(0x3000,18))
        with self.assertRaisesRegex(RuntimeError,'ambiguous'):
            self.link()

    def test_factory_changed(self):
        self.memory.write(0x100,b'\xE9')
        with self.assertRaisesRegex(RuntimeError,'factory'):
            self.link()


if __name__=='__main__':
    unittest.main()
