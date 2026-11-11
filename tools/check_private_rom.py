#!/usr/bin/env python3
"""Fail-closed private ROM fixture identity check; does not upload/read soundbank."""
import argparse
import hashlib
import pathlib
import sys

p=argparse.ArgumentParser()
p.add_argument("--rom",type=pathlib.Path,required=True)
p.add_argument("--sha256",required=True,help="SHA256 from independently recorded original ROM")
a=p.parse_args()
if not a.rom.is_file():
    sys.exit("NOT VERIFIED: private ROM absent")
n=a.rom.stat().st_size
if n not in (1048574, 1048576):
    sys.exit(f"FAIL: unsupported original ROM size {n}")
digest=hashlib.sha256(a.rom.read_bytes()).hexdigest()
if len(a.sha256)!=64 or digest.lower()!=a.sha256.lower():
    sys.exit(f"FAIL: wrong private ROM SHA256; computed {digest}")
print(f"P001 ROM identity PASS: {n} bytes, SHA256 {digest}")
