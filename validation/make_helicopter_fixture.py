#!/usr/bin/env python3
"""Create a private synthetic entry near the real 1-2 helicopter bubble."""
import argparse,hashlib,json,struct
from pathlib import Path
from make_level_fixture import sections

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();original=a.source.read_bytes()
    try:
        start,size=sections(original)[b'BUS0'];iwram=start+0x40000
        if size<0x48000 or struct.unpack_from('<I',original,iwram+0x7240)[0]!=0x0300220c:
            raise ValueError('Expected initialized gameplay memory')
        if original[iwram+0x6b05]!=13:raise ValueError('Expected normal-gameplay source')
    except (KeyError,ValueError) as e:p.error(str(e))
    result=bytearray(original)
    # Ask the original level initializer to consume one synthetic screen exit.
    # This loads the actual sublevel and its original sprite/graphics data.
    changes=((0x6288,1,'H'),(0x6a52,1,'H'),(0x6b05,10,'B'),(0x224a,0,'H'))
    for offset,value,fmt in changes:struct.pack_into('<'+fmt,result,iwram+offset,value)
    # Sublevel 3B, tile (63,70), entrance animation 2, existing scroll settings.
    result[start+0x1b000:start+0x1b006]=bytes((0x3b,63,70,2,0x77,4))
    with a.output.open('xb') as f:f.write(result)
    print(json.dumps(dict(synthetic_entry=True,level_id=1,sublevel_id=0x3b,
        source_sha256=hashlib.sha256(original).hexdigest(),fixture_sha256=hashlib.sha256(result).hexdigest(),
        changed_iwram_addresses=[hex(0x03000000+o) for o,v,f in changes],
        changed_ewram_range='0201B000..0201B005',
        scope='Entry fixture only. Transformation must be triggered by input and verified separately.')))
if __name__=='__main__':main()
