#!/usr/bin/env python3
"""Summarize every-frame position gaps; no screenshots, RAM or ROM are exported."""
import argparse
import json
import re
from collections import Counter
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',type=Path)
p.add_argument('--report',type=Path,required=True)
a=p.parse_args()
text=a.log.read_text()
sample_re=(r'\[sma3:wide\] frame=(\d+) matched_object_positions=(\d+) '
           r'submission_age=(-?\d+) screen_objects=(\d+) unresolved_objects=(\d+) '
           r'visible_unresolved_objects=(\d+)')
sample_keys=('frame','world_positions','submission_age','screen_objects',
             'unresolved_objects','vertically_visible_unresolved_objects')
samples=[dict(zip(sample_keys,map(int,m))) for m in re.findall(sample_re,text)]
gap_re=(r'\[sma3:object-gap\] frame=(\d+) coherent=(\d+) unresolved=(\d+) '
        r'visible_unresolved=(\d+) slots=([0-9,]*)')
gaps=re.findall(gap_re,text)
if len(gaps)!=text.count('[sma3:object-gap]'):
    p.error('Incomplete or older gap instrumentation; refusing a false zero-gap result')
if [s['frame'] for s in samples]!=[7200,8000,9599,12000,16000,17999]:
    p.error('Requires a completed instrumented 18000-frame replay')
totals={k:int(v) for k,v in re.findall(r'\b(ppu_frames|dispatch_misses|interpreted_insns|healed_native)=(\d+)',text)}
if totals!={'ppu_frames':18000,'dispatch_misses':0,'interpreted_insns':0,'healed_native':0}:
    p.error('Incomplete run or native execution failure')
visible=[int(f) for f,c,n,v,slots in gaps if int(v)]
incoherent=[int(f) for f,c,n,v,slots in gaps if c=='0']
counts=Counter(slot for f,c,n,v,slots in gaps for slot in slots.split(',') if slot)
report={
    'frames':18000,'samples':samples,
    'frames_with_unresolved_positions':len(gaps),
    'unresolved_slot_frame_counts':dict(counts),
    'frames_with_vertically_visible_unresolved_objects':len(visible),
    'visible_gap_frames':visible,
    'frames_without_coherent_submission':incoherent,
    'visible_position_coverage_passed':not visible and not incoherent and any(s['submission_age']>=0 for s in samples),
    'supported_gameplay_only':True,
    'samples_without_active_submission':[s['frame'] for s in samples if s['submission_age']<0],
    'step_1_complete_for_all_scenes':False,
    'scope':'Object bounding boxes in this replay only. Vertically excluded objects retain unresolved X and native clipping. Does not prove full-game coverage, lateral spawning, Android readiness or performance.'
}
a.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('samples','visible_gap_frames','scope')},indent=2))
