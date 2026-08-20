# marching_cubes phase index

| phase | status | default recovery | summary |
| --- | --- | --- | --- |
| `000-current-state-and-edge-interpolation-diagnostic` | done / attempted / neutral-to-weak-positive | 进入 Phase 010 | Edge interpolation RVV correctness 通过且 asm 可见，但 board repeated 为 `0.992x/1.024x/1.049x`，Evidence Doctor `Errors=1`，不支持 production probe。 |
| `010-cube-index-prepass-diagnostic` | done / strong-positive diagnostic | 进入 Phase 020 | 批量判断 active cell，再用标量 emit 输出三角形；board repeated 为 `2.431x/2.681x/3.217x`，Evidence Doctor `Errors=0`。 |
| `020-structure-parity-doc-suite-and-production-boundary-audit` | done / boundary-audited | 进入 Phase 030 | topic-local 文档套件已补齐，`performReconstruction()` public boundary 已审计；生产源码未改。 |
| `030-pi1-production-integration-plan` | done / adopted narrow production | 进入 Phase 040 | `PointNormal` / `float` public production path 已接入 RVV active-cell prepass，3-run board repeated 为 `5.364x/6.388x/8.840x`，Evidence Doctor `Errors=0`。 |
| `040-production-stabilization-and-point-type-expansion-audit` | done / narrow steady anchor + generic audit | 进入 Phase 050 | 非 `PointNormal` fallback correctness 已补；`production_direct_repeated` 刷新为 5-run positive；generic traits gate 审计转入 Phase 050。 |
| `050-generic-point-type-expansion-audit` | done / adopted generic production | 文档同步后可 review；性能探索见 roadmap | 源码 gate 已放宽为 `RVVXYZAoSFloatLayout<PointNT>`；`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 5-run board repeated 分别为 `3.873x`、`3.618x`、`3.601x`、`3.639x`，Evidence Doctor 均 `Errors=0, Warnings=0, Suggestions=0`。 |
| `060-active-z-tail-table-lookup-compression-ab` | done / attempted / neutral | ready_for_review；恢复条件见 roadmap | finite-collapse single-buffer 候选 correctness 通过且 asm 证明少一组 staging store，但 RVV-vs-RVV 3-run median 只有 `1.007x`，Evidence Doctor `Errors=0, Warnings=1, Suggestions=1`，不保留 production patch。 |

当前 production truth 仍是 Phase 050 adopted generic active-cell prepass。Phase 060 不改变源码接入结论；恢复时不要重新接入 finite-collapse，除非先有 profile 或更强同边界证据。
