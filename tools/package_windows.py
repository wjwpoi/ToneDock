#!/usr/bin/env python3
"""Make a ready-to-run Windows package from explicit, audited build inputs."""
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zipfile
from pathlib import Path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pe_imports(path):
    data = path.read_bytes()
    if data[:2] != b'MZ':
        raise ValueError(f'Not a Windows executable: {path}')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, sections = struct.unpack_from('<HH', data, pe+4)
    if machine != 0x8664:
        raise ValueError('Windows package requires x64 binaries')
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    optional = pe+24
    if struct.unpack_from('<H', data, optional)[0] != 0x20b:
        raise ValueError('Expected PE32+ executable')
    import_rva = struct.unpack_from('<I', data, optional+112+8)[0]
    mappings = []
    for index in range(sections):
        section = optional+optional_size+index*40
        virtual_size, rva, size, offset = struct.unpack_from('<IIII', data, section+8)
        mappings.append((rva, max(size, virtual_size), offset, size))

    def position(rva):
        for base, size, offset, raw_size in mappings:
            if base <= rva < base+size and rva-base < raw_size:
                return offset+rva-base
        raise ValueError(f'Unmapped PE RVA: {rva}')

    imports = []
    if import_rva:
        descriptor = position(import_rva)
        for index in range(1024):
            fields = struct.unpack_from('<IIIII', data, descriptor+index*20)
            if not any(fields):
                break
            start = position(fields[3])
            end = data.index(b'\0', start, start+512)
            imports.append(data[start:end].decode('ascii'))
        else:
            raise ValueError('Unterminated import table')
    return sorted(set(imports), key=str.lower)


def audit_binary(path):
    imports = pe_imports(path)
    external_runtime = [name for name in imports if re.match(r'(vcruntime|msvcp|msvcr|concrt).*\.dll$', name, re.I)]
    if external_runtime:
        raise ValueError(f'Portable binary depends on VC runtime: {external_runtime}')
    return imports


def package(root, build, destination, version, commit):
    exe = build/'ToneDockApp_artefacts/Release/ToneDock.exe'
    plugin = build/'ToneDock_artefacts/Release/VST3/ToneDock.vst3'
    notices = build/'notices'
    for required in (exe, plugin, notices, root/'LICENSE', root/'packaging/PORTABLE-WINDOWS.md'):
        if not required.exists():
            raise FileNotFoundError(f'Missing package input: {required}')
    binaries = [exe]+[p for p in plugin.rglob('*') if p.is_file() and p.suffix == '.vst3'] if plugin.is_dir() else [exe, plugin]
    if len(binaries) < 2:
        raise ValueError('Missing VST3 binary')
    imports = {p.name: audit_binary(p) for p in binaries}
    name = f'ToneDock-{version}-Windows-x64-portable'
    staging = destination/name
    if staging.exists():
        raise FileExistsError(f'Package destination already exists: {staging}')
    staging.mkdir(parents=True)
    shutil.copy2(exe, staging/'ToneDock.exe')
    (staging/'plugins').mkdir()
    if plugin.is_dir():
        shutil.copytree(plugin, staging/'plugins/ToneDock.vst3')
    else:
        shutil.copy2(plugin, staging/'plugins/ToneDock.vst3')
    shutil.copytree(notices, staging/'licenses')
    shutil.copy2(root/'LICENSE', staging/'LICENSE.txt')
    shutil.copy2(root/'packaging/PORTABLE-WINDOWS.md', staging/'使用说明.md')
    shutil.copy2(root/'packaging/register-protocol.ps1', staging/'register-protocol.ps1')
    manifest = {'version': version, 'commit': commit, 'platform': 'Windows-x64',
                'source': f'https://github.com/wjwpoi/ToneDock/tree/{commit}',
                'imports': imports, 'files': {p.relative_to(staging).as_posix(): sha256(p) for p in sorted(staging.rglob('*')) if p.is_file()}}
    (staging/'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination/f'{name}.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for path in sorted(staging.rglob('*')):
            if path.is_file():
                output.write(path, path.relative_to(destination).as_posix())
    # The standalone application is also available as a single EXE download.
    single = destination/f'ToneDock-{version}-Windows-x64.exe'
    shutil.copy2(exe, single)
    checksum = destination/'SHA256SUMS.txt'
    checksum.write_text(''.join(f'{sha256(p)}  {p.name}\n' for p in (archive, single)), encoding='ascii')
    return archive


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=root/'build')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    version = re.search(r'project\(ToneDock VERSION ([0-9.]+)', (root/'CMakeLists.txt').read_text()).group(1)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    print(package(root, args.build.resolve(), (args.output or args.build/'portable').resolve(), version, commit))


if __name__ == '__main__':
    main()
