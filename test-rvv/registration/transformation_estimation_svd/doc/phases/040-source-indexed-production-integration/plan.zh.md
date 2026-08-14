# Phase 040 计划：source-indexed production integration

## 阶段意图和边界

本阶段把 Phase 030 已经 positive 的 `source-indexed-cloud-pair`（源索引点云对）candidate 进入 production integration loop（生产接入闭环）。生产候选范围只包含：

- public entry：`estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)`。
- row source policy：`source[indices_src[i]]` 与 `target[i]` 配对。
- point type / Scalar / layout：`Scalar=float`，source/target 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout`。
- runtime gate：`use_umeyama_ == true`、source/target dense、`indices_src.size() == cloud_tgt.size()`、`n >= 16`、indices 非负且落在 source cloud 内、source cloud 大小满足 32-bit byte offset gather gate。

本阶段不接入 `dual-indices-cloud-pair`、`correspondence-pair`、`Scalar=double`、非 dense 输入或 `use_umeyama_ == false` 分支；也不把 `PointXYZI` / `PointXYZRGB` 的 ordered-cloud-pair 代表性 correctness 写成 source-indexed 逐类型性能。

## 当前状态清单

| area | current state | 证据 |
| --- | --- | --- |
| source-indexed candidate correctness | implemented / pass | `run_test_compare` 已含 `SourceIndexedPublicMatchesFusedReference`、`SourceIndexedCandidateMatchesScalar` |
| source-indexed board diagnostic | positive | `log/board/source_indexed_cloud_pair_repeated/summary.md`：same-boundary median 4K `1.917x`、64K `1.843x`、256K `1.785x`；mixed-boundary median 4K `7.012x`、64K `9.031x`、256K `8.827x` |
| source-indexed Evidence Doctor | usable with warnings | Errors=0、Warnings=4、Suggestions=0；warning 需要在 result 中按 diagnostic 边界解释 |
| source-indexed asm | gather attributed | RVV bench full asm 内可见 `vluxseg3ei32.v`、`vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs` |
| current production | ordered-cloud-pair only | source-indexed overload 仍走 iterator 标量路径 |

## 实现和测试动作

1. 在 production `detail` 中新增 source-indexed RVV accumulation helper，复用 `indexed_load3_f32m2` 读取 source xyz，复用 `strided_load3_f32m2` 读取 target xyz。
2. 在 source-indexed public overload 里已有 size check 之后短路尝试 RVV helper；helper 返回 `false` 时自然 fallback 到原 `ConstCloudIterator`。
3. 增加 production direct correctness：确认 RVV helper 对合法 source-indexed 输入命中，并与 fused scalar reference 在误差预算内一致。
4. 增加 source-indexed fallback correctness：小规模、非 dense、非法 index、`use_umeyama_ == false`、`Scalar=double` 均不命中 RVV。
5. 增加 production source-indexed board repeated target，生成 `production_source_indexed_cloud_pair_repeated` summary / manifest / Evidence Doctor / registry。
6. 刷新 asm、QEMU correctness、board production repeated、Evidence Doctor、evidence_status 和相关文档。

## 板卡复跑预算和决策桶

- 复跑预算：5 runs，20 iterations，5 warm-up，与 Phase 020 production direct 口径一致。
- positive：各主要 size public Std/RVV median 均 > 1.15x 且 doctor 无 Error。
- weak-positive：median >= 1.03x 且 min >= 0.97，可作为窄生产候选但要说明收益弱。
- neutral / negative / unstable：不进入 production-ready，回滚或暂缓 source-indexed production patch。

## 继续 / 停止条件

若 production direct correctness、asm、board repeated 和 Evidence Doctor 均闭合，Phase 040 可把 source-indexed 标为 `adopted / production-ready`。否则保留 Phase 030 diagnostic positive，并把 production patch 回退或标记为 blocked。

Phase 040 完成后，默认下一阶段只评估 `dual-indices-cloud-pair` 或 `correspondence-pair`；它们不能继承 source-indexed 或 ordered-cloud-pair 的 production 结论。
