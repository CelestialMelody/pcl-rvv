# lzf_image_io 测试总览

## 测试层级

| layer | command / target | role | 不证明 |
| --- | --- | --- | --- |
| correctness aggregate | `make run_test_compare` | 分别构建 Std/RVV，运行同一组 gtest 对拍 | 不证明真实性能，不覆盖文件读取或 LZF 解压 |
| QEMU bench smoke | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | 确认可执行、checksum 和日志格式 | 不作为性能排序 |
| asm attribution | `make dump_bench_rvv` | 确认 RVV 指令出现在候选边界 | 不说明实际耗时收益 |
| board smoke | `make check_board_ssh`、`make run_board_test fetch_board_logs` | 确认板卡可用和 gtest 在板卡通过 | 不替代 repeated performance |
| board repeated | `make run_board_lzf_repeated` | 生成 5-run summary、manifest、Evidence Doctor 和 registry | 仍是 production-shaped diagnostic，不是 production direct |
| production board repeated | `make run_board_lzf_yuv422_production_repeated` | 只跑生产 helper case，生成 production-detail 证据 | 不覆盖真实文件读取和 LZF 解压成本 |
| freshness | `make check_evidence_freshness` | 检查登记证据未被未登记覆盖 | 不检查未引用 raw log |
| production freshness | `make check_production_evidence_freshness` | 检查 phase 020 生产证据登记状态 | 不检查未引用 raw log |

## Target 粒度审计

| target 类别 | 当前状态 | 说明 |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 覆盖 Std/RVV 5 个 gtest。 |
| correctness aliases | not_applicable with evidence | 当前 gtest 数量少，未单独拆 target；用 gtest 名区分 candidate family。 |
| bench diagnostic aliases | adopted | `--case-filter depth_xyz_640x480`、`yuv422_planar_rgb_640x480`、`rgb_buffer_to_cloud_640x480` 可隔离候选。 |
| bench production aliases | adopted | `--case-filter yuv422_planar_rgb_production_640x480` 可隔离生产 helper。 |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 聚合 correctness 与 RVV bench smoke。 |
| board smoke aliases | adopted | `run_board_lzf_depth_xyz`、`run_board_lzf_yuv422_rgb`、`run_board_lzf_rgb_copy`、`run_board_lzf_smoke`。 |
| board repeated aliases | adopted | `run_board_lzf_repeated` 和 `run_board_lzf_yuv422_production_repeated` 执行 5-run 并刷新摘要证据。 |
| doctor / registry aliases | adopted | phase 000 和 phase 020 都有 manifest、Doctor、registry record 和 freshness target。 |
| historical probe guarded aliases | not_applicable with evidence | 当前没有历史 production probe 或回滚 target。 |

## 覆盖矩阵

| candidate | correctness | QEMU smoke | asm | board repeated | Doctor |
| --- | --- | --- | --- | --- | --- |
| `depth_xyz_rvv` | covered | covered | covered | covered | Suggestion: near threshold |
| `yuv422_planar_rgb_rvv` | covered，包括 production helper direct | covered | covered | covered，包括 production-detail repeated | no blocking finding；production Doctor 0/0/0 |
| `rgb_buffer_to_cloud_rvv` | covered | covered | covered | covered | Error + Warning |

## 边界

测试资产直接喂解压后的 buffer。Phase 000 属于 production-shaped diagnostic（生产形态诊断）；phase 020 的 `yuv422_planar_rgb_production_640x480` 使用生产 helper 和 `pcl::PointXYZRGB`，属于 production-detail（生产细节 helper）证据。两者都不能回答真实 PCLZF reader 在文件读取和 LZF 解压后的总收益。
