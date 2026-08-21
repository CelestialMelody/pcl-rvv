# OpenNI2 Grabber 测试总览

## 测试分类

| 类别 | 当前入口 | 证明范围 |
| --- | --- | --- |
| correctness aggregate（正确性汇总） | `make run_test_compare` | Std/RVV build 的 gtest 全部通过。 |
| QEMU smoke（QEMU 小型验证） | `make run_qemu_smoke` 或小迭代 `run_bench_rvv` | build、运行和日志形状；不支持性能结论。 |
| asm attribution（反汇编归属） | `make dump_bench_rvv` | RVV helper 产生预期 vector instruction（向量指令）。 |
| board smoke（板卡小型验证） | `make run_board_openni2_grabber_smoke` | 板卡可运行、Std/RVV checksum 对齐。 |
| board repeated（板卡重复采集） | `make collect_board_openni2_grabber_repeated` | 目标硬件性能分布。 |
| Evidence Doctor（证据体检） | `make run_board_openni2_grabber_evidence_doctor` | 基于 topic-local JSON manifest 检查异常模式和证据边界。 |
| production-detail（生产内部边界） | `make run_test_compare`、`make collect_board_openni2_grabber_production_repeated` | 接入后的 production helper correctness 和板卡性能。 |

## Target 粒度审计

| target 类别 | 当前状态 | 处理 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 覆盖 Phase 000 diagnostic 和 Phase 020 production-detail gtest。 |
| correctness aliases | adopted for current scope | production-detail hook 已新增；public-entry OpenNI2 test 因依赖缺失阻塞。 |
| bench diagnostic aliases | adopted | bench 支持 `--case-filter all` 和单 case label。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 不运行完整性能 compare。 |
| board smoke aliases | adopted | `run_board_openni2_grabber_smoke` 固定 10 iteration / 2 warmup。 |
| board repeated aliases | adopted | diagnostic 用 `collect_board_openni2_grabber_repeated`；production-detail 用 `collect_board_openni2_grabber_production_repeated`。 |
| doctor / registry aliases | adopted | diagnostic 和 production-detail manifest / Doctor / registry 均已接入。 |
| historical probe guarded aliases | not_applicable with evidence | 当前没有旧 production probe；Phase 020 是当前唯一生产接入闭环。 |

## 覆盖矩阵

| case | point type | depth/RGB/IR layout | correctness | bench | current decision |
| --- | --- | --- | --- | --- | --- |
| `xyz_depth_full_640x480` | `PointXYZ` | depth 同尺寸连续 | yes | yes | positive diagnostic |
| `prod_xyz_depth_full_640x480` | `PointXYZ` | production detail helper depth 同尺寸连续 | yes | yes | adopted |
| `rgb_template_point_types` | `PointXYZRGB` / `PointXYZRGBA` | depth + RGB 模板诊断，同尺寸 RGB + 整数 step depth mapping | yes | no | diagnostic-only |
| `xyzrgba_depth_mismatch_320_to_640` | `PointXYZRGBA` | depth 宽度小于 cloud 宽度 | yes | yes | neutral / unstable |
| `rgba_overlay_full_640x480` | `PointXYZRGBA` | RGB 同尺寸连续 | yes | yes | neutral |
| `xyzi_depth_ir_full_640x480` | `PointXYZI` | depth/IR 同尺寸连续 | yes | yes | neutral / unstable |

## Production Direct 缺口

当前没有真实 `OpenNI2Grabber` 设备对象 entry 的 production-public test；交叉 PCL 安装未启用 `HAVE_OPENNI2`，RISC-V 依赖树也没有 OpenNI2 头/库。Phase 020 已补 production-detail helper correctness、asm 和 board evidence；若要补完整 public-entry，需要 OpenNI2-enabled RISC-V 构建环境。
