# SPDX-License-Identifier: GPL-2.0-only
# Preserve actual pending events; never synthesize a successful completion.
def ordered_pending(events):
 handoff=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_ordered_route_handoff']
 if handoff:return handoff
 rejected=[e for e in events if e.get('scope') in ('original_GnmSubmitFlip_first_GPU_draw','original_GnmSubmitFlip_MRT_GPU_draw','original_GnmSubmitFlip_vertex_only_GPU_draw') and e.get('invoked') and e.get('status')==88 and not e.get('GPU_execution')]
 unique={e['word']:e for e in rejected}
 return list(unique.values())
