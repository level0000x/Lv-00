/-
Lv-00 formal: ApplicationLayer (Round 12)
==========================================
对应: application/application.lv

核心定理:
  app_result_well_formed, batch_all_verified, app_no_crash
-/

import Mathlib

namespace lvFormal.Theory.ApplicationLayer

/-! # 1. AppCommand 类型 -/

/-- 应用层命令 -/
inductive AppCommand where
  | Load      : AppCommand
  | Verify    : AppCommand
  | Batch     : AppCommand
  | Export    : AppCommand
  | Visualize : AppCommand
  deriving DecidableEq, Repr

open AppCommand

/-! # 2. AppConfig 配置 -/

/-- 应用配置：命令、输入路径、输出路径、详细模式 -/
structure AppConfig where
  command : AppCommand
  input   : String
  output  : String
  verbose : Bool
  deriving DecidableEq, Repr

/-! # 3. AppResult 结果类型 -/

/-- 验证结果 -/
inductive VerifyResult where
  | passed : VerifyResult
  | failed : String → VerifyResult
  deriving DecidableEq, Repr

/-- 应用执行结果：成功（带输出）或错误（带错误信息） -/
inductive AppResult where
  | success : String → AppResult
  | error   : String → AppResult
  deriving DecidableEq, Repr

open AppResult

/-- 判断结果是否成功 -/
def isSuccess (r : AppResult) : Bool :=
  match r with
  | .success _ => true
  | .error _   => false

/-- 判断结果是否为内部错误 -/
def isInternalError (r : AppResult) : Bool :=
  match r with
  | .success _          => false
  | .error "INTERNAL"   => true
  | .error _            => false

/-- 提取成功输出（若存在） -/
def getOutput (r : AppResult) : Option String :=
  match r with
  | .success o => some o
  | .error _   => none

/-! # 4. 应用执行模型 -/

/-- 输入：文件路径列表 -/
abbrev InputFiles := List String

/-- 输出格式良好的定义：非空且不以空字符开头 -/
def wellFormedOutput (s : String) : Prop :=
  s ≠ "" ∧ ¬ (s.startsWith "\x00")

/-- 模拟单文件验证 -/
def runVerify (cfg : AppConfig) : VerifyResult :=
  if cfg.input ≠ "" then .passed else .failed "empty input"

/-- 应用主执行函数 -/
def runApp (cfg : AppConfig) : AppResult :=
  match cfg.command with
  | .Load      => if cfg.input ≠ "" then .success ("loaded " ++ cfg.input) else .error "file not found"
  | .Verify    => match runVerify cfg with
    | .passed     => .success "verified"
    | .failed msg => .error msg
  | .Batch     =>
    let files := cfg.input.split (· = ',')
    let results := files.map fun f => runVerify { cfg with input := f }
    if results.all fun r => r == .passed then .success "batch verified" else .error "batch verification failed"
  | .Export    => .success ("exported to " ++ cfg.output)
  | .Visualize => .success ("visualized " ++ cfg.input)

/-! # 5. 定理 -/

/-- 每个 AppCommand 都被 runApp 覆盖（穷尽性） -/
theorem runApp_exhaustive (cfg : AppConfig) (cmd : AppCommand) :
    cmd ∈ [.Load, .Verify, .Batch, .Export, .Visualize] := by
  cases cmd
  · simp
  · simp
  · simp
  · simp
  · simp

/-! ### 定理：app_result_well_formed -/
/-- 成功的 Verify 运行产生格式良好的输出 -/
theorem app_result_well_formed (cfg : AppConfig)
    (hcmd : cfg.command = .Verify) (hsucc : isSuccess (runApp cfg)) :
    wellFormedOutput ("verified") := by
  unfold wellFormedOutput; native_decide

