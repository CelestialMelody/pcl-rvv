# ISS 3D Benchmark And Evidence

## Bench 入口

| mode / target | case | 计时边界 | 证据角色 |
| --- | --- | --- | --- |
| default diagnostic | `scatter_indexed_256` | 只计时 test-only scatter helper，排除 search、EVD、NMS 和 output。 | diagnostic / component ablation |
| default diagnostic | `scatter_indexed_tail_73` | 同上，邻域数 73 覆盖 VL tail。 | diagnostic |
| default diagnostic | `scatter_contiguous_256` | 同上，邻域索引近似连续。 | diagnostic |
| `--mode public` | `public_iss_3d_grid_4096` | public `ISSKeypoint3D::compute()`，包含 KdTree search、scatter、Eigen EVD、NMS 和 output。 | production_public |

## 当前证据摘要

| run label | boundary | result | doctor | 结论 |
| --- | --- | --- | --- | --- |
| `iss_3d_phase000_scatter_f64_repeated` | diagnostic | indexed 256 values include one 0.691x outlier；median 1.629x。 | Errors=0，Warnings=3，Suggestions=6。 | historical unstable，不作为当前 truth。 |
| `iss_3d_phase000_scatter_f64_rerun1` | diagnostic | indexed 256 median 1.662x，tail 73 median 1.213x，contiguous 256 median 1.859x。 | Errors=0，Warnings=1，Suggestions=6。 | 当前 diagnostic positive。 |
| `iss_3d_phase010_production_public_repeated` | production_public | median 1.005x，min 0.996x，2/5 退化。 | Errors=1，Suggestions=3。 | historical neutral / degraded frequency。 |
| `iss_3d_phase010_production_public_reduction_shape` | production_public | median 1.014x，min 1.005x，max 1.022x，0/5 退化。 | Errors=0，Warnings=0，Suggestions=3。 | 当前 production neutral，不建议采纳。 |

## Evidence Doctor 处理

当前 production run 的 Suggestions 为：

- `environment_metadata_missing`：缺少 taskset、governor、freq、temperature 等环境字段。该项不单独阻塞，但削弱 near-threshold 结果解释力。
- `binary_identity_missing`：缺少 binary hash 或等价二进制身份字段。manifest 有 asm sha256，但没有完整 Std/RVV binary identity。
- `near_threshold_ba`：median 1.01x，接近 1.0，不能写成稳定加速。

这些 suggestion 共同使 production public 证据保持 `neutral`。它可以说明 probe 没有明显退化，但不足以支撑 adopted production behavior（已采纳生产行为）。

## 复现命令

```bash
make -C test-rvv/keypoints/iss_3d run_test_compare
make -C test-rvv/keypoints/iss_3d check_iss_3d_rvv_asm
make -C test-rvv/keypoints/iss_3d check_evidence_freshness
```

历史 production probe 的板卡 repeated 命令仅在重新引入 production probe patch 后使用：

```bash
SSH_AUTH_SOCK=<agent> make -C test-rvv/keypoints/iss_3d board_repeated_production record_evidence_state_production ISS3D_ALLOW_HISTORICAL_PRODUCTION_PROBE=1 PRODUCTION_BOARD_TAG=phase010_production_public_reduction_shape PRODUCTION_BOARD_REMOTE_TAG=phase010_production_public_reduction_shape PRODUCTION_BOARD_RUN_LABEL=iss_3d_phase010_production_public_reduction_shape REPEATED_BOARD_RUNS=5
```

## 提交边界

`summary.md`、`evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json` 和 `log/evidence_registry.json` 是 summary-only evidence（摘要证据）候选。每轮 `run_bench_*.log`、QEMU logs、`build/` 和远端 board 路径默认不提交。
