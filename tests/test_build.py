"""Check GUI build failure handling, discovery, and optional Core packaging."""

from contextlib import redirect_stderr, redirect_stdout
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build


class BuildTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="LEP GUI 测试 ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def test_explicit_missing_msbuild_does_not_fall_back(self):
        with self.assertRaises(FileNotFoundError):
            build.find_msbuild(str(self.root / "missing.exe"))

    def test_packaging_copies_only_available_core_files(self):
        output = self.root / "output"
        output.mkdir()
        source = self.root / "core/out/x64/LoaderDll_x64.dll"
        source.parent.mkdir(parents=True)
        source.write_bytes(b"core")
        with redirect_stdout(io.StringIO()):
            build.copy_core(self.root / "core", output, False)
        self.assertEqual((output / source.name).read_bytes(), b"core")
        self.assertEqual(len(list(output.iterdir())), 1)

    def test_dry_run_does_not_build_create_resources_or_copy(self):
        with patch.object(build, "ROOT", self.root):
            for project, _ in build.PROJECTS:
                path = self.root / project / f"{project}.csproj"
                path.parent.mkdir(exist_ok=True)
                path.touch()
            with patch.object(build, "find_msbuild", return_value=self.root / "MSBuild.exe"), \
                 patch.object(build.subprocess, "run") as run, \
                 patch.object(build.shutil, "copy2") as copy, redirect_stdout(io.StringIO()):
                self.assertEqual(build.main(["--dry-run"]), 0)
            run.assert_not_called()
            copy.assert_not_called()
            self.assertFalse((self.root / "LEPInstaller/Resources").exists())

    @unittest.skipUnless(os.name == "nt", "Windows MSBuild driver")
    def test_failure_stops_following_projects_and_packaging(self):
        with patch.object(build, "find_msbuild", return_value=self.root / "MSBuild.exe"), \
             patch.object(build.subprocess, "run", side_effect=subprocess.CalledProcessError(7, ["MSBuild.exe"])) as run, \
             patch.object(build, "copy_core") as copy, \
             redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(build.main([]), 7)
        self.assertEqual(run.call_count, 1)
        copy.assert_not_called()


if __name__ == "__main__":
    unittest.main()
