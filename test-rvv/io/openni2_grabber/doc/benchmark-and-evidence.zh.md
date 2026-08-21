# OpenNI2 Grabber Benchmark and Evidence

## Bench CLI

`src/bench_openni2_grabber.cpp` 支持：

```bash
./bench_openni2_grabber_rvv --case-filter all --iterations 10 --warmup-iterations 2
```

`--case-filter` 可选择 `all` 或单个 case label。输出包含平均耗时、Total Time、checksum 和 pixels。checksum 用于确认 Std/RVV 输出一致；性能结论只采用板卡或目标硬件。

## Case 字典

| case | 输入 | 计时边界 | checksum |
| --- | --- | --- | --- |
| `xyz_depth_full_640x480` | 640x480 synthetic depth | depth projection 写 `PointXYZ` | xyz float bits |
| `prod_xyz_depth_full_640x480` | 640x480 synthetic depth | `openni2_grabber.cpp` production detail helper 写 `PointXYZ` | xyz float bits |
| `xyzrgba_depth_mismatch_320_to_640` | 320x240 depth 写 640 宽 cloud | 初始化 cloud + mismatch xyz 写入 | xyz + rgba |
| `rgba_overlay_full_640x480` | 640x480 RGB | RGB pack/store | xyz + rgba |
| `xyzi_depth_ir_full_640x480` | 640x480 depth + IR | scalar IR candidate | xyz + intensity |

## 当前证据

| evidence | path | result |
| --- | --- | --- |
| QEMU bench smoke | `log/qemu/run_bench_rvv.log` | 可运行；不作为性能证据。 |
| asm | `build/asm/riscv/bench_openni2_grabber_rvv.asm` | 可见 depth/RGB candidate 的 RVV 指令。 |
| board repeated summary | `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/summary.md` | `xyz_depth_full_640x480` median 1.21x；其它 case neutral / unstable。 |
| Evidence manifest | `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_manifest.json` | 记录 boundary、row source、checksum、run contract 和 repeated values。 |
| Evidence Doctor | `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_doctor.md` | manifest-based：Errors=2，Warnings=1，Suggestions=2。 |
| production-detail board repeated | `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/summary.md` | `prod_xyz_depth_full_640x480` median 1.19x，min 1.15x，max 1.22x。 |
| production-detail Evidence Doctor | `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=0。 |

## Evidence Doctor 边界

当前 Doctor 输入是 topic-local JSON manifest（证据清单）。Phase 000 diagnostic 可用 `make generate_board_openni2_grabber_repeated_evidence_manifest` 和 `make run_board_openni2_grabber_evidence_doctor` 重新生成；Phase 020 production-detail 可用 `make run_board_openni2_grabber_production_evidence_doctor` 重新生成。production-detail manifest 路径是 `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_manifest.json`。

## Evidence Registry

当前 topic 已接入 `make record_board_openni2_grabber_repeated_evidence_state`、`make record_board_openni2_grabber_production_evidence_state`、`make evidence_status` 和 `make check_openni2_grabber_production_evidence_freshness`。registry 路径是 `test-rvv/io/openni2_grabber/log/evidence_registry.json`，同时登记 diagnostic 和 production-detail summary / manifest / Doctor。

## 提交边界

默认不提交 raw logs、QEMU logs、board raw logs、build 输出、二进制和私有板卡配置。若用户要求提交 evidence logs，应先脱敏并作为单独 evidence commit 精确选择。
