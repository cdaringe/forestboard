#!/usr/bin/env python3
"""Exercise the formatter against a disposable index, including partial staging."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

PROJECT = pathlib.Path(__file__).resolve().parents[1]


class FormatHookTest(unittest.TestCase):
    def test_staged_and_unstaged_content(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            def git(*args):
                return subprocess.check_output(["git", *args], cwd=root)
            git("init", "-q")
            shutil.copy(PROJECT / ".clang-format", root / ".clang-format")
            original = "int main(){return 0;}\n"
            clean = root / "clean.cpp"
            partial = root / "with spaces.cpp"
            clean.write_text(original)
            partial.write_text(original)
            git("add", "clean.cpp", "with spaces.cpp")
            unstaged = "int main(){return 7;} // Unstaged work\n"
            partial.write_text(unstaged)
            subprocess.run(["python3", str(PROJECT / "scripts/format_staged.py")], cwd=root, check=True)
            clean_staged = git("show", ":clean.cpp").decode()
            partial_staged = git("show", ":with spaces.cpp").decode()
            self.assertNotEqual(clean_staged, original)
            self.assertEqual(clean.read_text(), clean_staged)
            self.assertEqual(partial_staged, clean_staged)
            self.assertEqual(partial.read_text(), unstaged)
            self.assertNotIn("return 7", partial_staged)
            subprocess.run(["python3", str(PROJECT / "scripts/format_staged.py")], cwd=root, check=True)
            self.assertEqual(partial.read_text(), unstaged)
            self.assertEqual(git("show", ":clean.cpp").decode(), clean_staged)


if __name__ == "__main__":
    unittest.main()
