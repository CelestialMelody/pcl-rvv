# SIFT Keypoint Benchmark And Evidence

## bench case 字典

| case label | 输入 | 计时边界 | 证明点 |
| --- | --- | --- | --- |
| `sift_scale_space_96_points` | 96 synthetic points, 29 neighbors | computeScaleSpace synthetic batch only | 基础 kernel 吞吐 |
| `sift_scale_space_192_points` | 192 synthetic points, 33 neighbors | computeScaleSpace synthetic batch only | 更大 batch 的稳定性 |
| `sift_scale_space_tail_113_points` | 113 synthetic points, 25 neighbors | computeScaleSpace synthetic batch only | tail lane 和局部规约 |
| `public_sift_keypoint_320x240` | 320x240 organized dense synthetic `PointXYZI` cloud | public `SIFTKeypoint::compute()`，包含 downsample、radiusSearch、scale-space、extrema 和输出组装 | 生产公开入口性能和 correctness trace |

## 证据入口

| target | 输出 | 证据角色 |
| --- | --- | --- |
| `run_test_compare` | gtest stdout | correctness（正确性） |
| `run_bench_*` | `log/qemu/...` | QEMU 只做可运行性和日志形状 |
| `check_sift_keypoint_rvv_asm` | `build/asm/riscv/bench_sift_keypoint_rvv.asm` | asm attribution |
| `board_repeated` | `log/board/repeated_phase000_scale_space_profile_prerequisite/run_*/` | raw board logs |
| `record_evidence_state_repeated` | summary / manifest / doctor / registry | 可提交摘要证据 |
| `board_repeated_public` | `log/board/repeated_phase010_public_compute/run_*/` | production-public raw board logs |
| `record_evidence_state_public` | `log/board/repeated_phase010_public_compute/{summary.md,evidence_manifest.json,evidence_doctor.md,evidence_doctor.json}` + `log/evidence_registry.json` | production-public 可提交摘要证据 |
| `board_public_trace compare_public_trace` | `log/board/public_trace_phase010_public_compute/public_trace_compare.md` | public output correctness（公开入口输出正确性） |

## 证据边界

当前 production evidence（生产证据）来自
`log/board/repeated_phase010_public_compute/summary.md`：`public_sift_keypoint_320x240`
5-run median speedup 为 1.309x，min 1.304x，max 1.310x，`B/A < 1 = 0/5`。
Evidence Doctor 报告在 `log/board/repeated_phase010_public_compute/evidence_doctor.md`，
结果为 Errors=0，Warnings=0，Suggestions=2。两条 Suggestion 是环境 metadata 和 binary identity
建议，不阻塞当前窄范围采纳。

diagnostic evidence（诊断证据）来自
`log/board/repeated_phase000_scale_space_profile_prerequisite/summary.md`，只说明局部 Gaussian
权重循环值得生产探针，不作为生产性能结论。
