# Phase 000 Result: current-state-and-scaffold

## 执行范围

本阶段按计划建立 `extract_polygonal_prism_data` topic scaffold（脚手架）、标量 reference、RVV candidate、QEMU correctness、QEMU bench smoke、反汇编和板卡 repeated evidence。实际范围保持在 `test-rvv/segmentation/extract_polygonal_prism_data/**`，没有修改 production 源码。

## 计划动作回填

| action | status | 证据 | 结论 |
| --- | --- | --- | --- |
| 写 failing test | done | `make run_test_std` 首次失败：`fatal error: eppd.h: No such file or directory` | 测试先于 helper 存在，red step 成立 |
| 实现标量参考 helper | done | `make run_test_std` pass，2 tests | square height/order 和 two-rings XOR reference 语义闭合 |
| 写 RVV failing test | done | `make run_test_rvv` 失败：`segmentPolygonalPrismRvvCandidate` missing | RVV comparison test 先于 candidate 存在 |
| 实现 RVV candidate | done | `make run_test_compare` pass；Std 2 tests，RVV 4 tests | 单 polygon RVV candidate 与 reference 一致；multi-polygon fallback 成立 |
| 增加 bench harness | done | QEMU smoke Std/RVV checksum 一致；board repeated logs 可解析 | bench 输出合同可用 |
| 反汇编检查 | done | `build/asm/riscv/bench_eppd_rvv.full.asm` 中 `segmentPolygonalPrismRvvCandidate` 下有 `vlse32.v`、`vmand.mm`、`vmxor.mm`、`vcompress.vm` | RVV 指令可归属到 candidate |
| 板卡 repeated evidence | done | `log/board/repeated/summary.md` | 5 runs median 3.56x，decision bucket positive |
| Evidence Doctor | done | `log/board/repeated/evidence_doctor.md` | Errors=0 / Warnings=0 / Suggestions=0 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | `bench_eppd` test helper，post-projection single polygon scan |
| 计时边界 | 不包含 plane fitting、`projectPoints`、真实 `segment` wrapper 或 output 对象状态 |
| row source / point type / layout | dense ordered indices、`PointXYZ`、`float`、contiguous x/y/z stride |
| baseline / candidate | baseline 为 `segmentPolygonalPrismReference`；candidate 为 `segmentPolygonalPrismRvvCandidate` |
| 能证明什么 | 当前诊断边界内 RVV edge-parity + height mask + output compress 正确且板卡正向 |
| 不能证明什么 | 真实 production dispatch、arbitrary indices、multi-polygon RVV、倾斜平面、泛型点类型或完整入口收益 |
| weak / negative 时是否允许 bounded production probe | 本轮为 positive；若后续完整入口审计显示 projection 成本稀释，仍需降级或保持 diagnostic |
| clean adoption 是否需要 production boundary A/B | yes；production 接入后必须用真实 `segment` direct test 和 board bench 再决策 |

## 板卡复跑预算和 Evidence Doctor

预算执行为 5-run repeated collection，每 run 使用 `iterations=8`、`warmup=2`，设备为 Milkv-Jupiter。speedup values 为 3.65x、3.53x、3.56x、3.56x、3.53x；median 3.56x，min 3.53x，max 3.65x，桶稳定为 positive。Evidence Doctor 报告 Errors=0 / Warnings=0 / Suggestions=0。

## optimization matrix 更新

`doc/phases/optimization-matrix.zh.md` 已更新：single polygon RVV edge-parity candidate 为 `partial-production-candidate for post-projection single polygon diagnostic`；concave hull XOR、arbitrary indices gather 和 production integration 保持未闭合。

## 文档套件与结构审计

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已新增 `README.zh.md` | 需要先读路径、常用命令、证据白名单 | adopted | 文件存在 | none |
| testing overview / correctness tests | 轻量合并到 evaluation 和测试源码注释 | 当前只有 4 个 tests 和 1 个 bench case | merged:`doc/extract_polygonal_prism_data-evaluation.zh.md#测试计划和结果` | 当前 target 少，独立文档可暂缓 | 若进入 production loop 再拆分 |
| benchmark and evidence | 合并到 evaluation、README 和 result | 需要说明 QEMU / board 边界 | adopted | `README.zh.md#证据白名单`、本 result | none |
| optimization evidence | `optimization-matrix.zh.md` + roadmap | 需要 candidate 状态和证据路径 | adopted | matrix / roadmap 已存在 | none |
| test-support code map | Traceability Map 已在 evaluation | 需要定位 reference、candidate、bench、script | merged:`doc/extract_polygonal_prism_data-evaluation.zh.md#Traceability Map` | 当前支撑代码较小 | 若新增 production direct helper 再拆独立文档 |
| production topic doc | 无 `doc-rvv` | 未接 production 前不适用 | not_applicable with evidence | production 未改 | PI5 后再判断 |
| artifact tracking | 新增 topic 文件均在 `test-rvv/segmentation/extract_polygonal_prism_data/**` | 需要路径限定扫描 | partial | 当前为 untracked topic assets，等待 reviewer / commit phase | 提交前 staging topic paths |

## Continue / Stop Decision

本阶段完成，但 topic 未完成。默认下一阶段是 `010-production-scope-audit`，因为 single-polygon diagnostic 已正向，但继续修改 production 需要用户确认 production integration loop。当前合法停止条件为 `production_authorization_boundary`：继续到 PI2 会触碰 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` 生产源码。

## next_phase_default

`010-production-scope-audit`：审计完整 `segment` 入口成本、single polygon dispatch/fallback、concave XOR fallback 策略、arbitrary indices、倾斜平面和点类型边界；若 PI1 gate 闭合，再由用户确认是否进入 production integration loop。
