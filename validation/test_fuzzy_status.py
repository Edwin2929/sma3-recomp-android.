"""Adversarial evidence checks for the Fuzzy contact/recovery qualification."""
import copy
import struct
import unittest

from fuzzy_status import check_recovery, parse_fuzzy_status


def evidence():
    # Short native-shaped cycle: contact is followed by a countdown, visible
    # growth, phase-1 gradual fade and at least 100 normal frames. This is
    # synthetic validator evidence, not a claim that the game uses a timer of 5.
    phases = [(0, 0, 0), (5, 0, 0), (4, 128, 1), (3, 256, 1),
              (2, 384, 1), (1, 512, 1), (1, 256, 1)] + [(0, 0, 0)] * 101
    return [{"frame": 1000 + index, "elapsed_frame": index,
             "game_state": 13, "level_id": 6, "sublevel_id": 6,
             "fuzzy_status": {"timer": timer, "amplitude": amplitude,
                              "effect_active": active}}
            for index, (timer, amplitude, active) in enumerate(phases)]


class FuzzyRecoveryEvidenceTests(unittest.TestCase):
    def test_complete_observed_cycle_passes(self):
        result = check_recovery(evidence())
        self.assertTrue(result["pass"])
        self.assertEqual(result["recovery_elapsed_frame"], 7)
        self.assertEqual(result["normal_frames_at_end"], 101)

    def test_all_normal_trace_does_not_prove_contact(self):
        trace = evidence()
        for row in trace:
            row["fuzzy_status"] = {"timer": 0, "amplitude": 0, "effect_active": 0}
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_countdown_without_visible_effect_does_not_pass(self):
        trace = evidence()
        for row in trace:
            row["fuzzy_status"]["amplitude"] = 0
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_trace_starting_after_contact_is_rejected(self):
        trace = evidence()
        trace[0]["fuzzy_status"] = copy.deepcopy(trace[2]["fuzzy_status"])
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_guest_frame_gap_is_rejected(self):
        trace = evidence()
        trace[4]["frame"] += 1
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_guest_frame_reset_is_rejected(self):
        trace = evidence()
        for row in trace[7:]:
            row["frame"] -= 1000
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_elapsed_counter_gap_is_rejected(self):
        trace = evidence()
        trace[4]["elapsed_frame"] += 1
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_countdown_jump_is_rejected(self):
        trace = evidence()
        trace[3]["fuzzy_status"]["timer"] = 1
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_early_termination_during_mareo_is_rejected(self):
        with self.assertRaises(ValueError):
            check_recovery(evidence()[:6])

    def test_insufficient_normal_tail_is_rejected(self):
        with self.assertRaises(ValueError):
            check_recovery(evidence()[:-2])

    def test_forced_zero_after_contact_is_rejected(self):
        trace = evidence()
        for row in trace[4:]:
            row["fuzzy_status"] = {"timer": 0, "amplitude": 0, "effect_active": 0}
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_forced_zero_skipping_gradual_fade_is_rejected(self):
        trace = evidence()
        trace[6]["fuzzy_status"] = {"timer": 0, "amplitude": 0, "effect_active": 0}
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_death_during_effect_is_rejected(self):
        trace = evidence()
        trace[4]["game_state"] = 15
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_restart_scene_after_recovery_is_rejected(self):
        trace = evidence()
        trace[12]["game_state"] = 10
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_level_exit_cannot_substitute_for_recovery(self):
        trace = evidence()
        trace[8]["sublevel_id"] = 7
        with self.assertRaises(ValueError):
            check_recovery(trace)

    def test_missing_amplitude_observation_is_rejected(self):
        trace = evidence()
        trace[4]["fuzzy_status"]["amplitude"] = None
        with self.assertRaises(ValueError):
            check_recovery(trace)


class FuzzyMemoryObservationTests(unittest.TestCase):
    def test_truncated_snapshot_is_rejected(self):
        with self.assertRaises(ValueError):
            parse_fuzzy_status(bytes(0x7FFF))

    def test_invalid_gameplay_pointer_does_not_read_unrelated_memory(self):
        memory = bytearray(0x8000)
        struct.pack_into("<I", memory, 0x7240, 0x03007FFF)
        result = parse_fuzzy_status(memory)
        self.assertIsNone(result["timer"])
        self.assertIsNone(result["amplitude"])


if __name__ == "__main__":
    unittest.main()
