"""Read-only evidence for the original Fuzzy effect and its natural recovery."""
import struct

PROFILE = {
    "name": "sma3-usa-fuzzy-native-effect-v1",
    "iwram_size": 0x8000,
    "gameplay_base_pointer": "03007240",
    "timer_offset": "29CA",
    "amplitude_offset": "28CA",
    "active_address": "03007294",
    "allocation_pointers": ["03007264", "03007260"],
    "callback_pointers": ["0300736C", "03007370"],
}


def parse_fuzzy_status(iwram):
    if len(iwram) != 0x8000:
        raise ValueError("Fuzzy observation requires complete IWRAM")
    base = struct.unpack_from("<I", iwram, 0x7240)[0]
    offset = base - 0x03000000
    initialized = 0 <= offset <= len(iwram) - 0x29CC
    return {
        "gameplay_base": base,
        "timer": struct.unpack_from("<H", iwram, offset + 0x29CA)[0] if initialized else None,
        "amplitude": struct.unpack_from("<H", iwram, offset + 0x28CA)[0] if initialized else None,
        "effect_active": struct.unpack_from("<I", iwram, 0x7294)[0],
        "allocations": [struct.unpack_from("<I", iwram, x)[0] for x in (0x7264, 0x7260)],
        "callbacks": [struct.unpack_from("<I", iwram, x)[0] for x in (0x736C, 0x7370)],
    }


def transition_changed(previous, current):
    before, after = previous["fuzzy_status"], current["fuzzy_status"]
    return ((before["timer"] == 0) != (after["timer"] == 0) or
            before["effect_active"] != after["effect_active"])


def check_recovery(trace, normal_tail=100):
    if type(normal_tail) is not int or normal_tail < 1:
        raise ValueError("A positive normal-tail frame count is required")
    if len(trace) < normal_tail + 2:
        raise ValueError("Fuzzy recovery trace is too short")
    for index, row in enumerate(trace):
        if type(row["frame"]) is not int or row["elapsed_frame"] != index:
            raise ValueError("Fuzzy trace elapsed-frame reset or gap")
        effect = row["fuzzy_status"]
        if any(type(effect[field]) is not int or not 0 <= effect[field] <= 0xFFFF
               for field in ("timer", "amplitude")) or effect["effect_active"] not in (0, 1):
            raise ValueError("Invalid Fuzzy effect observation")
    for previous, current in zip(trace, trace[1:]):
        if current["frame"] != previous["frame"] + 1:
            raise ValueError("Fuzzy trace frame reset or gap")
    active = [row for row in trace if row["fuzzy_status"]["effect_active"] == 1]
    if not active or max(row["fuzzy_status"]["amplitude"] or 0 for row in active) <= 0:
        raise ValueError("The original visible Fuzzy effect was not observed")
    if not any((row["fuzzy_status"]["timer"] or 0) > 1 for row in active):
        raise ValueError("No native Fuzzy contact countdown observed")
    first = active[0]["elapsed_frame"]
    if any(trace[0]["fuzzy_status"][field] != 0
           for field in ("timer", "amplitude", "effect_active")):
        raise ValueError("Trace must start before contact")
    for row in trace[first:]:
        if row["game_state"] != 13 or row["level_id"] != 6 or row["sublevel_id"] != 6:
            raise ValueError("Gameplay left level 1-7 or entered a death/restart state")
    tail = trace[-normal_tail:]
    if any(row["fuzzy_status"][field] != 0
           for row in tail for field in ("timer", "amplitude", "effect_active")):
        raise ValueError("The last 100 frames must be normal after natural recovery")
    recovered = next((row for row in trace[first:]
                      if row["fuzzy_status"]["timer"] == 0 and
                      row["fuzzy_status"]["effect_active"] == 0 and
                      row["fuzzy_status"]["amplitude"] == 0), None)
    if recovered is None:
        raise ValueError("Natural recovery was not observed")
    # Merely seeing a nonzero timer followed by zeros could also describe a
    # forced cancellation. Require the original countdown, then its phase-1
    # amplitude fade (Sub0810A9DC: minus 0x100 per update) before normal play.
    countdown_steps = growth_steps = fade_steps = 0
    for previous, current in zip(trace[first:], trace[first + 1:]):
        before, after = previous["fuzzy_status"], current["fuzzy_status"]
        if before["effect_active"] != 1:
            continue
        old_timer, new_timer = before["timer"], after["timer"]
        old_amp, new_amp = before["amplitude"], after["amplitude"]
        if old_timer > 1:
            if new_timer < old_timer - 1:
                raise ValueError("Fuzzy countdown skipped frames or was forcibly cleared")
            countdown_steps += new_timer == old_timer - 1
        if after["effect_active"] == 1 and new_amp > old_amp:
            growth_steps += 1
        if old_timer == 1 and new_timer <= 1:
            expected = max(0, old_amp - 0x100)
            if new_amp != old_amp and new_amp != expected:
                raise ValueError("Fuzzy amplitude did not recover through its original gradual fade")
            if new_amp < old_amp:
                fade_steps += 1
        if after["effect_active"] == 0:
            if old_timer != 1 or not 0 < old_amp <= 0x100 or new_timer != 0 or new_amp != 0:
                raise ValueError("Fuzzy effect was cleared before its natural recovery")
    if countdown_steps < 2 or growth_steps < 1 or fade_steps < 2:
        raise ValueError("Fuzzy countdown, amplitude growth and gradual recovery were not fully observed")
    normal_end = 0
    for row in reversed(trace):
        if any(row["fuzzy_status"][field] != 0 for field in ("timer", "amplitude", "effect_active")):
            break
        normal_end += 1
    return {
        "pass": True,
        "first_effect_elapsed_frame": first,
        "first_effect_guest_frame": active[0]["frame"],
        "recovery_elapsed_frame": recovered["elapsed_frame"],
        "recovery_guest_frame": recovered["frame"],
        "active_frames": len(active),
        "maximum_amplitude": max(row["fuzzy_status"]["amplitude"] for row in active),
        "normal_frames_at_end": normal_end,
        "required_normal_tail_frames": normal_tail,
        "observed_countdown_steps": countdown_steps,
        "observed_amplitude_growth_steps": growth_steps,
        "observed_amplitude_fade_steps": fade_steps,
        "no_restart_or_death": True,
        "effect_was_not_disabled": True,
    }
