"""Windows owner original calibrated 86Box starter: fail-closed CI fixtures.

Private Win95/Creative/86Box binaries are deliberately NOT part of CI.
"""
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch
import zipfile
from tools import p002_owner_launch_original_vm as source

class PrivateOriginalOneClickSession(unittest.TestCase):
    def setUp(self):
        x=tempfile.TemporaryDirectory(prefix="p002-original-oneclick-")
        self.addCleanup(x.cleanup)
        self.root=Path(x.name)
        self.archive=self.root/"private.zip"
    def zip(self,files):
        with zipfile.ZipFile(self.archive,"w",
                             compression=zipfile.ZIP_DEFLATED) as z:
            for name in files:
                z.writestr(name,b"fake-private-fixture-never-real")
    def test_incomplete_original_clone_is_no_go(self):
        self.zip(["VM/win95.img","VM/86box.cfg"])
        with self.assertRaisesRegex(ValueError,"Incomplete"):
            source.verified_zip(self.archive)
    def test_unsafe_path_member_is_no_go_before_unzip(self):
        self.zip(list(source.PRIVATE_ARCHIVE_REQUIRED)+["../NOT_PRIVATE.txt"])
        with self.assertRaisesRegex(ValueError,"traversal"):
            source.verified_zip(self.archive)
    def test_unverified_512m_clone_size_rejected(self):
        self.zip(source.PRIVATE_ARCHIVE_REQUIRED)
        with self.assertRaisesRegex(ValueError,"512MiB"):
            source.verified_zip(self.archive)
    def test_linux_no_replacement_for_actual_win95_boot(self):
        # Running this test on a Windows runner must still exercise
        # the non-Windows refusal without replacing os.name globally.
        with patch.object(source,"os",SimpleNamespace(name="posix")):
            with self.assertRaisesRegex(ValueError,"REQUIRES Windows"):
                source.run(self.archive,self.root/"new-win95-session")
    def test_missing_archive_no_fake_capture(self):
        with self.assertRaisesRegex(ValueError,"not found"):
            source.verified_zip(self.root/"absent.zip")

if __name__=="__main__":
    unittest.main()
