# Phase 030 Plan: arbitrary-indices-gather-diagnostic

## 阶段意图和边界

本阶段在 topic-local 测试资产中验证 arbitrary indices gather（任意索引离散加载）路径。Phase 010 已证明 dense ordered indices（连续有序索引）下的 full-scan single polygon helper；但 production `segment` 使用 `(*indices_)[i]` 访问原始点，不能假设 `indices_[i] == i`。

本阶段只修改 `test-rvv/segmentation/extract_polygonal_prism_data/**`。不修改 production 源码，不改变公开 API。

## 当前状态清单

| area | 当前状态 | 证据路径 |
| --- | --- | --- |
| full-scan dense candidate | correctness / asm / board positive | `doc/phases/010-production-scope-audit/result.zh.md` |
| indexed row source | 当前 RVV full-scan helper对非 dense indices fallback 到 reference | `include/impl/eppd_candidates.hpp` |
| production row source | `segment` 对原始点使用 `(*input_)[(*indices_)[i]]`，投影点使用 `projected_points[i]` | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| indexed full-scan RVV | 原始点 x/y/z 可按 `indices` gather，投影点仍按扫描 lane strided load | gather 成本可能抵消收益；32-bit byte offset 只覆盖有限云规模 |
| dense + indexed unified helper | dense 和 indexed 使用同一 full-scan helper，按 `hasDenseOrderedIndices` 选择 strided 或 gather | 分支复杂度增加，asm 归属需分别检查 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| indexed full-scan RVV | arbitrary indices | `PointXYZ` / `float` / 32-bit byte offset | 新增 indexed correctness | 新增 indexed bench mode 或参数 | repeated board | gather 指令 `vluxei32.v` + compress | manifest + doctor | 本阶段待定 |

## 实现和测试动作

| action | 产物 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| RED：新增 indexed full-scan test | `src/test_eppd.cpp` | `make run_test_rvv` 失败，因为当前 helper fallback 或缺少 indexed RVV 路径标记 | 测试能证明 source index 输出不是 lane id |
| GREEN：实现 indexed gather helper | `include/impl/eppd_candidates.hpp` | `make run_test_compare` pass | 任意 indices 输出与 reference 一致 |
| bench：支持 `--indices dense/indexed` | `src/bench_eppd.cpp` | QEMU smoke checksum 一致 | board compare label 可区分 row source |
| manifest：记录 row source | `script/generate_eppd_board_evidence_manifest.py` | Evidence Doctor 可解析 | summary / manifest 标出 indexed row source |
| board：有界复跑 | `make run_board_eppd_repeated ... BENCH_ARGS="... --indices indexed"` | repeated summary + doctor | decision bucket 稳定或降级 |

## 32-bit byte offset gate

当前 RVV indexed helper 使用 `uint32_t` byte offset。测试资产里 `PointXYZ` 的 `indices` 由 fixture 生成并保证合法；production 接入时必须增加 `input_->size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` 或等价 gate，且不能把测试资产的合法性外推到 public entry。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | topic-local indexed full-scan helper |
| 当前决策问题 | indexed row source 是否值得纳入后续 production probe |
| diagnostic 是否可外推到 production | partial：row source 更接近 production，但仍不含 `projectPoints` 和 public object state |
| comparison-boundary / baseline mismatch 风险 | yes；indexed helper 与真实 `PCLBase` indices 校验边界不同 |
| weak / negative 时是否允许 bounded production probe | 若 dense path production 接入已成立，可先让 indexed fallback；弱/负 indexed 不能阻塞 dense 接入 |
| clean adoption 是否需要 production boundary A/B | yes |

## 板卡复跑预算和决策桶

预算为 5-run repeated collection，参数为 `--size 262144 --iterations 8 --warmup 2 --indices indexed`。若 Evidence Doctor 出现 warning 或方向接近 1.0，最多追加一次同边界确认复跑。

## 继续 / 停止条件

若 indexed full-scan positive，矩阵把 arbitrary indices 标为 production-candidate extension，但 PI2 仍需用户授权。若 indexed 退化，dense single polygon 仍保持 partial-production-candidate，indexed 在 production 中先 fallback。
