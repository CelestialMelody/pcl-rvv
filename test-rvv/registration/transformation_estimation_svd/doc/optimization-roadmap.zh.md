# transformation_estimation_svd 优化路线图

## 当前边界

当前 production 已覆盖四条 row source policy（行来源策略）：

- `ordered-cloud-pair`
- `source-indexed-cloud-pair`
- `dual-indices-cloud-pair`
- `correspondence-pair`

四条生产路径都只覆盖 `Scalar=float`、`use_umeyama_ == true`、dense xyz AoS layout-gated 点型和 `n >= 16`。`full-cloud` 仅保留为历史日志 / case-filter 兼容别名，不再作为规范名称。

## 候选搜索空间

| candidate family | idea source | 适用范围 | 预期收益 | 风险 / 未知 | 所需证据 | 状态 | next action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `fused_ordered_cloud_pair_accum` | 当前源码默认 Umeyama 动态矩阵装填 | dense ordered-cloud-pair `float` xyz AoS | 减少 3 x N matrix allocation / fill，降低前端 memory traffic | 加法树差异、SVD 符号修正、收益被 Eigen SVD 稀释 | correctness、asm、board repeated、doctor、production direct | adopted through production_direct_dispatch_ordered；legacy alias：`fused_full_cloud_accum` | none |
| `source_indexed_fused_accum` | registration row source carry-over audit | source indices + target sequential | 用 gather 读取 source，同时避免 public iterator 动态矩阵装填 | gather locality、index 合法性、32-bit byte offset gate | correctness、asm、board repeated、production direct | adopted through production_direct_dispatch_source_indexed | none |
| `dual_indices_fused_accum` | remaining row source audit | source indices + target indices | 双 gather 仍能保持正向收益 | 两侧合法性检查和 cache locality | correctness、asm、board repeated、production direct | adopted through production_direct_dispatch_dual_indices | none |
| `correspondence_fused_accum` | remaining row source audit | correspondence query/match | query/match row source 仍能保持正向收益 | query/match 语义、边界检查、访问局部性和容器成本 | correctness、asm、board repeated、production direct | adopted through production_direct_dispatch_correspondence | none |
| `generic_xyz_aos_gate` | mixed-field correctness 审计 | `PointXYZI` / `PointXYZRGB` / 其它 xyz AoS | 证明布局门控不是 exact `PointXYZ` 窄门 | 代表性性能不等于逐类型性能 | correctness、representative board、direct helper path-hit | correctness adopted / performance bounded | none |
| `Scalar=double` | user boundary / source fallback | all public overloads | none planned | double RVV 需要独立数值预算和收益证据，当前收益不明确 | explicit user request + new phase | rejected / fallback | none |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 060 | layout-gated generic xyz AoS correctness 已收口 | `PointXYZI` / `PointXYZRGB` 已完成 production-direct path-hit | no new board work unless要声明逐类型性能 | closed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `Scalar=double` | 当前生产 patch 明确回退到标量路径；没有 double RVV 性能预算。 | 用户另开 double 数值 / 性能计划。 |
| 逐类型 board performance for `PointXYZI` / `PointXYZRGB` | 当前只承诺代表性 performance，不承诺逐类型板卡性能。 | 用户明确要求按点型单独上板时再开。 |

## roadmap_default_recovery_queue

| order | phase | scope | status | blocker / stop condition | merge option | next action |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `000-current-state-and-gaps` | diagnostic scaffold + correctness | done | none | not needed | result 已回填 |
| 2 | `010-board-and-asm-diagnostic` | ordered-cloud-pair board repeated + manifest / doctor | done | none；doctor Warnings 已降级解释 | not needed | result 已回填 |
| 3 | `020-pi1-production-integration-plan` | ordered-cloud-pair production direct plan / patch / evidence | done / production-ready | none | 不与 row-source 诊断合并 | result 已回填 |
| 4 | `030-row-source-audit` | source-indexed candidate / board diagnostic | done | none；diagnostic warning 已解释 | 独立 phase | result 已回填 |
| 5 | `040-source-indexed-production-integration` | source-indexed production direct patch / evidence | done / production-ready | none | 不与 dual/correspondence 合并 | result 已回填 |
| 6 | `050-dual-indices-correspondences-audit` | dual-indices + correspondence candidate family | done | none；diagnostic positive 已进入 060 | 可拆成两个 phase | result 已回填 |
| 7 | `060-dual-indices-correspondences-production-integration` | dual-indices + correspondence production direct | done / production-ready | none | 不再拆分 | result 已回填 |

## 停止规则

`ready_for_review` 现在对四条 row source 的 production 范围都成立。若继续当前 topic，默认只剩文档 closeout，不再存在授权且未阻塞的 row-source 优化动作。任何一个已采用 row source 的 positive summary 都不能外推到其它 row source、逐点型性能或 `Scalar=double`。
