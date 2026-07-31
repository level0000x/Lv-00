import Mathlib

inductive Ty where | int | real | bool | arrow (d c : Ty) | list (e : Ty) | option (e : Ty) | pair (a b : Ty)

inductive Ex where
  | var (n : String)
  | intLit (v : Int)
  | floatLit (v : Float)
  | boolLit (v : Bool)
  | add (e1 e2 : Ex)
  | lambda (p : String) (t : Ty) (b : Ex)
  | listLit (es : List Ex)
  | some (e : Ex)
  | none (t : Ty)
  | app (f a : Ex)
  | pair (e1 e2 : Ex)

def infer : Ex → Option Ty
  | .var _ => none
  | .intLit _ => some .int
  | .floatLit _ => some .real
  | .boolLit _ => some .bool
  | .add e1 e2 =>
    match infer e1, infer e2 with
    | some .int, some .int => some .int
    | some .real, some .real => some .real
    | _, _ => none
  | .lambda _p t b => some (Ty.arrow t ((infer b).getD Ty.real))
  | .listLit es =>
    match es with
    | [] => none
    | e :: _ => (infer e).map .list
  | .some e => (infer e).map .option
  | .none t => some (.option t)
  | .pair e1 e2 =>
    match infer e1, infer e2 with
    | some t1, some t2 => some (.pair t1 t2)
    | _, _ => none
  | .app f _a =>
    match infer f with
    | some (.arrow _ codom) => some codom
    | _ => none

def check : Ex → Ty → Bool
  | .var _, _ => true
  | .intLit _, .int => true
  | .intLit _, .real => true
  | .floatLit _, .real => true
  | .boolLit _, .bool => true
  | .add e1 e2, .real => check e1 .real && check e2 .real
  | .add e1 e2, .int => check e1 .int && check e2 .int
  | .lambda _p t b, .arrow dom codom => t = dom && check b codom
  | .lambda _ _ _, _ => false
  | .listLit es, .list t => es.all (fun e => check e t)
  | .some e, .option t => check e t
  | .none t', .option t => t' = t
  | .pair e1 e2, .pair t1 t2 => check e1 t1 && check e2 t2
  | .app f a, t =>
    match infer f with
    | some (.arrow dom codom) => check a dom && codom = t
    | _ => false
  | _, _ => false
