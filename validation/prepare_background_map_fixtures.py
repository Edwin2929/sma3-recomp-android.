#!/usr/bin/env python3
"""Prepare PRIVATE BG3 oracle fixtures from run_widescreen_probe RAM captures.

The output contains game memory and must not be published. The test uses a
synthetic unique palette, isolating map indices, flips and palette banks.
Only complete 32-metatile-row BG3 images at hardware screenblock 26 qualify.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--frame',type=int,default=7200)
    parser.add_argument('captures',type=Path,nargs='+')
    args=parser.parse_args()
    rom=args.rom.read_bytes()
    if hashlib.sha1(rom).hexdigest()!='7352d2bd064d9ebaec579e264228aa21c7345b80':
        parser.error('Unsupported ROM identity')
    args.output.mkdir(parents=True,exist_ok=True)
    if any(args.output.iterdir()):parser.error('Use a new empty private output directory')
    records=[]
    for index,directory in enumerate(args.captures):
        prefix=directory/f'wide-{args.frame}'
        iwram=Path(str(prefix)+'-50331648.bin').read_bytes()
        ewram=Path(str(prefix)+'-33554432.bin').read_bytes()
        vram=Path(str(prefix)+'-100663296.bin').read_bytes()
        io=Path(str(prefix)+'-io.bin').read_bytes()
        if len(iwram)!=0x8000 or len(ewram)!=0x40000 or len(vram)!=0x18000 or len(io)!=0x400:
            parser.error('Unexpected capture dimensions')
        image_id=struct.unpack_from('<H',iwram,0x4ba6)[0]
        if image_id>0x2f:parser.error('Unsupported BG3 image id')
        pointer=struct.unpack_from('<I',rom,0x16766c+image_id*4)[0]
        if not 0x08000000<=pointer<=0x083ffffc:parser.error('Invalid image pointer')
        header=struct.unpack_from('<I',rom,pointer-0x08000000)[0]
        rows=(header>>8)//64
        control=struct.unpack_from('<H',io,0xe)[0]
        if header&255!=0x10 or rows!=32 or control&0xdf00!=0xda00:
            print(f'SKIP {directory.name}: image is not a complete supported BG3 map')
            continue
        data=b'BGF1'+struct.pack('<I',rows)+ewram+vram+io
        output=args.output/f'bg3-{index:03d}.bgf'
        output.write_bytes(data)
        records.append(dict(file=output.name,source=directory.name,frame=args.frame,
                            image_id=image_id,rows=rows,sha256=hashlib.sha256(data).hexdigest()))
    if not records:parser.error('No qualifying fixtures')
    (args.output/'manifest.json').write_text(json.dumps(records,indent=2)+'\n')
    print(f'Prepared {len(records)} private fixtures')


if __name__=='__main__':main()
