# lzf_image_io 优化证据

## Candidate Decision Index

| candidate | code path | test target | bench / board target | asm evidence | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `depth_xyz_rvv` | `include/impl/lzf_image_io_support.hpp` | `make run_test_compare` | `make run_board_lzf_repeated` case `depth_xyz_640x480` | `build/asm/riscv/bench_lzf_image_io_rvv.asm` | Suggestion: `near_threshold_ba` | `deferred-diagnostic` |
| `yuv422_planar_rgb_rvv` | `io/include/pcl/io/impl/lzf_image_io.hpp` + diagnostic support | `make run_test_compare` | `make run_board_lzf_yuv422_production_repeated` case `yuv422_planar_rgb_production_640x480` | `build/asm/riscv/bench_lzf_image_io_rvv.asm` | production Doctor 0/0/0 | `adopted weak-positive production behavior` |
| `rgb_buffer_to_cloud_rvv` | `include/impl/lzf_image_io_support.hpp` | `make run_test_compare` | `make run_board_lzf_repeated` case `rgb_buffer_to_cloud_640x480` | `build/asm/riscv/bench_lzf_image_io_rvv.asm` | Error: `ba_degradation_frequency` | `rejected-diagnostic-candidate` |

## Adopted / Attempted / Deferred / Rejected

- adopted：`yuv422_planar_rgb_rvv`。用户已确认“有收益即可采纳”，production-detail 5-run mean 1.0864x / median 1.0832x，Doctor 0/0/0，当前 patch 作为 adopted production behavior 保留。
- attempted：三条 test-only RVV candidate 均已完成 correctness、asm 和 board repeated diagnostic；YUV planar 已完成 production probe。
- deferred：`depth_xyz_rvv`，因为收益接近阈值且 invalid depth 语义仍需要标量 lane 修正。
- rejected：`rgb_buffer_to_cloud_rvv`，因为板卡 5/5 退化且 Doctor 报 Error。

## Production 接入条件

YUV planar 已补齐：

- production helper direct correctness test。
- `PointT` RGB member gate 和 fallback 测试。
- production asm attribution。
- board repeated summary 和 Evidence Doctor。
- PI5 用户检查点。

当前已完成用户确认采纳和 S11 production closeout，正式文档为
`doc-rvv/io/lzf_image_io-RVV.zh.md`。后续如果要扩展泛型 RGB/RGBA traits、depth16 xyz 或
Bayer debayer，需要另开 phase 并重新补 correctness、asm、board repeated 和 Evidence Doctor。
