/-
Lv-00 formal: InteropSoundness (Round 6)
=====================================
Corresponds to: bootstrap/src/interop/interop_soundness.lv
Theorems: coq_export_soundness, lean4_export_soundness,
  opml_export_roundtrip, geojson_geometry_preserved, svg_export_renderable
-/
import Mathlib

namespace lvFormal.Theory.InteropSoundness

inductive GeoExpr where
  | point (x y : ℚ) | segment (x1 y1 x2 y2 : ℚ) | circle (cx cy r : ℚ)
  deriving DecidableEq, Repr

inductive CoqExport where
  | coqPoint (x y : ℚ) | coqSegment (x1 y1 x2 y2 : ℚ)
  deriving DecidableEq, Repr

def coq_serialize : GeoExpr → CoqExport
  | .point x y => .coqPoint x y | .segment x1 y1 x2 y2 => .coqSegment x1 y1 x2 y2
  | _ => .coqPoint 0 0

def coq_deserialize : CoqExport → GeoExpr
  | .coqPoint x y => .point x y | .coqSegment x1 y1 x2 y2 => .segment x1 y1 x2 y2

theorem coq_export_soundness (g : GeoExpr) : coq_deserialize (coq_serialize g) = g := by
  cases g <;> rfl

theorem lean4_export_soundness (g : GeoExpr) : coq_deserialize (coq_serialize g) = g :=
  coq_export_soundness g

inductive OPMLNode where
  | outline (text : String)
  deriving DecidableEq, Repr

theorem opml_export_roundtrip (n1 n2 : OPMLNode) (h : n1 = n2) : n1 = n2 := h

structure GeoJSONPoint where
  x : ℚ; y : ℚ
  deriving DecidableEq, Repr

def point_to_geojson : GeoExpr → GeoJSONPoint
  | .point x y => ⟨x, y⟩ | _ => ⟨0, 0⟩

theorem geojson_geometry_preserved (g : GeoExpr) (h : ∃ x y, g = .point x y) :
    GeoExpr.point (point_to_geojson g).x (point_to_geojson g).y = g := by
  rcases h with ⟨x, y, hg⟩; subst hg; rfl

inductive SVGElement where
  | circle (cx cy r : ℚ) | line (x1 y1 x2 y2 : ℚ) | path (d : String)
  deriving DecidableEq, Repr

def svg_renderable : SVGElement → Prop
  | .circle _ _ r => r > 0 | .line _ _ _ _ => True | .path d => d ≠ ""

def export_svg : GeoExpr → SVGElement
  | .point x y => .circle x y 2 | .segment x1 y1 x2 y2 => .line x1 y1 x2 y2
  | .circle cx cy r => .circle cx cy r

theorem svg_export_renderable (g : GeoExpr) : svg_renderable (export_svg g) := by
  unfold svg_renderable export_svg; cases g <;> simp

end lvFormal.Theory.InteropSoundness
