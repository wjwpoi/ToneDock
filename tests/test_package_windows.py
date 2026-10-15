import hashlib
import importlib.util
import json
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

spec = importlib.util.spec_from_file_location('packager', Path(__file__).resolve().parents[1]/'tools/package_windows.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def fake_pe(imported='KERNEL32.dll'):
    data = bytearray(1024)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', data, 0x84, 0x8664, 1)
    struct.pack_into('<H', data, 0x94, 240)
    struct.pack_into('<H', data, 0x98, 0x20b)
    struct.pack_into('<I', data, 0x98+120, 0x1000)
    struct.pack_into('<IIII', data, 0x98+240+8, 512, 0x1000, 512, 512)
    struct.pack_into('<IIIII', data, 512, 0, 0, 0, 0x1080, 0)
    name = imported.encode()+b'\0'
    data[640:640+len(name)] = name
    return data


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)/'source'
        self.build = Path(self.temp.name)/'build'
        self.output = Path(self.temp.name)/'out'
        for name in ('LICENSE', 'packaging/PORTABLE-WINDOWS.md', 'packaging/register-protocol.ps1'):
            path = self.root/name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(name)
        self.exe = self.build/'ToneDockApp_artefacts/Release/ToneDock.exe'
        plugin = self.build/'ToneDock_artefacts/Release/VST3/ToneDock.vst3/Contents/x86_64-win/ToneDock.vst3'
        for path in (self.exe, plugin):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(fake_pe())
        notices = self.build/'notices/LICENSE.txt'
        notices.parent.mkdir()
        notices.write_text('license')

    def test_root_executable_and_manifest_exclude_personal_data(self):
        (self.build/'tokens.json').write_text('must not ship')
        archive = packager.package(self.root, self.build, self.output, '0.2.0', 'testcommit')
        with zipfile.ZipFile(archive) as zipped:
            prefix = 'ToneDock-0.2.0-Windows-x64-portable/'
            self.assertIn(prefix+'ToneDock.exe', zipped.namelist())
            self.assertFalse(any('tokens.json' in n for n in zipped.namelist()))
            manifest = json.loads(zipped.read(prefix+'manifest.json'))
            self.assertEqual(manifest['files']['ToneDock.exe'], hashlib.sha256(self.exe.read_bytes()).hexdigest())
            self.assertEqual(manifest['imports']['ToneDock.exe'], ['KERNEL32.dll'])
        checksum = (self.output/'SHA256SUMS.txt').read_text()
        self.assertIn(packager.sha256(archive), checksum)
        self.assertTrue((self.output/'ToneDock-0.2.0-Windows-x64.exe').is_file())

    def test_missing_input_fails_before_making_archive(self):
        self.exe.unlink()
        with self.assertRaises(FileNotFoundError):
            packager.package(self.root, self.build, self.output, '0.2.0', 'test')
        self.assertFalse(self.output.exists())

    def test_runtime_dependency_rejected(self):
        self.exe.write_bytes(fake_pe('VCRUNTIME140.dll'))
        with self.assertRaisesRegex(ValueError, 'VC runtime'):
            packager.package(self.root, self.build, self.output, '0.2.0', 'test')

    def test_invalid_or_wrong_architecture_rejected(self):
        self.exe.write_bytes(b'not an executable')
        with self.assertRaises(ValueError):
            packager.audit_binary(self.exe)
        data = fake_pe()
        struct.pack_into('<H', data, 0x84, 0x14c)
        self.exe.write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'x64'):
            packager.audit_binary(self.exe)


if __name__ == '__main__':
    unittest.main()
