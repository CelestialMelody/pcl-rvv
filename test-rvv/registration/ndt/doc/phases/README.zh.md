# NDT Phase Index

## 当前恢复入口

当前默认恢复动作：若继续优化，进入 `030-fused-formula-or-reduction-staging-probe`。phase 020 已证明 topic-local double `exp` RVV 原型没有改变 staged candidate 的负向 decision bucket，因此下一步只建议尝试减少 staged buffer / reduction 次数；不改 production。

## 阶段表

| phase | 目标 | 状态 | 主要证据 | 下一步 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | 建立 NDT 函数级评估、diagnostic scaffold、QEMU/asm/board evidence | complete | QEMU correctness pass；board repeated negative；Evidence Doctor board Errors=2 | 用户确认是否不接入 production |
| `010-public-entry-profile-or-closeout-confirmation` | 用真实 `NormalDistributionsTransform::align` 公开入口确认是否还有 RVV 搜索价值 | complete | board public repeated neutral；gprof 显示 `updateDerivatives` / `updateHessian` 是主热点 | 下一步评估 double `exp` helper 或 fused formula |
| `020-double-exp-helper-or-fused-formula-probe` | 用 topic-local double `exp` RVV 原型消融标量 `exp` 成本 | complete | QEMU correctness pass；board repeated hessian 0.321x、gradient 0.405x；Evidence Doctor Errors=2 | 不建议继续公共 double exp helper；若继续，转 fused formula / reduction staging |

## 文档归属

plan/result 保留阶段计划和事实；`optimization-matrix.zh.md` 记录候选状态；`../optimization-roadmap.zh.md` 记录后续搜索空间；`../ndt-evaluation.zh.md` 是诊断决策主归属。
