# Phase 010 Result: production-scope-audit

## 执行范围

本阶段完成了 full-scan diagnostic（完整扫描段诊断）：测试资产中的 reference / RVV helper 同时接收原始点、`projected_points`、平面系数和运行期 `k1/k2`，覆盖真实 `segment` 扫描段里的 height distance（高度有符号距离）、二维投影坐标选择、single polygon predicate（单多边形判定）和保序输出压缩。

本阶段只修改 `test-rvv/segmentation/extract_polygonal_prism_data/**`，没有修改 production 源码 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`。

## 计划动作回填

| action | status | 证据 | 结论 |
| --- | --- | --- | --- |
| RED：倾斜平面 full-scan test | done | `make run_test_rvv` 首次失败：`segmentPolygonalPrismRvvFullScanCandidate` 不存在；实现后曾因 fixture 平面常数不匹配失败 | red step 成立，测试能抓住 Phase 000 只按简化高度 / 坐标处理的缺口 |
| GREEN：full-scan reference / RVV helper | done | `make run_test_compare` pass；Std 2 tests，RVV 5 tests | 倾斜平面手写 expected、reference 和 RVV candidate 一致 |
| bench full-scan path | done | QEMU smoke Std/RVV checksum 均为 `17458065235267028439` | bench 二进制命中新 full-scan helper；QEMU 只作为日志形状和 checksum 证据 |
| checksum 口径修复 | done | board checksum 从偶数轮 XOR 抵消的 `0` 变为 `6086149666645642665` | 证据日志可读性提升，不改变被测输出集合 |
| 反汇编检查 | done | `build/asm/riscv/bench_eppd_rvv.full.asm` 和 `build/asm/riscv/bench_eppd_rvv.asm` | `segmentPolygonalPrismRvvFullScanCandidate` 下出现 `vlse32.v`、`vfmacc.vf`、`vmand.mm`、`vmxor.mm`、`vcompress.vm` |
| 板卡 repeated evidence | done | `log/board/repeated/summary.md` | final current run：5 runs，median 1.98x，min 1.77x，max 2.16x，values 1.81x、1.77x、1.99x、1.98x、2.16x |
| Evidence Doctor | done | `log/board/repeated/evidence_doctor.md` | final current run：Errors=0 / Warnings=0 / Suggestions=0 |

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | `bench_eppd` topic-local full-scan helper；仍不是 public overload |
| 计时边界 | 包含扫描段里的平面距离、`projected_points[k1/k2]` 读取、polygon predicate 和 output compress；不包含 plane fitting、`SampleConsensusModelPlane::projectPoints`、真实 `segment` wrapper、对象状态或 `PointIndices` 写回对象生命周期 |
| row source / point type / layout | dense ordered indices、`PointXYZ`、`float`、single polygon、AoS stride load |
| baseline / candidate | baseline 为 `segmentPolygonalPrismFullScanReference`；candidate 为 `segmentPolygonalPrismRvvFullScanCandidate` |
| 能证明什么 | 在 full-scan diagnostic 边界内，RVV 平面距离 mask + 投影坐标 polygon predicate + `vcompress` 保序输出正确，并在 Milkv-Jupiter 上稳定 positive |
| 不能证明什么 | 真实 production dispatch、`projectPoints` 前置成本是否稀释收益、arbitrary indices gather、concave hull 多 polygon RVV、泛型点类型、`Scalar=double` 或完整 public `segment` speedup |
| weak / negative 时是否允许 bounded production probe | 本轮 final current run 为 positive；若 production direct 证明 `projectPoints` 稀释收益或 dispatch 维护成本过高，应降级或保持 diagnostic |
| clean adoption 是否需要 production boundary A/B | yes；必须经 PI1-PI5 生产接入闭环，含 public entry correctness、fallback、asm 和 board |

## 板卡复跑预算和 Evidence Doctor

本阶段执行了计划内 5-run repeated collection；checksum 口径修复后又按同边界完成 final 5-run current run。中间一批新二进制 run 因长尾触发 `long_tail_or_variance` warning；按计划追加一次同边界复跑后，final current run 为 median 1.98x、min 1.77x、max 2.16x，decision bucket 稳定为 positive，Evidence Doctor 为 0 / 0 / 0。旧的 3.56x post-projection summary 和中间 full-scan runs 降级为 historical evidence，不作为当前 truth。

## optimization matrix 更新

`doc/phases/optimization-matrix.zh.md` 已更新：full-scan single polygon RVV candidate 为 `partial-production-candidate`。Phase 000 的 post-projection candidate 保留为历史较窄证据；production integration 仍为 `turn_stop_deferred with authorization boundary`。

## 文档套件与结构审计

| area | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| README navigation | 已指向 evaluation、phase、roadmap、matrix、测试和证据白名单 | adopted | 本阶段同步更新 | none |
| testing overview / correctness tests | 仍合并在 evaluation、测试源码注释和 phase result | merged | 当前测试规模可读；新增 full-scan test 有中文说明 | production direct 后再拆独立文档 |
| benchmark and evidence | README、evaluation、result 和 summary 分工 | adopted | final summary / doctor 已刷新 | none |
| optimization evidence | roadmap + matrix + result | adopted | candidate 状态已回填 | none |
| test-support code map | Traceability Map 在 evaluation | merged | 新 full-scan helper 已补入 map | production direct 后再扩 |
| production topic doc | 无 `doc-rvv` | not_applicable with evidence | production 未改、PI5 未发生 | PI5 后再判断 |
| artifact tracking | topic assets 仍为未跟踪文件；raw `build/`、`log/` 和 `__pycache__` 默认不提交 | partial | 提交前需 staging topic asset，并排除 raw/build/local cache | commit phase 前复核 |

## Continue / Stop Decision

本阶段完成。当前 EvidenceDecision（证据决策）是 `partial-production-candidate`：full-scan diagnostic 对生产接入更有说服力，但仍不能替代 production direct（真实生产路径证据）。当前默认下一步是 `PI1 production_integration_plan`；继续到 PI2 会触碰 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`，命中 `production_authorization_boundary`。

仍存在 medium-priority 诊断扩展：concave hull 多 polygon RVV 和 arbitrary indices gather。它们可以在 production 授权前继续另开 phase，但当前 high-priority 决策问题已经变成“是否允许进入生产接入闭环”；若生产接入只采纳 single polygon / dense ordered fallback，二者可作为后续扩展队列，而不是阻塞本阶段结论。

## next_phase_default

`PI1 production_integration_plan`：在不直接写 production 主体前，冻结 public `segment` 的接入范围、`__RVV10__` gate、`PointT` traits / layout、dense ordered indices、single polygon fallback、`Scalar=double` fallback、arbitrary indices fallback、concave hull fallback、生产直连测试和板卡证据计划。PI1 gate 闭合且用户授权后，才能进入 PI2 production patch。
