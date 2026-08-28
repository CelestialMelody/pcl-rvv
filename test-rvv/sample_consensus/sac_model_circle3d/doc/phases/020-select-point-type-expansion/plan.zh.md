# Phase 020 Plan: selectWithinDistance 点类型扩展

## 阶段意图和边界

本阶段验证 Phase 010 已接入的 `SampleConsensusModelCircle3D<PointT>::selectWithinDistance` RVV（RISC-V Vector，可变长向量扩展）生产路径是否能在常见 xyz AoS（结构数组）点类型上保持 correctness（正确性）和板卡收益。当前生产源码已经使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 做 traits gate（点类型字段特征准入判断），因此只保留 `PointXYZ` 证据会让文档范围过窄。本阶段不修改 production（生产源码），只补测试资产、bench（性能测试）入口、summary manifest（摘要证据清单）和文档。

验证范围：

- 入口：`selectWithinDistance` public entry（公开入口）。
- row source（行来源）：direct indexed `indices_`，乱序索引。
- 点类型：`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。
- `Scalar`：model coefficients 为 `float`，输出 `error_sqr_dists_` 为 `double`。
- 布局：满足 `RVVXYZAoSFloatLayout<PointT>` 的 xyz float AoS。
- 规模：板卡 bench 使用 65536 点、200 次计时迭代、20 次 warm-up（预热）。

不覆盖：

- `countWithinDistance` 和 `getDistancesToModel`。
- `Scalar=double` 或非 xyz float AoS 点类型。
- `PointXYZRGBNormal`、`PointXYZINormal`、用户自定义点类型和非标准布局。
- 新 RVV family（实现族）或 count-specific formula（面向 count 的新公式）。

## 当前状态清单

| 对象 | 当前状态 | 路径 |
| --- | --- | --- |
| Phase 010 production select | 计划创建时 `PointXYZ` 生产证据曾写成 positive-with-warning；后续 post-narrowing 10-run 已刷新为不支持采纳。 | `../010-selectwithin-production-probe/result.zh.md` |
| 生产源码 | 计划创建时为 traits-gated `selectWithinDistanceRVVCircle3D`；本阶段证据负向后已收窄到 exact `pcl::PointXYZ`，三点型 fallback。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` |
| bench CLI | 已支持第四参数选择 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。 | `test-rvv/sample_consensus/sac_model_circle3d/src/bench_sac_model_circle3d.cpp` |
| correctness tests | 已覆盖 `PointXYZ`、三点型 public-vs-Std 对拍和三点型 helper fallback。 | `test-rvv/sample_consensus/sac_model_circle3d/src/test_sac_model_circle3d.cpp` |
| manifest wrapper | 已能从日志解析 point type，并用 `--candidate-gate` 保留 Phase 020 的 pre-narrowing gate 归属。 | `test-rvv/sample_consensus/sac_model_circle3d/script/generate_circle3d_board_evidence_manifest.py` |

## 优化矩阵

| candidate family | row source | point type / layout | correctness target | bench / board target | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| select production RVV point-type expansion | direct indexed `indices_` | `PointXYZI` xyz AoS | `run_test_compare` 新增点型 public-reference 对拍 | `collect_select_xyzi_repeated_board_evidence` | `check_select_production_asm` | `record_select_xyzi_board_evidence_state` | pending |
| select production RVV point-type expansion | direct indexed `indices_` | `PointXYZRGB` xyz AoS | 同上 | `collect_select_xyzrgb_repeated_board_evidence` | 同上 | `record_select_xyzrgb_board_evidence_state` | pending |
| select production RVV point-type expansion | direct indexed `indices_` | `PointXYZRGBA` xyz AoS | 同上 | `collect_select_xyzrgba_repeated_board_evidence` | 同上 | `record_select_xyzrgba_board_evidence_state` | pending |

## 实现和测试动作

1. 把 test / bench 输入构造模板化，为 `PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 设置额外字段，但断言只依赖 `x/y/z` 和公开入口输出。
2. bench 增加第四个 CLI 参数 `<point_type>`，默认仍为 `PointXYZ`，保持 Phase 010 命令兼容。
3. manifest wrapper 解析或接收 point type，把 production-public summary 的 `point_type` 和 case 名写成当前点型。
4. Makefile 新增三个点型的 repeated board target、manifest / doctor / registry / status target。
5. 运行 `run_test_compare`、`check_select_production_asm`，再在板卡上各执行 5-run repeated summary。若任一点型出现负向或不稳定，先按 Evidence Doctor（证据体检）结果决定是否追加有界复跑；默认总预算为每点型 5-run，只有 decision bucket（决策桶）摇摆时追加到 10-run。

## Evidence Doctor 和 registry 规则

每个点型生成独立 manifest、doctor Markdown、doctor JSON，并登记到 `log/evidence_registry.json`。Evidence Doctor 出现 Error 时，本阶段不能把该点型写成 adopted；Warning 必须解释退化频率、长尾、环境字段缺失或 clock skew（时钟偏斜）的影响。

## 阶段完成条件

本阶段可关闭的矩阵条目只限 `selectWithinDistance`、direct indexed `indices_`、当前三个点型、`Scalar=float`、当前板卡和当前规模。全部点型 correctness、asm 和 board summary 闭合后，若收益成立，则把生产 gate 记录为 `PointXYZ/PointXYZI/PointXYZRGB/PointXYZRGBA current evidence adopted`；若某点型负向，则文档必须把该点型列为保留 fallback 或后续收窄生产 gate 的候选。

## Diagnostic 到 Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-public。 |
| A/B boundary | Std binary public overload vs RVV binary public overload。 |
| 当前决策问题 | 现有 traits-gated `selectWithinDistance` 生产 RVV 是否能扩展到更多常见 xyz AoS 点型。 |
| diagnostic 是否可外推到 production | 不依赖 diagnostic 外推；本阶段使用真实 public entry 的 Std/RVV repeated board。 |
| comparison-boundary / baseline mismatch 风险 | 低；两侧 wrapper、row source、timer boundary 和 checksum policy 一致，允许 `gate` / `reduction` 差异作为目标变量。 |
| 弱 / 负 / 中性 / 不稳定时是否允许继续 | 可以在同点型内追加到 10-run；若仍负向或不稳，不扩大为泛型采纳。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前不是 RVV-family-selection；未来新增 family 才需要 detail A/B。 |

## 继续 / 停止条件

继续条件：板卡可用、correctness 通过、manifest 可解析且 Evidence Doctor 无未处理 Error。停止条件：任一点型 correctness 失败、板卡不可用、Evidence Doctor Error 无法修复、或需要修改 production gate 才能保证语义时暂停并报告。

下一阶段默认入口：本阶段完成后，如果三点型均正向，补齐 production closeout 文档和 Handoff；如果点型证据分裂，则先整理收窄 / fallback 决策，不继续做 `getDistancesToModel`。
