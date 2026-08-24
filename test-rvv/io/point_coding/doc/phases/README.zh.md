# point_coding Phase Index

## 当前状态

| phase | 状态 | plan | result | 当前决定 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-component-ablation` | completed | `000-current-state-and-component-ablation/plan.zh.md` | `000-current-state-and-component-ablation/result.zh.md` | diagnostic-positive / partial-production-candidate；不接 production。 |
| `010-production-shaped-boundary-scout` | completed | `010-production-shaped-boundary-scout/plan.zh.md` | `010-production-shaped-boundary-scout/result.zh.md` | small leaf encode 仍正向；decode 降级为 unstable / weak-positive。 |
| `020-encode-quantize-safety` | completed | `020-encode-quantize-safety/plan.zh.md` | `020-encode-quantize-safety/result.zh.md` | f64 exact quantize correctness 通过但性能拒绝；默认 candidate 恢复为 gather + scalar same-chain quantize。 |
| `030-encode-input-domain-and-boundary-fallback` | completed | `030-encode-input-domain-and-boundary-fallback/plan.zh.md` | `030-encode-input-domain-and-boundary-fallback/result.zh.md` | production 源码缺少可证明输入域合同；不实现 f32 fast path + boundary fallback。 |
| `040-decode-stability-profile` | completed | `040-decode-stability-profile/plan.zh.md` | `040-decode-stability-profile/result.zh.md` | decode-only 10-run median 全部大于 1，Evidence Doctor 为 `Errors=0，Warnings=1`；结论为 weak-positive diagnostic / no-production。 |
| `050-production-shaped-context-scout` | completed | `050-production-shaped-context-scout/plan.zh.md` | `050-production-shaped-context-scout/result.zh.md` | production-shaped diagnostic weak-positive，但 `Errors=0，Warnings=7`，不进 production。 |
| `055-full-octree-context-scout` | completed | `055-full-octree-context-scout/plan.zh.md` | `055-full-octree-context-scout/result.zh.md` | multi-leaf production-shaped diagnostic weak-positive，但 `Errors=0，Warnings=3`，不进 production。 |
| `056-public-octree-roundtrip-feasibility` | completed | `056-public-octree-roundtrip-feasibility/plan.zh.md` | `056-public-octree-roundtrip-feasibility/result.zh.md` | public octree roundtrip smoke 可构造；这不是 RVV 性能证据。 |
| `060-production-integration-plan` | adopted production behavior | `060-production-integration-plan/plan.zh.md` | `060-production-integration-plan/result.zh.md` | production-direct decode `PointXYZ` 10-run median `1.22x / 1.23x / 1.18x / 1.16x`，`Errors=0，Warnings=1`；用户已确认采纳，正式 `doc-rvv/io/point_coding-RVV.zh.md` 已创建。 |
| `070-pointxyz-like-traits-expansion` | adopted production behavior | `070-pointxyz-like-traits-expansion/plan.zh.md` | `070-pointxyz-like-traits-expansion/result.zh.md` | traits-gated `PointXYZI` / `PointXYZRGB` 10-run median `1.27x / 1.20x / 1.27x / 1.19x`，`Errors=0，Warnings=2`；按用户口径采纳 traits 扩围。 |
| `080-public-octree-end-to-end` | attempted / weak-near-threshold | `080-public-octree-end-to-end/plan.zh.md` | `080-public-octree-end-to-end/result.zh.md` | public decode / roundtrip 10-run median `1.01x / 1.01x / 1.01x / 1.02x`，`Errors=1，Warnings=2，Suggestions=4`；不作为稳定 public-positive evidence。 |

## 默认恢复入口

下一轮短 prompt 默认从 Phase 080 closeout 恢复。当前 production patch 已覆盖 exact `PointXYZ` 和 traits-gated PointXYZ-like decode；public octree synthetic boundary 已测为 near-threshold weak signal。若继续推进，需要真实 workload / profile 或具体点型需求。

## 文档归属

| 信息类型 | 主归属 |
| --- | --- |
| Phase 000 的实际命令、结果和 Evidence Doctor warning | `000-current-state-and-component-ablation/result.zh.md` |
| Phase 020 的 f64 quantize 负向证据和默认路径刷新 | `020-encode-quantize-safety/result.zh.md` |
| Phase 030 的输入域 / boundary fallback 语义审计 | `030-encode-input-domain-and-boundary-fallback/result.zh.md` |
| Phase 040 的 decode-only 10-run 稳定性和 long-tail warning | `040-decode-stability-profile/result.zh.md` |
| Phase 050 的 production-shaped context scout 和不稳定 warning | `050-production-shaped-context-scout/result.zh.md` |
| Phase 055 的 multi-leaf production-shaped context scout 和不稳定 warning | `055-full-octree-context-scout/result.zh.md` |
| Phase 056 的 public octree roundtrip feasibility smoke | `056-public-octree-roundtrip-feasibility/result.zh.md` |
| Phase 060 的 production-direct board data、production diff 和 PI5 stop | `060-production-integration-plan/result.zh.md` |
| Phase 070 的 traits-gated board data、production diff 和 adopted 扩围 | `070-pointxyz-like-traits-expansion/result.zh.md` |
| Phase 080 的 public octree boundary 稀释审计 | `080-public-octree-end-to-end/result.zh.md` |
| 跨 phase 候选和下一步 | `../optimization-roadmap.zh.md` |
| 矩阵状态 | `optimization-matrix.zh.md` |
| 函数级 EvidenceDecision | `../point_coding-evaluation.zh.md` |
| test / bench / code map | `../testing-overview.zh.md`、`../benchmark-and-evidence.zh.md`、`../test-support-code-map.zh.md` |
