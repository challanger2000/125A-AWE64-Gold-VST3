import importlib.util
import sys
from pathlib import Path
import unittest
P=Path(__file__).resolve().parent.parent/"tools/p002_verify_velocity_candidate.py"
s=importlib.util.spec_from_file_location("p002_velocity_verify",P)
m=importlib.util.module_from_spec(s)
s.loader.exec_module(m)

class CandidateIndependentControls(unittest.TestCase):
    def fake_levels(self):
        # Numeric test-only table; never present fabricated values as original.
        return [0]+[49-(v*49//127) for v in range(1,128)]
    def test_actual_cplusplus_candidate_has_128_levels(self):
        source=P.parent.parent/"src/creative_compat/CreativeCandidateRegister.cpp"
        values=m.source_levels(source)
        self.assertEqual(len(values),128)
        self.assertEqual((values[1],values[127]),(49,0))
    def test_original_cutoff_parametric_prediction_at_multiple_velocities(self):
        values=m.source_levels(P.parent.parent/"src/creative_compat/CreativeCandidateRegister.cpp")
        self.assertEqual(m.predict(70,values,13,210,True)[0],116)
        self.assertEqual(m.predict(71,values,13,210,True)[0],117)
        self.assertEqual(m.predict(100,values,13,210,True)[0],165)
        self.assertEqual(m.predict(127,values,13,210,True)[0],209)
    def test_zero_velocity_is_noteoff_not_voice(self):
        with self.assertRaisesRegex(ValueError,"Not a NoteOn"):
            m.predict(0,self.fake_levels(),16,178,False)
    def test_reference_missing_velocities_is_no_go(self):
        levels=m.source_levels(P.parent.parent/"src/creative_compat/CreativeCandidateRegister.cpp")
        rows={
            0:{v:{"attenuation":m.predict(v,levels,16,178,False)[1],
                   "cutoff":178} for v in range(1,128)},
            40:{v:{"attenuation":m.predict(v,levels,13,210,True)[1],
                    "cutoff":m.predict(v,levels,13,210,True)[0]}
                for v in range(1,128)}
        }
        # Note: synthetic expected rows, only negative-control machinery.
        self.assertEqual(m.verify_rows(rows,levels)["original_rows"],254)
        rows[40][71]["cutoff"]+=1
        with self.assertRaisesRegex(ValueError,"velocity=71"):
            m.verify_rows(rows,levels)
    def test_wrong_attenuation_on_other_program_is_rejected(self):
        levels=m.source_levels(P.parent.parent/"src/creative_compat/CreativeCandidateRegister.cpp")
        rows={
            0:{v:{"attenuation":m.predict(v,levels,16,178,False)[1],
                   "cutoff":178} for v in range(1,128)},
            40:{v:{"attenuation":m.predict(v,levels,13,210,True)[1],
                    "cutoff":m.predict(v,levels,13,210,True)[0]}
                for v in range(1,128)}
        }
        rows[40][100]["attenuation"]+=1
        with self.assertRaisesRegex(ValueError,"velocity=100"):
            m.verify_rows(rows,levels)

if __name__=="__main__":unittest.main()
