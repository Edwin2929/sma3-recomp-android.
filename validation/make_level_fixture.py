#!/usr/bin/env python3
"""Create a PRIVATE synthetic level-entry state; never overwrite the source."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def sections(data):
    if len(data)<52 or data[:4]!=b'GBAS' or struct.unpack_from('<I',data,4)[0]!=2:
        raise ValueError('Expected version-2 GBAS state')
    if data[8:48]!=b'7352d2bd064d9ebaec579e264228aa21c7345b80':
        raise ValueError('Unsupported ROM identity')
    result={};position=52
    for _ in range(struct.unpack_from('<I',data,48)[0]):
        if position+8>len(data):raise ValueError('Truncated section header')
        tag=data[position:position+4];size=struct.unpack_from('<I',data,position+4)[0]
        start=position+8;position=start+size
        if tag in result or position>len(data):raise ValueError('Invalid section table')
        result[tag]=(start,size)
    if position!=len(data):raise ValueError('Trailing data')
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--level',type=int,choices=(0,1,3),required=True)
    a=p.parse_args();original=a.source.read_bytes()
    try:
        table=sections(original);start,size=table[b'BUS0']
        if size<0x48000:raise ValueError('Truncated bus memory')
        iwram=start+0x40000
        if struct.unpack_from('<I',original,iwram+0x7240)[0]!=0x0300220c:
            raise ValueError('Expected initialized gameplay memory')
        if original[iwram+0x6b05]!=13:raise ValueError('Expected normal-gameplay source state')
    except (ValueError,KeyError) as e:p.error(str(e))
    changes=((0x6288,a.level,2),(0x6a52,0,2),(0x6b05,10,1))
    result=bytearray(original)
    for offset,value,width in changes:
        result[iwram+offset:iwram+offset+width]=value.to_bytes(width,'little')
    # Exclusive creation protects the original state and all previous fixtures.
    with a.output.open('xb') as f:f.write(result)
    print(json.dumps({'synthetic_fixture':True,'level_id':a.level,
        'source_sha256':hashlib.sha256(original).hexdigest(),
        'fixture_sha256':hashlib.sha256(result).hexdigest(),
        'changed_iwram_addresses':[hex(0x03000000+o) for o,v,w in changes],
        'scope':'For diagnostic entry only; not evidence of a naturally played transition.'}))

if __name__=='__main__':main()
