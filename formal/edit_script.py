with open(r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\formal\lvFormal\Theory\Cv00Memory.lean', 'r') as f:
    content = f.read()

old = 'noncomputable def exec_stmt (m : Mem) (env : Env) (stmt : Cv00Stmt) : ExecResult :=\n  .normal m env'

new = 'noncomputable def exec_stmt (m : Mem) (env : Env) (stmt : Cv00Stmt) : ExecResult :=\n  match stmt with\n  | .nop => .normal m env\n  | .assign lhs rhs =>\n    match eval_expr env rhs with\n    | some v => .normal m (env_set env lhs v)\n    | none => .aborted "eval failed"\n  | _ => .normal m env'

content = content.replace(old, new)

with open(r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\formal\lvFormal\Theory\Cv00Memory.lean', 'w') as f:
    f.write(content)

print('Done')