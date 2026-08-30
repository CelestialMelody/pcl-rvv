# SIFT Keypoint RVV Topic

## 当前结论

当前 `EvidenceDecision`（证据决策）是 `production-adopted-narrow-scope`。生产源码
`keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` 中的 `computeScaleSpace()` 在
`__RVV10__` 构建下使用 RVV 批量计算 Gaussian weight（高斯权重），再用 scalar tail
（标量尾段）按原邻域顺序累加，公开 API 不变。

已采纳范围只覆盖 `PointXYZI -> PointWithScale`、organized dense synthetic
`public_sift_keypoint_320x240` 公开入口 case、`Scalar=float` 和 Milkv-Jupiter 板卡上的
production-public（真实公开入口）证据。`findScaleSpaceExtrema()`、其它点型和真实 workload
尚未采纳。

## 阅读路径

| 目的 | 路径 |
| --- | --- |
| 函数级评估 | `doc/sift_keypoint-evaluation.zh.md` |
| 测试总览 | `doc/testing-overview.zh.md` |
| 正确性测试 | `doc/correctness-tests.zh.md` |
| benchmark 和证据 | `doc/benchmark-and-evidence.zh.md` |
| 优化证据 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` |
| phase 索引 | `doc/phases/README.zh.md` |
| production phase result | `doc/phases/010-production-full-gaussian-rvv/result.zh.md` |
| 优化路线图 | `doc/optimization-roadmap.zh.md` |
| production 长期文档 | `../../../doc-rvv/keypoints/sift_keypoint-RVV.zh.md` |

## 常用命令

```bash
make -C test-rvv/keypoints/sift_keypoint run_test_compare
make -C test-rvv/keypoints/sift_keypoint check_sift_keypoint_rvv_asm
SSH_AUTH_SOCK="${SSH_AUTH_SOCK}" make -C test-rvv/keypoints/sift_keypoint board_repeated_public record_evidence_state_public
make -C test-rvv/keypoints/sift_keypoint check_evidence_freshness
```

## 证据白名单

可提交摘要证据以 topic-local summary / manifest / Evidence Doctor（证据体检）和 registry
（证据登记表）为主：

- `log/board/repeated_phase010_public_compute/summary.md`
- `log/board/repeated_phase010_public_compute/evidence_manifest.json`
- `log/board/repeated_phase010_public_compute/evidence_doctor.md`
- `log/board/repeated_phase010_public_compute/evidence_doctor.json`
- `log/board/public_trace_phase010_public_compute/public_trace_compare.md`
- `log/board/public_trace_phase010_public_compute/public_trace_compare.json`
- `log/board/repeated_phase000_scale_space_profile_prerequisite/summary.md`
- `log/board/repeated_phase000_scale_space_profile_prerequisite/evidence_manifest.json`
- `log/board/repeated_phase000_scale_space_profile_prerequisite/evidence_doctor.md`
- `log/board/repeated_phase000_scale_space_profile_prerequisite/evidence_doctor.json`
- `log/evidence_registry.json`

raw logs（原始日志）、`build/` 二进制、本机 `config.mk` 和板卡私有配置默认不提交。
