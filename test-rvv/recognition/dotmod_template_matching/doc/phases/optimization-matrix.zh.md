# DOTMOD Template Matching Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `submap-direct-window-score-rvv` | organized quantized byte map | `uint8_t` image/template row-major layout | one window x one template x one modality helper | `run_test_compare` passed | Phase 000 bench passed | 5-run board median `2.440x` | `vle8` / `vand` / `vmsne` / `vcpop` present | `Errors=0`, `Warnings=0`, `Suggestions=2` | adopted for diagnostic evidence | none |
| `full-detecttemplates-direct-window-rvv` | organized quantized byte map sliding windows | `uint8_t` image/template row-major layout | full detectTemplates-shaped helper | `run_test_compare` passed | Phase 010 bench passed | 5-run board median `2.124x` | `vle8` / `vand` / `vmsne` / `vcpop` present | `Errors=0`, `Warnings=0`, `Suggestions=2` | adopted as production probe input | none |
| `production-direct-window-rvv` | organized quantized byte map sliding windows | `uint8_t` image/template row-major layout | real `DOTMOD::detectTemplates()` public entry | `run_production_direct_test_compare` passed | `bench_dotmod_template_matching_production_direct_*` | historical pre-Phase030 median `3.164x`; current post-Phase030 evidence supersedes it | `dotmodScoreWindowDirectRVV` plus `vle8` / `vand` / `vmsne` / `vcpop` present | historical `Errors=0`, `Warnings=0`, `Suggestions=2` | adopted as production base, superseded by Phase 030 current shape | none |
| `response-buffer-reuse` | organized quantized byte map sliding windows | `uint8_t` image/template row-major layout | real `DOTMOD::detectTemplates()` public entry | `run_production_direct_test_compare` passed | `bench_dotmod_template_matching_production_direct_*` | current 5-run board median `3.298x`, range `3.276x - 3.304x`, `B/A < 1 = 0/5` | `dotmodScoreWindowDirectRVV` plus `vle8` / `vand` / `vmsne` / `vcpop` present | `Errors=0`, `Warnings=0`, `Suggestions=2` | adopted production behavior | none inside current topic |
| `threshold-output-rvv` | detection output order | `DOTMODDetection` vector append order | threshold scan and output push | not run | not run | not run | not run | not run | deferred outside current closeout | resume only if detection output dominates profile |
| `QuantizedMap::getSubMap-standalone-rvv` | standalone submap copy | `uint8_t` row-major source/copy layout | `QuantizedMap::getSubMap()` callers outside current RVV branch | not run | not run | not run | not run | not run | not_applicable for adopted path | open separate support-kernel topic if another caller needs it |

## Scope Notes

`point_type_expansion_queue`: not applicable。当前 topic 处理量化 byte map，不是 PCL 点类型模板入口。

`Scalar` expansion: not applicable。生产计分输入是 `unsigned char` / `uint8_t`，`responses` 仍按原路径使用 `float`。

`layout` expansion: current evidence covers row-major `QuantizedMap` data and contiguous template feature vectors.
其它对象状态、真实 RGB-D 输入分布和 modality preprocessing（模态预处理）不由本矩阵关闭。
