# Phase 060: PointXYZRGB / PointXYZRGBA 点型扩展计划

## 阶段意图和边界

Phase 045/046 已采纳 `selectWithinDistance` 的 `vcompress` production RVV path，Phase 050 已用接入后的板卡数据证明 `PointXYZI` 仍为 positive-stable。当前 production gate（生产准入条件）使用 `RVVXYZFloatLayout<PointT>`，理论上会覆盖更多 registered single-float xyz 点型；但当前长期证据只覆盖 `PointXYZ` 和 `PointXYZI`。

本阶段继续扩展 `PointXYZRGB` / `PointXYZRGBA` 的 dedicated evidence（专门证据），避免把已有点型性能直接外推到其它带颜色字段的点类型。

本阶段不修改 production 源码，不改变已采纳的 `vcompress` 实现族。若板卡数据正向，则只把对应点型标成已验证；若数据弱、负向或不稳定，则不扩大长期结论。

## RED

当前 `bench_sac_model_sphere` 只接受 `PointXYZ` / `PointXYZI`：

```bash
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'
```

结果：编译通过，但运行返回 `Unsupported point type: PointXYZRGB (expected PointXYZ or PointXYZI)`，exit code 为 2。

## 实现计划

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 扩 correctness 点型 | `include/test_sac_model_sphere.h`、`src/test_sac_model_sphere.cpp` | 新增 RGB/RGBA layout 对拍，不改变既有 TEST 名和数量语义。 |
| 扩 bench CLI 点型 | `include/bench_sac_model_sphere.h`、`src/bench_sac_model_sphere.cpp` | `PointXYZRGB` / `PointXYZRGBA` QEMU smoke 输出 Dataset 行。 |
| 扩 manifest 点型解析 | `script/generate_sphere_board_evidence_manifest.py` | manifest `point_type`、`name`、`gate` 和 asm count 绑定当前点型。 |
| 增加 board evidence target | `Makefile` | 独立 RGB/RGBA collect、doctor、registry、status target。 |
| 板卡复跑 | Phase 060 manifest / doctor | 各点型 5-run repeated board，select production row clean 或解释异常。 |

## 验证

```bash
make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'
make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGBA'
make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_rgb_point_type_repeated_board_evidence collect_rgba_point_type_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_rgb_point_type_board_evidence_state record_rgba_point_type_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_sphere rgb_point_type_evidence_status rgba_point_type_evidence_status
```

QEMU 只用于 correctness（正确性）和日志形状；性能结论只写板卡 repeated 数据。

## Continue / Stop Decision

若 RGB/RGBA 都正向并且 Evidence Doctor 的 select 行 clean，则更新 Phase 060 result、matrix、roadmap、topic docs、`doc-rvv` 和 Handoff。若其中任一点型负向或不稳定，则只关闭已证明的点型，不扩大生产性能边界。
