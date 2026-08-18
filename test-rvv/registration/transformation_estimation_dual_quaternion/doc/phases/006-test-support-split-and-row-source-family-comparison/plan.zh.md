# Phase 006 Plan：test-support-split-and-row-source-family-comparison

## 阶段意图和边界

本阶段继续 Phase 005 的 row source 诊断，但先闭合测试支撑结构成熟度缺口，再做
source-indexed implementation-family comparison（实现族比较）。范围只限
`test-rvv/registration/transformation_estimation_dual_quaternion` 的测试支撑、bench、摘要和
topic-local 文档；不修改
`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`，
不重开 production integration loop（生产接入闭环）。

第一部分把夹具和 row-source 展开辅助从超长的 `tedq_candidates.hpp` 拆到
`include/impl/tedq_adapters.hpp`，保持 `include/tedq.h` 作为稳定聚合入口，保持测试名、bench
case label、计时边界和证据字段不变。第二部分只对 source-indexed-cloud-pair 评估 direct
indexed gather（直接离散加载）与当前 staged ordered reuse（先展开并暂存为顺序点云）的同边界
候选；dual-indexed 和 correspondence 先保留既有 staged 诊断，避免把 source-indexed 结果外推。

## 当前事实和结构门禁

| 项目 | 当前状态 | 证据 / 风险 |
| --- | --- | --- |
| Phase 005 row-source staged reuse | 已完成 | source-indexed `positive`；dual-indexed / correspondence `weak_positive`；三份 doctor 均 0/0/0。 |
| `tedq_candidates.hpp` | 858 行 | 超过 `test_support.helper_split_soft_line_limit=800`；同时混合 fixtures、adapters、reference、candidates、assertions。 |
| 聚合入口 | 已存在 | `include/tedq.h` 保持不变；内部头按 `include/impl` 配置落位。 |
| production 源码 | unchanged | 本阶段禁止修改 production TEDQ 头文件。 |
| board 环境 | 已可用 | 若 direct gather correctness 和 smoke 通过，使用已有 5-run / 20 iterations / 5 warm-up 预算，不无限复跑。 |

## 经验迁移审计

| 经验维度 | 当前采用方式 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- |
| row source | 每个 policy 独立 adapter；不把 source-indexed 结果外推到双索引或 correspondence | adopted | Phase 005 三份独立 summary / manifest / doctor | source-indexed 做 family comparison |
| source / index policy | staged candidate 把索引展开计入计时；direct candidate 使用同一索引数组读取 source xyz | attempted | 需要同一 row pairing、相同 solve 和 checksum 边界 | correctness 后再板卡 |
| shared math pipeline | direct gather 和 staged reuse 共用 `DualQuaternionAccumulation` 与 Eigen 后段 | adopted | 只改变 row-source ingress，不改变 C1/C2 公式和 solve | 对拍 accumulation / matrix |
| staging / reduction | 保留 staged ordered reuse；direct gather 不引入额外 staging buffer，仍使用现有向量规约 | attempted | 目标是分离 index 展开与 gather 成本，规约误差预算保持不变 | asm + board |
| formula / FMA | 不改变现有 widen-to-f64 reduction 公式 | adopted | 避免把 family comparison 与公式变化混合 | 复用现有误差预算 |
| evidence model | Std/RVV correctness、QEMU path/log shape、board repeated、doctor、registry 分层 | adopted | 与 Phase 005 证据合同一致 | 记录 direct family 的独立 summary |
| production boundary | 不修改 public dispatch，不创建 `doc-rvv` | adopted | 当前仍是未接 production 的诊断候选 | 需要用户 / reviewer 明确 PI1 才能扩大 |

## 动作和完成判据

| action | 内容 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| A1 shape-preserving split | 新增 `include/impl/tedq_adapters.hpp`，迁移 fixtures、index/correspondence range check、materialize 和测试输入变换辅助 | `tedq_adapters.hpp`、`tedq_candidates.hpp` | `tedq_candidates.hpp` 降到 800 行以内或有明确剩余职责说明；`include/tedq.h` 不变。 |
| A2 rebuild parity | 清理并重新构建 Std/RVV test 与 bench，确保 header-only split 没有复用旧二进制 | Makefile build / run targets | Std/RVV correctness 仍通过，case label 和 checksum 合同不变。 |
| A3 direct source-indexed candidate | 在 test-rvv 内增加 source-indexed direct gather candidate；索引合法性、target 顺序和 staging candidate 保持同一输入 | `include/impl/tedq_candidates.hpp` 或新的候选内部头 | 非 RVV 构建回退到标量；RVV 构建能记录 direct-gather path；matrix 输出与 staged scalar 对拍通过。 |
| A4 dedicated family bench | 增加 source-indexed family comparison case-filter / target，计时边界同时覆盖 index ingress、公式、规约和 solve | `src/bench_tedq.cpp`、`Makefile`、topic-local docs | 只生成板卡性能结论；QEMU 只做窄 smoke / 日志形状。 |
| A5 board evidence | direct gather 与 staged reuse 使用已有 bounded 5-run、20 iterations、5 warm-up 预算；生成 summary、manifest、doctor、registry | `log/board/` | checksum 一致，Evidence Doctor 无 Error；方向稳定才保留 family 结论。 |
| A6 docs / matrix | 更新 phase result、README、testing overview、bench/evidence、optimization evidence、evaluation、roadmap 和 matrix | topic-local docs | 每个 row source policy 和实现族比较状态可独立恢复。 |

## 数值和性能合同

- 标量 reference 仍来自 test-only 对 production C1/C2 累加的复刻；direct gather 只改变输入行的
  获取方式，不改变公式、double 累加和 Eigen 4x4 solve。
- RVV reduction（向量规约）顺序可能不同，继续使用现有矩阵误差预算和分桶 checksum；不能用
  raw double bitwise checksum 把正常舍入差异写成 correctness failure。
- `B/A = staged_rvv_ms / direct_gather_rvv_ms`，大于 1 表示 direct gather 更快。它是
  test-rvv 内的 RVV-vs-RVV family comparison，不是 production performance evidence。
- QEMU 只证明构建、路径和日志形状。若 board summary 出现 Error、方向摇摆或 checksum mismatch，
  先修复或降级证据，不进入 production 结论。

## 继续 / 停止条件

完成 A1 后必须继续 A2。若 direct gather correctness 通过且板卡可用，继续 A4-A5；若 direct
gather 在当前公共 load wrapper 或目标工具链上无法安全表达，保留 staged candidate，记录具体
工具 / 语义阻塞和恢复命令，不修改 production。只有用户明确要求扩大到 production、板卡不可用、
Evidence Doctor Error 无法修复或 dirty isolation 不安全时，才允许本阶段停止。

本阶段结束时，即使 family comparison 为 neutral / negative，也不能删除 Phase 005 的 staged
诊断；应把 direct gather 标为 attempted / rejected，并为下一阶段留下明确的 production boundary
和 row-source policy 状态。
