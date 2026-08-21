# organized_pointcloud_conversion benchmark 与证据

## Bench label 字典

| label | evidence role | 入口 / 被测路径 | 输入 | 当前状态 |
| --- | --- | --- | --- | --- |
| `pointxyz_disparity_dense_307k` | diagnostic | test-only PointXYZ cloud -> disparity | 640x480 finite | historical positive diagnostic |
| `pointxyz_disparity_mixed_invalid_307k` | diagnostic | test-only PointXYZ cloud -> disparity | 640x480 mixed invalid | historical positive diagnostic |
| `pointxyz_disparity_dense_1m` | diagnostic | test-only PointXYZ cloud -> disparity | 1024x1024 finite | historical diagnostic |
| `pointxyzrgb_disparity_rgb_dense_307k` | diagnostic | fused colored diagnostic RGB | 640x480 finite | historical positive diagnostic |
| `pointxyzrgb_disparity_mono_dense_307k` | diagnostic | fused colored diagnostic mono | 640x480 finite | historical positive diagnostic |
| `pointxyzrgb_disparity_rgb_mixed_invalid_307k` | diagnostic | fused colored diagnostic RGB | 640x480 mixed invalid | historical positive diagnostic |
| `pointxyz_decode_disparity_*` | diagnostic | decode disparity -> `PointXYZ` cloud | dense / mixed invalid | rejected negative diagnostic |
| `production_pointxyz_disparity_*` | production direct | public uncolored overload | `PointXYZ` | adopted |
| `production_pointxyzi_disparity_*` | production direct | public uncolored overload | `PointXYZI` | adopted representative |
| `production_pointxyzrgb_disparity_*` | production direct | public colored overload | `PointXYZRGB` RGB / mono | adopted |
| `production_pointxyzrgba_disparity_*` | production direct | public colored overload | `PointXYZRGBA` RGB | adopted representative |
| `production_full_pointxyz_encode_dense_307k` | production-shaped diagnostic | encodePointCloud-shaped helper | 640x480 organized `PointXYZ` | positive shaped evidence |
| `production_full_pointxyzrgb_encode_rgb_dense_307k` | production-shaped diagnostic | encodePointCloud-shaped helper + RGB PNG | 640x480 organized `PointXYZRGB` | positive shaped evidence |
| `production_full_pointxyzrgb_encode_mono_dense_307k` | production-shaped diagnostic | encodePointCloud-shaped helper + mono PNG | 640x480 organized `PointXYZRGB` | positive shaped evidence |
| `analyze_pointxyz_organized_dense_307k` | diagnostic | analyze max-depth / focal-length component | 640x480 organized finite `PointXYZ` | positive component diagnostic |
| `analyze_pointxyz_organized_mixed_invalid_307k` | diagnostic | analyze max-depth / focal-length component | 640x480 organized mixed invalid `PointXYZ` | positive component diagnostic |
| `production_analyze_detail_pointxyz_dense_307k` | production detail | `organized_compression_detail::analyzeOrganizedCloud` | 640x480 organized finite `PointXYZ` | adopted production-detail |
| `production_analyze_detail_pointxyz_mixed_invalid_307k` | production detail | `organized_compression_detail::analyzeOrganizedCloud` | 640x480 organized mixed invalid `PointXYZ` | adopted production-detail |
| `production_analyze_detail_pointxyzi_mixed_invalid_307k` | production detail | `organized_compression_detail::analyzeOrganizedCloud` | 640x480 organized mixed invalid `PointXYZI` | adopted production-detail |

## 当前 production repeated 结果

| case group | result |
| --- | --- |
| `PointXYZ` uncolored | 5-run min/median/max 约 2.07x-2.11x |
| `PointXYZI` uncolored | 5-run min/median/max 约 2.01x-2.05x |
| `PointXYZRGB` RGB | 5-run min/median/max 约 1.35x-1.39x |
| `PointXYZRGB` mono | 5-run min/median/max 约 1.64x-1.77x；单独报告，不外推到 RGB |
| `PointXYZRGBA` RGB | 5-run min/median/max 约 1.35x-1.43x |

主证据路径：

- `log/board/production_direct_repeated/summary.md`
- `log/board/production_direct_repeated/evidence_manifest.json`
- `log/board/production_direct_repeated/evidence_doctor.md`

## Evidence Doctor 处理

Production repeated Doctor 输出 `Errors=0, Warnings=1, Suggestions=0`。唯一 Warning 是 `production_pointxyzrgb_disparity_mono_dense_307k` 的 `group_outlier`：mono 输出每点 1 byte，而 RGB 输出每点 3 bytes，计时边界不同，不能把 mono 高收益外推到 RGB。该 warning 不阻塞 adoption。

Full encode-shaped repeated Doctor 输出 `Errors=0, Warnings=0, Suggestions=0`，证据角色为 `production_shaped_diagnostic`。Analyze component repeated Doctor 输出 `Errors=0, Warnings=0, Suggestions=0`，证据角色为 `diagnostic`。

Analyze production-detail repeated Doctor 输出 `Errors=0, Warnings=1, Suggestions=0`。唯一 Warning 是 `production_analyze_detail_pointxyzi_mixed_invalid_307k` 的 `group_outlier`：`PointXYZI` fixture 的 z 分布让标量 max-depth 更新次数少于 `PointXYZ`，因此 speedup 较低。该 warning 不阻塞 `PointXYZI` 自身 1.820x median 正向结论，但禁止把 `PointXYZ` 的 3.8x 外推到其它点型。

Analyze detail 接入后的 full encode-shaped repeated Doctor 输出 `Errors=0, Warnings=0, Suggestions=0`，证据角色仍是 `production_shaped_diagnostic`；它说明接入后压缩形态 helper 仍正向，但不是真实 public class direct。

共享 single-run Doctor 输出 `Errors=3, Warnings=28`，Errors 全部来自 decode diagnostic cases。它们用于支撑 decode v0 rejected，不参与 encode production adoption。

## 运行方式

```bash
cd test-rvv/io/organized_pointcloud_conversion
make run_production_repeated_evidence_doctor
make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/full_encode_repeated
make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/analyze_production_detail_repeated
make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/full_encode_after_analyze_detail_repeated
make run_board_evidence_doctor
```

板卡 repeated 采集由已有 `run-*` 目录提供输入。Analyze component 结果使用同一 topic-local manifest wrapper 手动生成：

```bash
python3 script/generate_opc_board_evidence_manifest.py \
  --run-dir log/board/analyze_component_repeated/run-1 \
  --run-dir log/board/analyze_component_repeated/run-2 \
  --run-dir log/board/analyze_component_repeated/run-3 \
  --run-dir log/board/analyze_component_repeated/run-4 \
  --run-dir log/board/analyze_component_repeated/run-5 \
  --summary-log log/board/analyze_component_repeated/summary.md \
  --output log/board/analyze_component_repeated/evidence_manifest.json
python3 ../../script/evidence_doctor.py \
  --manifest log/board/analyze_component_repeated/evidence_manifest.json \
  --output log/board/analyze_component_repeated/evidence_doctor.md \
  --json-output log/board/analyze_component_repeated/evidence_doctor.json \
  --fail-on never
```

Analyze production-detail 和 after-patch full shaped 结果同样通过 `PRODUCTION_REPEATED_DIR` 指向对应目录复核：

- `log/board/analyze_production_detail_repeated/summary.md`
- `log/board/analyze_production_detail_repeated/evidence_doctor.md`
- `log/board/full_encode_after_analyze_detail_repeated/summary.md`
- `log/board/full_encode_after_analyze_detail_repeated/evidence_doctor.md`
