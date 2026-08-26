# Phase 040 PointXYZ-like production expansion result

## 执行范围

本阶段把 Phase 030 的 exact `PointXYZ` production gate 扩展为 `RVVXYZAoSFloatLayout<PointT>` gate。已验证并采纳的常见输入点型为 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`；输出类型仍为 `pcl::MomentInvariants`，row source（行来源）仍是 `computeFeature` 经 KdTree nearestKSearch 得到的 indexed neighbor list。

本阶段不覆盖 `PointXYZRGBNormal`、`PointXYZINormal`、用户自定义点型、非 xyz 单 float layout、`Scalar=double`、其它输出类型、full-cloud overload 或 KdTree/search 优化。

## 实现和测试回填

| action | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| RED asm gate | done | `make check_production_pointxyzi_rvv_asm` 在 exact `PointXYZ` gate 下曾失败。 | target 能观察 typed production RVV 缺失。 |
| typed correctness | done | `make run_test_compare` | Std/RVV 各 8/8 pass；新增 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` public `computeFeature` 对拍。 |
| typed bench cases | done | `src/bench_moment_invariants.cpp` | 新增 `mi_production_compute_feature_pointxyzi`、`mi_production_compute_feature_pointxyzrgb`、`mi_production_compute_feature_pointxyzrgba`。 |
| production gate expansion | done | `features/include/pcl/features/impl/moment_invariants.hpp` | 使用 `RVVXYZAoSFloatLayout<PointT>`，保留 `PointOutT`、dense、规模和 u32 byte offset gate。 |
| typed asm evidence | done | `make check_production_pointxyzi_rvv_asm`；`make check_production_pointxyzrgb_rvv_asm`；`make check_production_pointxyzrgba_rvv_asm` | 三种 typed production helper / public symbol 均可归属 `vlux*ei32` 和 `vfred*sum`。 |
| typed board evidence | done | `log/board/repeated_phase040_production_*_compute_feature/` | 三种 typed case 均为 5/5 正向，decision bucket `weak_positive`。 |

## Typed board summary

| point type | case | summary | median | min | max | B/A < 1 | Doctor |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- |
| `PointXYZ` | `mi_production_compute_feature` | `log/board/repeated_phase030_production_compute_feature/summary.md` | `1.067x` | `1.023x` | `1.085x` | `0/5` | 0E/0W/2S |
| `PointXYZI` | `mi_production_compute_feature_pointxyzi` | `log/board/repeated_phase040_production_pointxyzi_compute_feature/summary.md` | `1.071x` | `1.065x` | `1.075x` | `0/5` | 0E/0W/2S |
| `PointXYZRGB` | `mi_production_compute_feature_pointxyzrgb` | `log/board/repeated_phase040_production_pointxyzrgb_compute_feature/summary.md` | `1.078x` | `1.066x` | `1.091x` | `0/5` | 0E/0W/2S |
| `PointXYZRGBA` | `mi_production_compute_feature_pointxyzrgba` | `log/board/repeated_phase040_production_pointxyzrgba_compute_feature/summary.md` | `1.078x` | `1.059x` | `1.087x` | `0/5` | 0E/0W/2S |

每个 summary 的 `evidence_role=production-public`、`A/B boundary=public overload`、`timer_boundary=public_compute_feature_with_kdtree_search_and_output_write`。计时包含 KdTree nearestKSearch、indexed moment accumulation 和 output write，不包含点云构造或 KdTree 初始化。

## Evidence Doctor 和 registry

Phase 040 三个 typed summary 的 Evidence Doctor 均为 0 Error / 0 Warning / 2 Suggestion。两条 suggestion 是环境 metadata（taskset、governor、freq、temperature）和 binary identity（binary hash 或等价二进制身份）缺失；由于每个点型 5-run 全部正向、decision bucket 稳定，本阶段接受为 weak-positive production evidence，并把 metadata 补强列为后续 evidence hardening（证据加固）。

`log/evidence_registry.json` 已登记 Phase 030 / 040 summary、manifest 和 doctor，状态为 fresh。

## EvidenceDecision

Phase 040 结论为 adopted production behavior（已采纳生产行为）：`PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 在 `Scalar=float`、dense AoS layout、`PointOutT=pcl::MomentInvariants`、indexed neighbor list 且邻域规模达到 16 时，可通过真实 public `computeFeature` 命中 RVV indexed moment accumulation。其它路径自然 fallback（回退）到标量。

该结论只回答当前 public RVV path 是否快于当前 public scalar path。它不关闭 normal 复合点型、自定义点型、full-cloud overload、其它输出类型、`Scalar=double`、非 dense surface 或 search/KdTree 优化。

## Continue / Stop

当前 roadmap 和 optimization matrix 中没有必须在本 topic 内继续推进的高优先级未阻塞动作。`PointXYZRGBNormal`、`PointXYZINormal` 和自定义点型扩展需要新的 phase 明确冻结范围并补独立板卡证据；full-cloud overload 需要真实 caller/profile 后再开。默认下一步是 review / commit decision（审查或提交判断），不继续自动扩大生产范围。
