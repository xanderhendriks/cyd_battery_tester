# -*- mode: python ; coding: utf-8 -*-
import git
import platform

from PyInstaller.utils.hooks import collect_data_files


datas = []
datas += collect_data_files('esptool')


def get_git_version():
    repo = git.Repo(search_parent_directories=True)
    tags = sorted(repo.tags, key=lambda t: t.commit.committed_datetime)
    latest_tag = tags[-1] if tags else None
    sha = repo.head.object.hexsha
    return f"{latest_tag}-{sha[:7]}" if latest_tag else sha[:10]


a = Analysis(
    ['cyd-battery-tester-firmware-update.py'],
    pathex=[],
    binaries=[('../esp32/build/bootloader/bootloader.bin', 'binaries'), ('../esp32/build/cyd_battery_tester.bin', 'binaries'), ('../esp32/build/partition_table/partition-table.bin', 'binaries')],
    datas=datas,
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)


exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name=f'cyd-battery-tester-firmware-update-{platform.system().lower()[:3]}-{get_git_version()}',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=['nxs.ico'],
)
