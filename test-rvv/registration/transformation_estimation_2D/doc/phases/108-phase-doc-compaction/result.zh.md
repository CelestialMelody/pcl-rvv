# Phase 108 Result: phase-doc-compaction

## 结论

本阶段完成提交前 phase 文档整理。生产源码和 Phase 107 生产结论未改变：ordered generic、
source-indexed exact 和 dual-indexed exact 仍是当前有收益 production patch；source-indexed
generic widening 仍为 guarded / negative，不采纳；correspondence direct RVV dispatch 仍已退回。

## 文档整理事实

| area | result |
| --- | --- |
| phase README | `doc/phases/README.zh.md` 已压缩为当前恢复入口、阅读路径、提交边界和停止规则。 |
| historical phase index | 新增 `doc/phases/history.zh.md`，按阶段组压缩 Phase 000-107，并保留关键决策锚点。 |
| evidence_status doc inputs | Makefile 的 `evidence_status` 已从逐个 phase plan/result 扫描改为主文档、历史索引、optimization matrix 和关键 result 扫描。 |
| production truth | 未改变 `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp`。 |

## Evidence registry 处理

第一次瘦身后，`evidence_status` 暴露历史 evidence path 缺少文档锚点。已在
`doc/phases/history.zh.md` 中补充 registry 历史锚点，包括 aggregate correctness logs、
row-source fused historical board、source-indexed generic guarded public probe、Phase 104
PointNormal 256K rerun 的 QEMU / board summary、manifest 和 Evidence Doctor 路径。

再次运行：

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

结果：`evidence registry check: fresh`。

## Doc-suite 审计

| area | current shape scan | decision | evidence / next action |
| --- | --- | --- | --- |
| README navigation | 原 README 串联了 Phase 070-107 的长段历史，阅读负担高。 | adopted | 已改为短入口，历史移到 `history.zh.md`。 |
| phase history | 历史 phase 数量多，但包含采纳、guarded、negative 和 rollback 事实。 | adopted | 用 `history.zh.md` 压缩导航，不删除事实。 |
| evidence registry | registry 需要已登记 evidence file 在提交后文档入口中可定位。 | adopted | Makefile doc input 列表已瘦身，`history.zh.md` 补历史锚点，`evidence_status` fresh。 |
| production docs | `doc-rvv` 已记录当前 production patch 和未采纳方向。 | adopted | 本阶段未改变生产结论。 |
| artifact tracking | 新增 `history.zh.md` 和 Phase 108 plan/result 需要纳入提交候选。 | adopted | 提交前用路径限定 `git status --short --untracked-files=all` 检查。 |

## 验证

已完成：

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
git diff --check -- registration/include/pcl/registration/impl/transformation_estimation_2D.hpp test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D doc-rvv/registration/transformation_estimation_2D-RVV.zh.md
```

结果：`evidence_status` fresh；`git diff --check` pass。

## 提交准备建议

当前可以进入 topic-only commit preparation。建议提交当前生产 patch、测试资产、长期 `doc-rvv`、
topic-local doc suite、Phase 106/107/108 关键文档、`history.zh.md` 和
`optimization-matrix.zh.md`。

本次不提交 `log/**`、raw board logs、本地 build 输出、私有 `config.mk`、未被文档引用的临时日志
或其它 topic 脏改动；这些证据继续留在本机工作区，供后续复核和重新登记使用。
