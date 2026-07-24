import lvFormal.Theory.GeometryPresetDefs
open lvFormal.Theory.GeometryPresetDefs

-- Test with 4 specific points: a square
def P0 : Pt := { x := 0, y := 0 }
def P1 : Pt := { x := 1, y := 0 }
def P2 : Pt := { x := 1, y := 1 }
def P3 : Pt := { x := 0, y := 1 }

-- Verify shoelace_triangulate manually
#eval shoelace_sum (P0 :: P1 :: P2 :: [P3])
#eval shoelace_sum [P0, P1, P2] + shoelace_sum (P0 :: P2 :: [P3])

-- These should be equal: -2 = (-1) + (-1) = -2
#eval shoelace_sum (P0 :: P1 :: P2 :: [P3]) = shoelace_sum [P0, P1, P2] + shoelace_sum (P0 :: P2 :: [P3])
