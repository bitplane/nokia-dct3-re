import pathlib
import subprocess
import tempfile
import unittest

from tools.check_mame_patch_stack import check_patch_stack


class MamePatchStackTest(unittest.TestCase):
    def test_replays_ordered_patches_without_touching_worktree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            repo = root / "repo"
            repo.mkdir()

            def git(*args):
                return subprocess.run(
                    ["git", "-C", str(repo), *args],
                    check=True,
                    capture_output=True,
                    text=True,
                ).stdout.strip()

            git("init", "-q")
            git("config", "user.name", "Test")
            git("config", "user.email", "test@example.invalid")
            source = repo / "sample.txt"
            source.write_text("first\n")
            git("add", "sample.txt")
            git("commit", "-qm", "base")
            commit = git("rev-parse", "HEAD")

            source.write_text("second\n")
            first = root / "first.patch"
            first.write_text(git("diff", "--", "sample.txt") + "\n")
            git("add", "sample.txt")
            source.write_text("third\n")
            second = root / "second.patch"
            second.write_text(git("diff", "--", "sample.txt") + "\n")

            check_patch_stack(repo, commit, [first, second])
            self.assertEqual(source.read_text(), "third\n")
            self.assertEqual(git("diff", "--", "sample.txt"), second.read_text().strip())
            with self.assertRaisesRegex(RuntimeError, "first.patch"):
                check_patch_stack(repo, commit, [first, first])


if __name__ == "__main__":
    unittest.main()
