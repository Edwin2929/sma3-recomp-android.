#!/usr/bin/env python3
"""Compare bounded private sessions. Publish only the aggregate JSON, not assets."""
import argparse,json,re
from pathlib import Path
from PIL import Image
from make_level_fixture import sections

def compare(control,candidate):
    left=json.loads((control/'session.json').read_text())
    right=json.loads((candidate/'session.json').read_text())
    assert left==right and left, 'Session trajectory differs'
    centers=[]
    for i in range(len(left)):
        a=Image.open(control/f'step-{i:03}.png').convert('RGB')
        b=Image.open(candidate/f'step-{i:03}.png').convert('RGB')
        assert a.size==(240,160) and b.size==(356,160)
        equal=a.tobytes()==b.crop((58,0,298,160)).tobytes()
        assert equal, 'Central image differs'
        centers.append(equal)
    a=(control/'final.state').read_bytes();b=(candidate/'final.state').read_bytes()
    sa=sections(a);sb=sections(b)
    assert sa.keys()==sb.keys()
    exact=[]
    for tag in sa:
        x,n=sa[tag];y,m=sb[tag];aa=a[x:x+n];bb=b[y:y+m]
        assert n==m
        if tag==b'PPU0':continue # Viewport-specific; compare central pixels above.
        if tag==b'BUS0':
            assert n==395416 and aa[395277]==bb[395277]==0, 'Unexpected bus layout or active RTC'
            # Pinned engine serializes wall-clock seconds even for inactive RTC.
            aa=aa[:395310]+bytes(8)+aa[395318:]
            bb=bb[:395310]+bytes(8)+bb[395318:]
        assert aa==bb, f'State differs: {tag!r}'
        exact.append(tag.decode())
    log=(candidate/'run.log').read_text()
    assert 'dispatch_misses=0 interpreted_insns=0 healed_native=0' in log, 'Missing completion counters'
    assert '[sma3:wide]' in log, 'Missing widescreen instrumentation'
    gaps=re.findall(r'\[sma3:object-gap\] frame=(\d+) coherent=(\d+) unresolved=(\d+) visible_unresolved=(\d+)',log)
    visible=sum(int(g[3])>0 for g in gaps)
    incoherent=sum(int(g[1])!=1 for g in gaps)
    return dict(level_id=left[0]['level_id'],sublevel_ids=sorted({x['sublevel_id'] for x in left}),
        frames=sum(x['action']['n'] for x in left),central_images_equal=centers,
        compared_state_sections=exact,bus_exception='Inactive RTC wall-clock seconds only (8 bytes)',
        viewport_state_compared=False,visible_unresolved_frames=visible,incoherent_frames=incoherent,
        raw_gap_frames=len(gaps),differential_pass=True,coverage_pass=not(visible or incoherent),
        synthetic_entry=True,scope='Short entry session only; not full-level, boss, transformation or Android qualification')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('control',type=Path);p.add_argument('candidate',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();result=compare(a.control,a.candidate)
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
    if not result['coverage_pass']:raise SystemExit(1)
if __name__=='__main__':main()
