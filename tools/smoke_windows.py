#!/usr/bin/env python3
"""Launch the EXE extracted from the actual distribution ZIP, without audio."""
import argparse
import os
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build'))
    args = parser.parse_args()
    archives = list((args.build/'portable').glob('*-portable.zip'))
    if len(archives) != 1:
        raise SystemExit('Expected exactly one portable ZIP')
    screenshots = args.build.resolve()/'previews'
    screenshots.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ToneDock-package-') as temporary:
        directory = Path(temporary)
        with zipfile.ZipFile(archives[0]) as archive:
            for name in archive.namelist():
                if Path(name).is_absolute() or '..' in Path(name).parts:
                    raise SystemExit('Unsafe ZIP path')
            archive.extractall(directory/'unpacked')
        executable, = (directory/'unpacked').glob('*/ToneDock.exe')
        for name, switches, expected in (
            ('pedalboard', [], (1360, 880)),
            ('long-chain-compact', ['--preview-chain', '--compact'], (1120, 780)),
            ('devices', ['--device-page'], (1360, 880)),
            ('catalog', ['--catalog-page'], (1360, 880)),
            ('catalog-filled', ['--catalog-demo'], (1360, 880)),
            ('catalog-chain-compact', ['--catalog-demo', '--catalog-chain', '--compact'], (1120, 780)),
            ('catalog-filters-compact', ['--catalog-demo', '--catalog-chain', '--catalog-filters', '--compact'], (1120, 780)),
        ):
            target = screenshots/f'{name}.png'
            env = {**os.environ, 'TONEDOCK_DATA_DIR': str(directory/f'data-{name}')}
            subprocess.run([str(executable), '--no-audio', f'--screenshot={target}', *switches],
                           env=env, cwd=executable.parent, check=True, timeout=45)
            image = target.read_bytes()
            if image[:8] != b'\x89PNG\r\n\x1a\n':
                raise SystemExit(f'Invalid native PNG: {target}')
            actual = struct.unpack_from('>II', image, 16)
            if actual != expected:
                raise SystemExit(f'Invalid native screenshot: {target}; expected {expected}, got {actual}')
            print(f'PASS packaged EXE: {name} ({expected[0]}x{expected[1]})')


if __name__ == '__main__':
    main()
