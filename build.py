#!/usr/bin/env python3
"""Build LocaleEmulatorPlus (native C++23 via mcpp + optional WinUI 3 Store MSIX) and stage release artifacts.

Requires Windows, Python 3.10+, mcpp, and Windows 10/11 SDK (for makeappx/signtool).
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


kScriptRoot = Path(__file__).resolve().parent
kVersion = "3.0.1"
kDefaultSdkRoot = Path(r"C:\Program Files (x86)\Windows Kits\10")
kDefaultSdkVersion = "10.0.26100.0"
kPublisher = "CN=LocaleEmulatorPlus"


def parseOptions(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=("release", "fast-release"), default="release")
    parser.add_argument("--core-dir", dest="coreDir", type=Path,
                        default=os.environ.get("CORE_DIR") or (kScriptRoot.parent / "LocaleEmulatorPlus-Core"),
                        help="LocaleEmulatorPlus-Core directory containing out/x86 and out/x64")
    parser.add_argument("--sdk-root", dest="sdkRoot", type=Path,
                        default=os.environ.get("SDK_ROOT") or kDefaultSdkRoot)
    parser.add_argument("--sdk-version", dest="sdkVersion",
                        default=os.environ.get("SDK_VER") or kDefaultSdkVersion)
    parser.add_argument("--pfx", type=Path, default=None,
                        help="Optional .pfx certificate path for signing LEPShellExt.msix")
    parser.add_argument("--pfx-password", dest="pfxPassword", default="",
                        help="Password for --pfx (empty for auto-generated dev cert)")
    parser.add_argument("--skip-sparse", dest="skipSparse", action="store_true",
                        help="Skip building Win11 LEPShellExt.msix sparse package")
    parser.add_argument("--skip-native", dest="skipNative", action="store_true",
                        help="Reuse existing out/dist native binaries (useful when iterating on LEPWinUI)")
    parser.add_argument("--store-msix", dest="storeMsix", action="store_true",
                        help="Also build full Microsoft Store MSIX package from packaging/AppxManifest.store.xml")
    parser.add_argument("--zip", dest="makeZip", action="store_true",
                        help="Create portable release zip archive in out/")
    parser.add_argument("--dry-run", dest="dryRun", action="store_true")
    return parser.parse_args(argv)


def runCommand(cmd: list[str], cwd: Path, dryRun: bool, env: dict[str, str] | None = None) -> None:
    print(f"> {subprocess.list2cmdline(cmd)}", flush=True)
    if not dryRun:
        subprocess.run(cmd, cwd=cwd, check=True, env=env)


def findBuiltArtifact(targetRoot: Path, triple: str, fileName: str) -> Path:
    candidates = sorted(
        (targetRoot / triple).rglob(fileName),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        raise FileNotFoundError(f"Built artifact not found: {fileName} under {targetRoot / triple}")
    return candidates[0]


def findSdkTool(sdkRoot: Path, sdkVersion: str, toolName: str) -> Path:
    candidate = sdkRoot / "bin" / sdkVersion / "x64" / toolName
    if candidate.is_file():
        return candidate
    found = shutil.which(toolName)
    if found:
        return Path(found)
    raise FileNotFoundError(f"Windows SDK tool not found: {toolName} (checked {candidate})")


def ensureSelfSignedCert(signingDir: Path, dryRun: bool) -> tuple[Path, Path, str]:
    pfxPath = signingDir / "LEPShellExt.pfx"
    cerPath = signingDir / "LEPShellExt.cer"
    password = "lep-dev-cert-password"
    if pfxPath.is_file() and cerPath.is_file():
        return pfxPath, cerPath, password

    if dryRun:
        print(f"[cert] would generate self-signed cert in {signingDir}", flush=True)
        return pfxPath, cerPath, password

    signingDir.mkdir(parents=True, exist_ok=True)
    psScript = (
        "$ErrorActionPreference='Stop'; "
        f"$cert = New-SelfSignedCertificate -Type Custom -Subject '{kPublisher}' "
        "-KeyUsage DigitalSignature -FriendlyName 'Locale Emulator Plus Shell Extension' "
        "-CertStoreLocation 'Cert:\\CurrentUser\\My' "
        "-TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}'); "
        f"$pwd = ConvertTo-SecureString -String '{password}' -Force -AsPlainText; "
        f"Export-PfxCertificate -Cert $cert -FilePath '{pfxPath}' -Password $pwd | Out-Null; "
        f"Export-Certificate -Cert $cert -FilePath '{cerPath}' | Out-Null; "
        "Remove-Item -Path $cert.PSPath -Force"
    )
    psEnv = {k: v for k, v in os.environ.items() if k.upper() != "PSMODULEPATH"}
    runCommand(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", psScript],
               kScriptRoot, dryRun=False, env=psEnv)
    return pfxPath, cerPath, password


def buildSparsePackage(distDir: Path, sdkRoot: Path, sdkVersion: str,
                       pfxOpt: Path | None, pfxPassword: str, dryRun: bool) -> None:
    makeappx = findSdkTool(sdkRoot, sdkVersion, "makeappx.exe")
    makepri = findSdkTool(sdkRoot, sdkVersion, "makepri.exe")
    signtool = findSdkTool(sdkRoot, sdkVersion, "signtool.exe")

    stageDir = kScriptRoot / "out" / "sparse-stage"
    priConfig = kScriptRoot / "out" / "sparse-priconfig.xml"
    msixOut = distDir / "LEPShellExt.msix"
    cerOut = distDir / "LEPShellExt.cer"

    if not dryRun:
        if stageDir.exists():
            shutil.rmtree(stageDir)
        (stageDir / "Assets").mkdir(parents=True, exist_ok=True)
        shutil.copy2(kScriptRoot / "packaging" / "AppxManifest.sparse.xml", stageDir / "AppxManifest.xml")
        for png in (kScriptRoot / "packaging" / "Assets").glob("*.png"):
            shutil.copy2(png, stageDir / "Assets" / png.name)

    runCommand([str(makepri), "createconfig", "/cf", str(priConfig), "/dq", "en-US", "/o"],
               kScriptRoot, dryRun)
    runCommand([
        str(makepri), "new",
        "/pr", str(stageDir),
        "/cf", str(priConfig),
        "/mn", str(stageDir / "AppxManifest.xml"),
        "/of", str(stageDir / "resources.pri"),
        "/o",
    ], kScriptRoot, dryRun)

    runCommand([str(makeappx), "pack", "/d", str(stageDir), "/p", str(msixOut), "/nv", "/o"],
               kScriptRoot, dryRun)

    if pfxOpt is not None:
        pfxPath = pfxOpt.resolve()
        password = pfxPassword
        cerSrc = pfxPath.with_suffix(".cer")
    else:
        pfxPath, cerSrc, password = ensureSelfSignedCert(kScriptRoot / "out" / "signing", dryRun)

    signCmd = [str(signtool), "sign", "/fd", "SHA256", "/f", str(pfxPath)]
    if password:
        signCmd += ["/p", password]
    signCmd.append(str(msixOut))
    runCommand(signCmd, kScriptRoot, dryRun)

    if not dryRun and cerSrc.is_file():
        shutil.copy2(cerSrc, cerOut)


def buildStorePackage(distDir: Path, sdkRoot: Path, sdkVersion: str, dryRun: bool) -> None:
    makeappx = findSdkTool(sdkRoot, sdkVersion, "makeappx.exe")
    winuiProj = kScriptRoot / "LEPWinUI" / "LEPWinUI.csproj"
    winuiOut = kScriptRoot / "out" / "winui-publish"
    stageDir = kScriptRoot / "out" / "store-stage"
    msixOut = kScriptRoot / "out" / f"LocaleEmulatorPlus-{kVersion}-x64.msix"

    if not dryRun and winuiOut.exists():
        shutil.rmtree(winuiOut)

    runCommand([
        "dotnet", "publish", str(winuiProj),
        "-c", "Release",
        "-r", "win-x64",
        "-p:Platform=x64",
        "-o", str(winuiOut),
    ], kScriptRoot, dryRun)

    if not dryRun:
        if stageDir.exists():
            shutil.rmtree(stageDir)
        shutil.copytree(winuiOut, stageDir, ignore=shutil.ignore_patterns("*.pdb"))
        # 合并原生 C++ 引擎（LEP.exe / LEPShellExt.dll / LEPHelper_x86.exe / Core DLL / locales）
        for item in distDir.iterdir():
            if item.name in ("LEPShellExt.msix", "LEPShellExt.cer"):
                continue
            dst = stageDir / item.name
            if item.is_dir():
                if dst.exists():
                    shutil.rmtree(dst)
                shutil.copytree(item, dst)
            else:
                shutil.copy2(item, dst)

        (stageDir / "Assets").mkdir(parents=True, exist_ok=True)
        shutil.copy2(kScriptRoot / "packaging" / "AppxManifest.store.xml", stageDir / "AppxManifest.xml")
        for png in (kScriptRoot / "packaging" / "Assets").glob("*.png"):
            shutil.copy2(png, stageDir / "Assets" / png.name)

    runCommand([str(makeappx), "pack", "/d", str(stageDir), "/p", str(msixOut), "/o"],
               kScriptRoot, dryRun)


def copyCoreDlls(coreDir: Path, distDir: Path, dryRun: bool) -> None:
    for arch in ("x86", "x64"):
        for name in (f"LoaderDll_{arch}.dll", f"LocaleEmulatorPlus_{arch}.dll"):
            src = coreDir / "out" / arch / name
            if not src.is_file():
                print(f"[warn] Core DLL not found: {src}", flush=True)
                continue
            if dryRun:
                print(f"[copy] {src} -> {distDir / name}", flush=True)
            else:
                shutil.copy2(src, distDir / name)
                print(f"[copy] {name}", flush=True)


def createPortableZip(distDir: Path, dryRun: bool) -> Path:
    zipPath = kScriptRoot / "out" / f"LocaleEmulatorPlus-{kVersion}-x64.zip"
    if dryRun:
        print(f"[zip] would pack {distDir} -> {zipPath}", flush=True)
        return zipPath
    with zipfile.ZipFile(zipPath, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for filePath in sorted(distDir.rglob("*")):
            if filePath.is_file():
                zf.write(filePath, filePath.relative_to(distDir))
    print(f"[zip] {zipPath}", flush=True)
    return zipPath


def main(argv: list[str] | None = None) -> int:
    options = parseOptions(argv)
    if os.name != "nt":
        print("This build requires Windows.", file=sys.stderr)
        return 1

    try:
        distDir = kScriptRoot / "out" / "dist"
        if not options.skipNative:
            print(f"=== Building LocaleEmulatorPlus {kVersion} ({options.profile}) ===", flush=True)
            runCommand(["mcpp", "build", "-p", "lep", "--profile", options.profile, "--static"],
                       kScriptRoot, options.dryRun)
            runCommand(["mcpp", "build", "-p", "lep-shell-ext", "--profile", options.profile, "--static"],
                       kScriptRoot, options.dryRun)
            runCommand(["mcpp", "build", "-p", "lep-helper", "--target", "i686-windows-msvc",
                        "--profile", options.profile, "--static"],
                       kScriptRoot, options.dryRun)

            if not options.dryRun:
                if distDir.exists():
                    shutil.rmtree(distDir)
                distDir.mkdir(parents=True, exist_ok=True)

                targetRoot = kScriptRoot / "target"
                shutil.copy2(findBuiltArtifact(targetRoot, "x86_64-windows-msvc", "LEP.exe"), distDir / "LEP.exe")
                shutil.copy2(findBuiltArtifact(targetRoot, "x86_64-windows-msvc", "LEPShellExt.dll"), distDir / "LEPShellExt.dll")
                shutil.copy2(findBuiltArtifact(targetRoot, "i686-windows-msvc", "LEPHelper_x86.exe"), distDir / "LEPHelper_x86.exe")
                shutil.copytree(kScriptRoot / "assets" / "locales", distDir / "locales")
                # 稀疏包通过 ExternalLocation 指向本目录，Windows 11 设置页会从 PackageRootFolder\Assets 加载图标
                shutil.copytree(kScriptRoot / "packaging" / "Assets", distDir / "Assets")

            copyCoreDlls(options.coreDir.resolve(), distDir, options.dryRun)

            if not options.skipSparse:
                buildSparsePackage(distDir, options.sdkRoot, options.sdkVersion,
                                   options.pfx, options.pfxPassword, options.dryRun)

        if options.storeMsix:
            buildStorePackage(distDir, options.sdkRoot, options.sdkVersion, options.dryRun)

        if options.makeZip:
            createPortableZip(distDir, options.dryRun)

        print(f"\nBuild succeeded. Output: {distDir}", flush=True)
    except (subprocess.CalledProcessError, OSError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
