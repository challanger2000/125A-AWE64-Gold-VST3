"""Private V4 nested package resolver: fake-media negative controls only.

No original MS-DOS/Windows95, ROM, Creative VxD, driver, or owner's SF2 used.
"""
from pathlib import Path
import hashlib
import io
import sys
import tempfile
import unittest
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p108_resolve_private_original_package import (
    validate_zip, inspect_original_package,resolve,NESTED_FILENAME
)

def sha(x):return hashlib.sha256(x).hexdigest()

def fake_nested():
    stream=io.BytesIO()
    with zipfile.ZipFile(stream,"w",compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("86Box/86Box.exe",b"MZ"+"fake-not-86box".encode())
        z.writestr("86Box/roms/sound/creative/awe32.raw",b"fake_rom_not_creative")
        z.writestr("86Box/roms/sound/creative/CT4540 PnP.BIN",b"fake_pnp")
        z.writestr("VM/86box.cfg",b"fake-config")
        z.writestr("VM/win95.img",b"no_source_vm")
    return stream.getvalue()

def build_outer(path,pe=None,sf2=None,nested=None,extra=None):
    pe=pe if pe is not None else b"MZ"+b"A"*2048
    sf2=sf2 if sf2 is not None else b"RIFF"+(b"\x00"*4)+b"sfbk"+b"B"*1200
    nested=nested if nested is not None else fake_nested()
    with zipfile.ZipFile(path,"w",compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("P002-PRIVATE/"+NESTED_FILENAME,nested)
        z.writestr("GUEST-EXACT.EXE",pe)
        z.writestr("exact_second_soundfont.sf2",sf2)
        if extra:
            name,data=extra
            z.writestr(name,data)
    return sha(nested),sha(pe),sha(sf2)

class TestOriginalV4NestedResolver(unittest.TestCase):
    def test_exact_three_source_hashes_identified_regardless_of_outer_path(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            n,e,s=build_outer(outer)
            found=inspect_original_package(outer,n,e,s)
            self.assertEqual(found,("P002-PRIVATE/"+NESTED_FILENAME,
                                    "GUEST-EXACT.EXE","exact_second_soundfont.sf2"))
    def test_wrong_nested_original_sha_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            n,e,s=build_outer(outer)
            with self.assertRaisesRegex(ValueError,"SHA-256 mismatch"):
                inspect_original_package(outer,"0"*64,e,s)
    def test_missing_original_stress_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            n,e,s=build_outer(outer)
            with self.assertRaisesRegex(ValueError,"not uniquely"):
                inspect_original_package(outer,n,"f"*64,s)
    def test_missing_original_bank_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            n,e,s=build_outer(outer)
            with self.assertRaisesRegex(ValueError,"not uniquely"):
                inspect_original_package(outer,n,e,"e"*64)
    def test_duplicate_content_not_unique(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            pe=b"MZ"+b"A"*2048
            n,e,s=build_outer(outer,pe=pe,extra=("copy.exe",pe))
            with self.assertRaisesRegex(ValueError,"not uniquely"):
                inspect_original_package(outer,n,e,s)
    def test_duplicate_nested_archive_name_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            n,e,s=build_outer(outer)
            with zipfile.ZipFile(outer,"a") as z:
                z.writestr("other/"+NESTED_FILENAME,b"another")
            with self.assertRaisesRegex(ValueError,"unique pinned"):
                inspect_original_package(outer,n,e,s)
    def test_zip_slip_rejected(self):
        for name in ("../evil.txt","/absolute","folder\\evil","C:/ms/path","a/../../evil"):
            with self.subTest(name=name):
                with tempfile.TemporaryDirectory() as t:
                    outer=Path(t)/"fictional-v4.zip"
                    n,e,s=build_outer(outer,extra=(name,b"evil"))
                    with self.assertRaisesRegex(ValueError,"unsafe"):
                        inspect_original_package(outer,n,e,s)
    def test_symlink_zip_entry_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fake.zip"
            n,e,s=build_outer(outer)
            member=zipfile.ZipInfo("link_to_owner")
            member.create_system=3
            member.external_attr=(0o120777<<16)
            with zipfile.ZipFile(outer,"a") as z:z.writestr(member,b"x")
            with self.assertRaisesRegex(ValueError,"unsafe"):
                inspect_original_package(outer,n,e,s)
    def test_no_overwrite_of_source_or_result(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            n,e,s=build_outer(outer)
            with self.assertRaisesRegex(ValueError,"new directory"):
                resolve(outer,Path(t),n,e,s)
            with self.assertRaisesRegex(ValueError,"new directory"):
                resolve(outer,outer,n,e,s)
    def test_nested_vm_not_real_fails_closed(self):
        # A completely synthetic nested archive is deliberately not 512MiB.
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fictional-v4.zip"
            n,e,s=build_outer(outer)
            with self.assertRaisesRegex(ValueError,"512 MiB"):
                resolve(outer,Path(t)/"new-output",n,e,s)
    def test_bomb_total_size_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            outer=Path(t)/"fake.zip"
            n,e,s=build_outer(outer)
            with zipfile.ZipFile(outer) as z:
                with self.assertRaisesRegex(ValueError,"size exceeds"):
                    validate_zip(z,30)

if __name__=="__main__":unittest.main()
