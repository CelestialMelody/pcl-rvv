# Phase 010 Result: colored-cloud-to-disparity

## 结论

本阶段完成 `PointXYZRGB` cloud -> disparity + RGB / mono 的 test-only diagnostic（测试专用诊断）。
正确性通过，板卡单次 diagnostic 显示弱正向：RGB dense 1.09x、mono dense 1.07x、RGB mixed-invalid
1.10x。该结果没有命中停止条件，但说明当前 colored path 的收益被 scalar color pack（标量颜色写出）
和重复 finite check（有限值检查）明显稀释。

本阶段不修改 production，不能作为 production evidence（生产证据）。默认下一阶段进入
`020-fused-colored-pack`，验证是否把 disparity RVV pass（视差 RVV 批处理）和颜色写出合并到一次
organized cloud scan（有组织点云顺序扫描）里。

## 计划动作回填

| action | 产物 / 命令 | 结果 | 状态 |
| --- | --- | --- | --- |
| 写 colored failing tests | `src/test_organized_pointcloud_conversion.cpp` | RED 阶段因 `makePointXYZRGBCloud` 和 colored diagnostic helper 缺失失败 | completed |
| 实现 PointXYZRGB diagnostic helper | `include/impl/opc_fixtures.hpp`, `include/impl/opc_candidates.hpp` | RGB literal 和 mono PCL scalar 对拍通过 | completed |
| 扩展 bench case | `src/bench_organized_pointcloud_conversion.cpp` | 新增 3 个 `PointXYZRGB` bench label 和 color checksum | completed |
| QEMU correctness | `make run_test_compare` | std/RVV 各 5 个 gtest 全部通过 | completed |
| 反汇编归属 | `make dump_bench_rvv` | 生成 `build/asm/riscv/bench_organized_pointcloud_conversion_rvv.asm`，RVV disparity hot region 仍可见 | completed |
| 板卡 bench + Doctor | `make board_smoke`, Evidence Doctor manifest | checksum 一致；Doctor `Errors=0, Warnings=12, Suggestions=0` | completed |

## 板卡结果

| case | Std ms/iter | RVV ms/iter | speedup | checksum |
| --- | ---: | ---: | ---: | --- |
| `pointxyz_disparity_dense_307k` | 13.1796 | 6.6297 | 1.99x | match |
| `pointxyz_disparity_mixed_invalid_307k` | 13.1300 | 6.6711 | 1.97x | match |
| `pointxyz_disparity_dense_1m` | 44.9207 | 22.5242 | 1.99x | match |
| `pointxyzrgb_disparity_rgb_dense_307k` | 19.5359 | 17.8591 | 1.09x | match |
| `pointxyzrgb_disparity_mono_dense_307k` | 20.7159 | 19.2708 | 1.07x | match |
| `pointxyzrgb_disparity_rgb_mixed_invalid_307k` | 19.6634 | 17.8992 | 1.10x | match |

## Evidence Doctor 处理

`log/board/evidence_doctor.md` 报告 `Errors=0, Warnings=12, Suggestions=0`。

已知 warning（警告）分两类：

- `low_run_count`：当前是单次 board diagnostic，只能支撑继续 / 停止诊断决策，不能写成稳定 production 性能。
- `contract_mismatch`：baseline 用 `pcl::isFinite`，candidate 用 `vfclass finite mask`。checksum 与 gtest 对拍证明当前输入语义一致，但这仍是 diagnostic mixed-boundary（混合边界诊断），不能外推到 production。

## EvidenceDecision

`PointXYZRGB` colored diagnostic 判为 `attempted_weak_positive_diagnostic`。它足以支持继续做更窄的
color-pack candidate，但不足以进入 production integration loop（生产接入闭环）。

`PointXYZ` 数字在本次重跑中提升到约 1.97x-1.99x，旧的 1.72x-1.74x 只保留为 historical evidence
（历史证据）。当前 truth 使用本文件和 `log/board/evidence_manifest.json` 的数值。

## 下一阶段

下一阶段默认入口：`020-fused-colored-pack`。

阶段问题：当前 colored helper 先跑 RVV disparity，再重新标量扫描 cloud 做 color pack。若 fused candidate
能复用同一批 finite mask 和 lane 顺序，可能减少一次全量 `pcl::isFinite` 和一个额外 scan；若仍只有
弱正向，则 colored production probe 应被推迟，优先转向 decode/backprojection diagnostic。
