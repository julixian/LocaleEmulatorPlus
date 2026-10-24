#!/usr/bin/env python3
"""Build LocaleEmulatorPlus (.NET Framework 4.8) with Python 3.10+ and MSBuild."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
PROJECTS = (
    ("LEPCommonLibrary", "AnyCPU"),
    ("LEPContextMenuHandler", "AnyCPU"),
    ("LEPGUI", "AnyCPU"),
    ("LEPUpdater", "AnyCPU"),
    ("LEPProc", "x86"),
    ("LEPProc", "x64"),
    ("LEPInstaller", "AnyCPU"),
)


def find_msbuild(explicit: str | None) -> Path:
    if explicit:
        candidate = Path(explicit).expanduser()
        if candidate.is_file():
            return candidate.resolve()
        found = shutil.which(explicit)
        if found:
            return Path(found)
        raise FileNotFoundError(f"MSBuild not found: {explicit}")

    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if vswhere.is_file():
        result = subprocess.run(
            [str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.Component.MSBuild",
             "-find", r"MSBuild\**\Bin\MSBuild.exe", "-utf8"],
            check=True, capture_output=True, text=True, encoding="utf-8-sig",
        )
        for line in result.stdout.splitlines():
            candidate = Path(line.strip())
            if candidate.is_file():
                return candidate
    found = shutil.which("MSBuild.exe")
    if found:
        return Path(found)
    raise FileNotFoundError("MSBuild not found. Install MSBuild or pass --msbuild PATH.")


def parse_options(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", type=str.capitalize, choices=("Release", "Debug"),
                        default=os.environ.get("CONFIGURATION", "Release").capitalize())
    parser.add_argument("--core-dir", type=Path,
                        default=os.environ.get("CORE_DIR") or ROOT.parent / "LocaleEmulatorPlus-Core",
                        help="Core checkout containing out/x86 and out/x64")
    parser.add_argument("--msbuild", default=os.environ.get("MSBUILD"),
                        help="MSBuild.exe path; otherwise discover it through vswhere or PATH")
    parser.add_argument("--dry-run", action="store_true", help="print commands without building or copying files")
    return parser.parse_args(argv)


def copy_core(core_dir: Path, output: Path, dry_run: bool) -> None:
    for arch in ("x86", "x64"):
        for name in (f"LoaderDll_{arch}.dll", f"LocaleEmulatorPlus_{arch}.dll"):
            source = core_dir / "out" / arch / name
            if not source.is_file():
                print(f"[skip] Core DLL not found: {source}", flush=True)
            elif dry_run:
                print(f"[copy] {source} -> {output / name}", flush=True)
            else:
                shutil.copy2(source, output / name)
                print(f"[copy] {name}", flush=True)


def main(argv: list[str] | None = None) -> int:
    options = parse_options(argv)
    if os.name != "nt":
        print("This build requires Windows and MSBuild.", file=sys.stderr)
        return 1
    try:
        msbuild = find_msbuild(options.msbuild)
        for project, _ in PROJECTS:
            source = ROOT / project / f"{project}.csproj"
            if not source.is_file():
                raise FileNotFoundError(source)
        output = ROOT / "Build" / options.configuration
        core_dir = options.core_dir.resolve()
        print(f"LocaleEmulatorPlus GUI build\n  MSBuild: {msbuild}\n"
              f"  Configuration: {options.configuration}\n  Output: {output}", flush=True)
        if not options.dry_run:
            (ROOT / "LEPInstaller/Resources").mkdir(parents=True, exist_ok=True)
        for project, platform in PROJECTS:
            command = [str(msbuild), "/nologo", "/nr:false", "/v:minimal",
                       str(ROOT / project / f"{project}.csproj"), "/t:Build",
                       f"/p:Configuration={options.configuration}", f"/p:Platform={platform}",
                       f"/p:SolutionDir={ROOT}{os.sep}"]
            print(f"\n[build] {project} ({platform})", flush=True)
            if options.dry_run:
                print(subprocess.list2cmdline(command), flush=True)
            else:
                subprocess.run(command, cwd=ROOT, check=True)
        copy_core(core_dir, output, options.dry_run)
        status = "Dry run completed" if options.dry_run else "Build succeeded"
        print(f"\n{status}.\n  Output: {output}", flush=True)
        return 0
    except subprocess.CalledProcessError as error:
        print(f"Build failed (exit {error.returncode}): {subprocess.list2cmdline(error.cmd)}", file=sys.stderr)
        return error.returncode if 0 < error.returncode < 256 else 1
    except OSError as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Build interrupted.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
