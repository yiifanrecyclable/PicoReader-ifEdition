#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Prepare the unchanged, hash-pinned standard face and preserve existing TF fonts."""
import argparse
import hashlib
import shutil
from pathlib import Path
from urllib.request import urlopen

ROOT=Path(__file__).resolve().parents[1]
URL='https://fonts.gstatic.com/s/notosanssc/v41/k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaG-3FnYw.ttf'
SHA256='09f17deea660cdd9cc2ba1e8b90adad18eabeca73b62b3c5f76b7ec3b61b33e0'
SIZE=10533840
NAME='PicoSans-Medium.ttf'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def install(source,target):
    if target.exists():
        if digest(target)!=SHA256:raise ValueError(f'保留已有不同字体，未覆盖：{target}')
        return
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--src',type=Path,default=ROOT/'build/font-source/NotoSansSC-Medium.ttf')
    p.add_argument('--download',action='store_true')
    p.add_argument('--install-card',action='store_true')
    a=p.parse_args()
    if a.download and not a.src.exists():
        a.src.parent.mkdir(parents=True,exist_ok=True)
        temp=a.src.with_suffix('.part')
        with urlopen(URL,timeout=60) as response,temp.open('wb') as f:shutil.copyfileobj(response,f)
        if temp.stat().st_size!=SIZE or digest(temp)!=SHA256:
            temp.unlink();raise ValueError('官方字库校验失败')
        temp.rename(a.src)
    if a.src.stat().st_size!=SIZE or digest(a.src)!=SHA256:raise ValueError('源字库大小或SHA256不符')
    package=ROOT/'assets/fonts';install(a.src,package/NAME)
    if a.install_card:
        card=ROOT.parent/'TF卡/fonts';install(package/NAME,card/NAME)
        notice=(package/'OFL.txt').read_bytes();target=card/'PicoSans-OFL.txt'
        if not target.exists() or target.read_bytes()!=notice:target.write_bytes(notice)
    print(f'Standard font verified: {SIZE} bytes; {SHA256}')
if __name__=='__main__':main()
