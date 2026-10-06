#!/usr/bin/env python3
"""Opt-in generated-code patch for the pinned SMA3 USA small-object filter.

Public script only: generated game sources remain private. Defaults are original
constants; desktop diagnostics alone may change the exported interval.
"""
import hashlib
from pathlib import Path

root=Path(__file__).resolve().parents[1]
path=root/'generated-project/generated/recompiled_003.cpp'
original_hash='e3e2f9c32ccb61071329e94a649c7ddf8bb3b81f715112208a078bf02b90f055'
prefix='\nextern "C" uint32_t sma3_object_x_bias;\nextern "C" uint32_t sma3_object_x_limit;\n'
pairs=[
 ('_rn_0804CE30 + 0x00000010u','_rn_0804CE30 + sma3_object_x_bias'),
 ('arm_set_nzcv_add(_rn_0804CE30, 0x00000010u,','arm_set_nzcv_add(_rn_0804CE30, sma3_object_x_bias,'),
 ('_rn_0804CE36 - 0x000000FFu','_rn_0804CE36 - sma3_object_x_limit'),
 ('arm_set_nzcv_sub(_rn_0804CE36, 0x000000FFu,','arm_set_nzcv_sub(_rn_0804CE36, sma3_object_x_limit,'),
]
data=path.read_text()
baseline=data
if prefix in baseline:
    baseline=baseline.replace(prefix,'',1)
    for old,new in pairs:
        if baseline.count(new)!=1: raise SystemExit('Unexpected patched shard')
        baseline=baseline.replace(new,old,1)
if hashlib.sha256(baseline.encode()).hexdigest()!=original_hash:
    raise SystemExit('Unrecognized generated shard; refusing changes')
patched=baseline.replace('#include "recompiled.h"','#include "recompiled.h"'+prefix,1)
for old,new in pairs:
    if patched.count(old)!=1: raise SystemExit('Unexpected instruction count')
    patched=patched.replace(old,new,1)
if patched!=data: path.write_text(patched)
print('Verified small-object bounds patch; rebuild the generated archive')
