# Phase 060 Result: point-type-expansion

## 实际执行范围

本阶段把 production public（公开入口生产证据）点型扩展从 `PointXYZ` 推进到常见 PointXYZ-like AoS float layout（结构数组 float x/y/z 布局）点型。真实入口仍是 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)`，bench 使用 `--path production --point-type <type>`，计时边界包含 plane setup（平面准备）、`projectPoints` 和 RVV scan（RVV 扫描段）。

本阶段修改了 production gate（生产准入条件）：`segmentRvv` 对 `sizeof(PointT) > 32` 的大步长 AoS 点型显式返回 false，让它们走 `segmentStd` fallback（回退路径）。这个 gate 是性能稳定性边界，不是算法语义边界；原因是 `PointXYZINormal` 在接入前板卡 5-run / 20-run 中收益不稳定且退化频率高。

## 动作结果

| action | 状态 | 证据 / 命令 | 结论 |
| --- | --- | --- | --- |
| production direct correctness（真实生产入口正确性） | done | `make run_test_compare`；新增 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 对拍和 `PointXYZINormal` fallback test | 三个 <=32-byte 点型命中 RVV 后与 `segmentStd` 输出一致；`PointXYZINormal` 被 gate 拒绝 |
| bench point type 参数 | done | `src/bench_eppd.cpp` 支持 `--point-type xyz|xyzi|xyzrgb|xyzrgba|xyzinormal` | 非 `xyz` 点型仅允许 `--path production`，避免 diagnostic 数据外推 |
| manifest parser（证据清单解析器） | done | `script/generate_eppd_board_evidence_manifest.py` 先匹配 `xyzinormal`、`xyzrgba`，再匹配较短 token | 修复 `xyzrgba` 被误分为 `PointXYZRGB`、`xyzinormal` 被误分为 `PointXYZI` 的 substring 风险 |
| post-gate board repeated（生产 gate 后重复板卡测试） | done | `repeated-production-*-postgate` 四组 summary / manifest / doctor | 当前正式文档采用 post-gate 数据 |
| evidence registry（证据登记表） | done | `test-rvv/segmentation/extract_polygonal_prism_data/log/evidence_registry.json` | post-gate 四组已登记，历史 point-type run 保留为 historical evidence（历史证据） |

## 当前证据

| 点型 | production gate | post-gate board summary | Evidence Doctor | decision bucket | 结论 |
| --- | --- | --- | --- | --- | --- |
| `PointXYZI` | adopted | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi-postgate/summary.md`：5 runs，median 1.85x，min 1.77x，max 1.87x | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi-postgate/evidence_doctor.md`：0 / 0 / 0 | positive | 纳入当前 adopted production behavior |
| `PointXYZRGB` | adopted | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb-postgate/summary.md`：5 runs，median 1.83x，min 1.75x，max 1.88x | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb-postgate/evidence_doctor.md`：0 / 0 / 0 | positive | 纳入当前 adopted production behavior |
| `PointXYZRGBA` | adopted | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba-postgate/summary.md`：5 runs，median 1.84x，min 1.79x，max 1.86x | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba-postgate/evidence_doctor.md`：0 / 0 / 0 | positive | 纳入当前 adopted production behavior |
| `PointXYZINormal` | scalar fallback | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-postgate/summary.md`：5 runs，median 1.00x，min 0.99x，max 1.03x | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-postgate/evidence_doctor.md`：Errors=0，Warnings=1，Suggestions=1 | neutral fallback confirmation | 不纳入 RVV；当前证据确认 gate 后公开入口等价回退 |

`PointXYZINormal` 的 post-gate warning 来自接近 1x 且 1/5 run 低于 1。因为此时 RVV build 已因 `sizeof(PointT) > 32` 回退到标量，该 warning 不再表示 RVV 候选可采纳；它只说明 fallback confirmation（回退确认）的性能接近标量，不能写成加速。

## 历史证据降级

| run label | 路径 | 结果 | 当前角色 |
| --- | --- | --- | --- |
| `eppd-full-scan-production-pointtype-xyzi-5run` | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzi/evidence_doctor.md` | median 1.83x，doctor 0 / 0 / 0 | historical；被 post-gate `xyzi` 取代 |
| `eppd-full-scan-production-pointtype-xyzrgb-5run` | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgb/evidence_doctor.md` | median 1.82x，doctor 0 / 0 / 0 | historical；被 post-gate `xyzrgb` 取代 |
| `eppd-full-scan-production-pointtype-xyzrgba-5run` | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzrgba/evidence_doctor.md` | median 1.82x，doctor 0 / 0 / 0 | historical；被 post-gate `xyzrgba` 取代 |
| `eppd-full-scan-production-pointtype-xyzinormal-5run` | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal/evidence_doctor.md` | median 1.09x，min 0.99x，max 1.45x；doctor 0 / 2 / 0 | historical unstable signal；触发 20-run 确认和 fallback gate |
| `eppd-full-scan-production-pointtype-xyzinormal-confirm20` | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-confirm20/summary.md`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-confirm20/evidence_manifest.json`、`test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production-xyzinormal-confirm20/evidence_doctor.md` | median 1.12x，min 0.88x，max 1.54x；5/20 低于 1；doctor 1 / 1 / 0 | historical rejection evidence；支撑 `sizeof(PointT) > 32` fallback gate |

## Evidence Doctor 处理

`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 的 post-gate doctor 均为 Errors=0 / Warnings=0 / Suggestions=0，可以作为本阶段 production public 性能证据。`PointXYZINormal` post-gate doctor 为 0 / 1 / 1；本阶段不把它解释为 RVV 弱收益，而是写成 fallback confirmation。接入前 20-run doctor 已出现 Error，因此 `PointXYZINormal` 的 RVV 路径不 clean-adopt。

## Optimization Matrix 更新

| candidate family | row source | point type / layout | correctness / fallback | board evidence | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| point type expansion | dense ordered | `PointXYZI` / `sizeof <= 32` / `RVVXYZAoSFloatLayout` | `segmentRvv` vs `segmentStd` pass | post-gate median 1.85x | 0 / 0 / 0 | adopted | none for dense single polygon |
| point type expansion | dense ordered | `PointXYZRGB` / `sizeof <= 32` / `RVVXYZAoSFloatLayout` | `segmentRvv` vs `segmentStd` pass | post-gate median 1.83x | 0 / 0 / 0 | adopted | none for dense single polygon |
| point type expansion | dense ordered | `PointXYZRGBA` / `sizeof <= 32` / `RVVXYZAoSFloatLayout` | `segmentRvv` vs `segmentStd` pass | post-gate median 1.84x | 0 / 0 / 0 | adopted | none for dense single polygon |
| wide-stride point type | dense ordered | `PointXYZINormal` / `sizeof > 32` | `segmentRvv` declines; public entry falls back | post-gate median 1.00x | 0 / 1 / 1 | rejected for RVV, adopted fallback | no auto-rerun; requires new wide-stride phase if revisited |

## Continue / Stop Decision

Phase 060 完成。当前 topic 内仍有一个未阻塞但独立的优化方向：`070-size-threshold-tuning`，用于检查 `indices_->size() >= 64` 是否仍是合适阈值。该方向不需要扩大 production API，也不依赖新的点型语义；板卡可用时可以按小规模 sweep（规模扫描）继续推进。

下一 phase 默认入口：`070-size-threshold-tuning`。
