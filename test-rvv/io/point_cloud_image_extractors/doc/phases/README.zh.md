# Phase Index

| phase | status | current decision | next action |
| --- | --- | --- | --- |
| `000-current-state-and-diagnostic-scaffold` | done | RGB positive, fixed-factor weak-positive, full-range v0 negative | closed by Phase 000 result |
| `010-scaling-full-range-reduction-ab` | done | `scaling_reduction_v1` positive within diagnostic boundary | closed by Phase 040 PI1 plan |
| `020-rgb-segment-store-ab` | done | `rgb_segment_store_v1` positive and stronger than v0 | closed by Phase 040 PI1 plan |
| `030-doc-suite-parity` | done | topic-local doc suite role split adopted | closed by Phase 040 PI1 plan |
| `040-pi1-production-integration-plan` | done | production scope and fallback gate frozen, no production diff | closed by Phase 080/090 production evidence |
| `050-pi2-gate-policy-test-support` | done | Phase 040 PI2 gate policy encoded as test-only correctness gate, no production diff | closed by Phase 080/090 production evidence |
| `060-normal-field-diagnostic` | done | `normal_float_stride_v0` correctness pass but board repeated negative, median 0.61x | close normal v0; continue topic-local `070-label-mono16-diagnostic` |
| `070-label-mono16-diagnostic` | done | `label_mono16_stride_v0` positive within diagnostic boundary, median 1.21x, Doctor pass | closed by Phase 090 production evidence |
| `080-pi2-production-patch` | done | production-public repeated board positive: RGB 1.54x / 1.55x, scaling 1.52x; Doctor clean; 用户确认有收益即可采纳 | continue to Phase 090 label production loop |
| `090-label-mono16-production-plan` | done | production-public weak-positive: label mono16 median 1.08x, min 1.05x, max 1.09x; Doctor clean | ready_for_review / commit decision |

默认恢复入口：当前 production patch（生产补丁）已保留在工作树，RGB/scaling 和 label mono16
均已按 production-public 板卡证据采纳。若用户要求提交，默认只提交 topic 源码、测试、bench、
文档和 summary-only 证据引用；raw board logs 不进入默认提交边界。
