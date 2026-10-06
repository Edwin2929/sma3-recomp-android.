#!/usr/bin/env python3
"""Compare identical replay runs, separating inherited bus warnings from changes."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
from PIL import Image, ImageChops

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('control',type=Path)
p.add_argument('candidate',type=Path)
p.add_argument('--report',type=Path,required=True)
a=p.parse_args()
def load(folder):
    log=(folder/'run.log').read_text()
    fields={k:int(v) for k,v in re.findall(r'\b(unmapped|io_unhandled|ppu_frames|dispatch_misses|interpreted_insns|healed_native)=(\d+)',log)}
    warnings=Counter(re.findall(r'\[gba:bus\] UNMAPPED .*',log))
    return fields,warnings
baseline,bus_base=load(a.control)
candidate,bus_candidate=load(a.candidate)
names=sorted(f.name for f in a.control.glob('wide-*.bin'))
candidate_names=sorted(f.name for f in a.candidate.glob('wide-*.bin'))
mismatches=[name for name in names if not (a.candidate/name).is_file() or (a.control/name).read_bytes()!=(a.candidate/name).read_bytes()]
base_image=Image.open(a.control/'frame.png').convert('RGB')
candidate_image=Image.open(a.candidate/'frame.png').convert('RGB')
center_equal=False
if base_image.height==candidate_image.height==160 and min(base_image.width,candidate_image.width)>=240:
    def center(image):
        left=(image.width-240)//2
        return image.crop((left,0,left+240,160))
    center_equal=ImageChops.difference(center(base_image),center(candidate_image)).getbbox() is None
required={'ppu_frames','unmapped','dispatch_misses','interpreted_insns','healed_native','io_unhandled'}
passed=(required<=baseline.keys() and required<=candidate.keys() and baseline==candidate and
        bool(names) and names==candidate_names and not mismatches and center_equal and bus_base==bus_candidate and
        all(candidate[k]==0 for k in ('dispatch_misses','interpreted_insns','healed_native')))
report={'regression_comparison_passed':passed,'control':baseline,'candidate':candidate,
        'capture_count':len(names),'capture_mismatches':mismatches,'capture_names_equal':names==candidate_names,
        'control_image_size':base_image.size,'candidate_image_size':candidate_image.size,
        'native_center_pixel_equal':center_equal,'unmapped_warning_sets_equal':bus_base==bus_candidate,
        'unmapped_events':dict(bus_candidate),
        'scope':'Replay regression only. Does not establish Android performance, full-game safety or correctness of every margin object.'}
a.report.parent.mkdir(parents=True,exist_ok=True)
a.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(0 if passed else 1)