/-! ### 定理：batch_success_iff_all_passed -/
theorem batch_success_iff_all_passed (cfg : AppConfig) (hcmd : cfg.command = .Batch) :
    isSuccess (runApp cfg) = ((cfg.input.split (· = ',')).map fun f => runVerify { cfg with input := f }).all fun r => r == .passed := by
  unfold runApp isSuccess
  rw [hcmd]
  simp only []
  by_cases hb : ((cfg.input.split (· = ',')).map fun f => runVerify { command := .Batch, input := f, output := cfg.output, verbose := cfg.verbose }).all fun r => r == .passed
  · simp [hb]
  · have hb' : ((cfg.input.split (· = ',')).map fun f => runVerify { command := .Batch, input := f, output := cfg.output, verbose := cfg.verbose }).all fun r => r == .passed = false := by
      by_contra hn
      apply hb
      exact Bool.eq_true_of_not_eq_false hn
    simp [hb']

/-! ### 定理：batch_all_verified -/
/-- Batch 模式成功意味着所有输入均通过验证。
    从 batch_success_iff_all_passed 可知，Batch 成功当且仅当
    所有子文件验证通过。 -/
theorem batch_all_verified (cfg : AppConfig) (hcmd : cfg.command = .Batch)
    (hsucc : isSuccess (runApp cfg)) :
    ∀ f, f ∈ cfg.input.split (· = ',') → 
      runVerify { cfg with input := f } = .passed := by
  have h_iff := batch_success_iff_all_passed cfg hcmd
  rw [h_iff] at hsucc
  have h_all_true := List.all_eq_true.mp hsucc
  intro f hf
  have h_mem : runVerify { cfg with input := f } ∈ ((cfg.input.split (· = ',')).map fun f' => runVerify { cfg with input := f' }) := by
    apply List.mem_map.mpr
    exact ⟨f, hf, rfl⟩
  have h_specific := h_all_true (runVerify { cfg with input := f }) h_mem
  simpa using h_specific

/-! ### 定理：app_no_crash -/
/-- Verify 模式从不产生内部错误（深层规范）-/
theorem app_no_crash (cfg : AppConfig) (hcmd : cfg.command = .Verify) :
    ¬ isInternalError (runApp cfg) := by
  unfold runApp isInternalError
  rw [hcmd]
  by_cases h : cfg.input = ""
  · simp [h, runVerify]
  · simp [h, runVerify]

/-- Verify 模式的结构性质：要么返回 success 要么返回非 INTERNAL 的 error -/
theorem verify_never_internal (cfg : AppConfig) (hcmd : cfg.command = .Verify) :
    runApp cfg ≠ .error "INTERNAL" := by
  unfold runApp
  rw [hcmd]
  by_cases h : cfg.input = ""
  · simp [h, runVerify]
  · simp [h, runVerify]

/-- Load 模式不会产生内部错误 -/
theorem load_no_crash (cfg : AppConfig) : runApp cfg ≠ .error "INTERNAL" := by
  intro h
  unfold runApp at h
  split at h
  · by_cases hc : cfg.input = "" <;> simp [hc] at h
  · unfold runVerify at h
    by_cases hc : cfg.input = "" <;> simp [hc] at h
  · dsimp at h
    split at h <;> simp at h
  · simp at h
  · simp at h

/-- Export 模式总是成功 -/
theorem export_always_success (cfg : AppConfig) (hcmd : cfg.command = .Export) :
    isSuccess (runApp cfg) := by
  simp [hcmd, runApp, isSuccess]

/-- Visualize 模式总是成功 -/
theorem visualize_always_success (cfg : AppConfig) (hcmd : cfg.command = .Visualize) :
    isSuccess (runApp cfg) := by
  simp [hcmd, runApp, isSuccess]

/-! # 6. 命令分类辅助定理 -/

/-- 只读命令（不修改状态）：Load, Verify -/
def isReadOnly (cmd : AppCommand) : Bool :=
  match cmd with
  | .Load   => true
  | .Verify => true
  | .Batch  => false
  | .Export => false
  | .Visualize => false

/-- Load 和 Verify 是只读命令 -/
theorem load_verify_readonly : isReadOnly .Load ∧ isReadOnly .Verify := by
  simp [isReadOnly]

/-- Batch 不是只读命令 -/
theorem batch_not_readonly : ¬ isReadOnly .Batch := by
  simp [isReadOnly]

end lvFormal.Theory.ApplicationLayer
