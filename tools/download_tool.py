#!/usr/bin/env python3
"""Download the toolchain into tools/ (binutils, wibo, objdiff-cli, MWCC).

Usage: python3 tools/download_tool.py [binutils|wibo|objdiff|compiler ...]
With no arguments, downloads everything that is missing.
"""
import io
import platform
import shutil
import stat
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"

BINUTILS_VERSION = "v0.10"
WIBO_VERSION = "1.2.0"
OBJDIFF_VERSION = "v3.8.2"
COMPILER = "mwcps2-3.0.3-020716"


def host() -> tuple[str, str]:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if machine in ("amd64", "x86_64"):
        machine = "x86_64"
    elif machine in ("arm64", "aarch64"):
        machine = "arm64"
    return system, machine


def fetch(url: str) -> bytes:
    print(f"downloading {url}")
    with urllib.request.urlopen(url) as r:
        return r.read()


def make_executable(path: Path) -> None:
    path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def safe_extract_tar(data: bytes, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        tar.extractall(dest, filter="data")


def binutils_dir() -> Path:
    return TOOLS / "binutils"


def download_binutils() -> None:
    system, machine = host()
    name = {
        ("darwin", "arm64"): "macos-arm64",
        ("linux", "x86_64"): "linux-x86-64",
        ("windows", "x86_64"): "windows-x86-64",
    }.get((system, machine))
    if name is None:
        sys.exit(f"no prebuilt binutils for {system}/{machine}; build decompals/binutils-mips-ps2-decompals yourself")
    base = f"https://github.com/decompals/binutils-mips-ps2-decompals/releases/download/{BINUTILS_VERSION}"
    if system == "windows":
        data = fetch(f"{base}/binutils-mips-ps2-decompals-{name}.zip")
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            z.extractall(binutils_dir())
    else:
        safe_extract_tar(fetch(f"{base}/binutils-mips-ps2-decompals-{name}.tar.gz"), binutils_dir())
        for f in binutils_dir().iterdir():
            make_executable(f)


def download_wibo() -> None:
    system, machine = host()
    if system == "windows":
        return  # MWCC runs natively
    asset = "wibo-macos" if system == "darwin" else "wibo-x86_64"
    out = TOOLS / "bin" / "wibo"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(fetch(f"https://github.com/decompals/wibo/releases/download/{WIBO_VERSION}/{asset}"))
    make_executable(out)


def download_objdiff() -> None:
    system, machine = host()
    os_name = {"darwin": "macos", "linux": "linux", "windows": "windows"}[system]
    arch = {"x86_64": "x86_64", "arm64": "arm64" if system != "linux" else "aarch64"}[machine]
    ext = ".exe" if system == "windows" else ""
    out = TOOLS / "bin" / f"objdiff-cli{ext}"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(
        fetch(f"https://github.com/encounter/objdiff/releases/download/{OBJDIFF_VERSION}/objdiff-cli-{os_name}-{arch}{ext}")
    )
    make_executable(out)


def download_compiler() -> None:
    dest = TOOLS / "compilers" / COMPILER
    if dest.exists():
        shutil.rmtree(dest)
    data = fetch(f"https://github.com/decompme/compilers/releases/download/compilers/{COMPILER}.tar.gz")
    safe_extract_tar(data, dest)


TOOLSET = {
    "binutils": (download_binutils, lambda: binutils_dir() / "mips-ps2-decompals-as"),
    "wibo": (download_wibo, lambda: TOOLS / "bin" / "wibo"),
    "objdiff": (download_objdiff, lambda: TOOLS / "bin" / "objdiff-cli"),
    "compiler": (download_compiler, lambda: TOOLS / "compilers" / COMPILER / "mwccps2.exe"),
}


def ensure(names=None) -> None:
    system, _ = host()
    for name in names or TOOLSET:
        download, marker = TOOLSET[name]
        if name == "wibo" and system == "windows":
            continue
        path = marker()
        if system == "windows" and name in ("objdiff",):
            path = path.with_suffix(".exe")
        if not path.exists():
            download()


if __name__ == "__main__":
    names = sys.argv[1:] or None
    for n in names or []:
        if n not in TOOLSET:
            sys.exit(f"unknown tool {n!r}; choose from {', '.join(TOOLSET)}")
    if names:
        for n in names:
            TOOLSET[n][0]()
    else:
        ensure()
