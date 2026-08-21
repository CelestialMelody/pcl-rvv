# image_depth optimization evidence

## 本文职责

本文把已经尝试、暂缓或拒绝的优化方式映射到代码、测试、bench、board、asm 和 Evidence Doctor。搜索空间和默认恢复队列见 `optimization-roadmap.zh.md`。

## 当前结论摘要

| 状态 | candidate |
| --- | --- |
| `adopted` | contiguous depth meters RVV |
| `adopted` | contiguous disparity RVV |
| `rejected for production RVV` | downsample depth `vlse16` RVV；production public entry keeps scalar fallback |
| `adopted` | downsample disparity `vlse16` RVV |
| `profile-gated deferred` | `fillDepthImageRaw()` RVV |
| `deferred` | OpenNI legacy parity |

## 优化方式总表

| candidate family | code path | correctness | bench / board | asm | doctor | decision boundary |
| --- | --- | --- | --- | --- | --- | --- |
| contiguous depth meters RVV | `include/image_depth.h::fillDepthMetersContiguousRVV` | `run_test_compare` | `depth_full_640x480` median 1.38x；padded median 1.21x | `vle16` / `vfmul` / masked `vse32` | depth cases Error=0 | production-shaped only |
| contiguous disparity RVV | `include/image_depth.h::fillDisparityContiguousRVV` | `run_test_compare` | `disparity_full_640x480` median 1.93x | `vle16` / `vfrdiv` / masked `vse32` | Error=0, Warning=1 | production-shaped only |
| downsample depth `vlse16` RVV | `include/image_depth.h::fillDepthMetersDownsampleRVV` | `run_test_compare` | median 1.17x | `vlse16` / `vfmul` / masked `vse32` | Error=0, Warning=1 | production-shaped only |
| downsample disparity `vlse16` RVV | `include/image_depth.h::fillDisparityDownsampleRVV` | `run_test_compare` | median 1.38x | `vlse16` / `vfrdiv` / masked `vse32` | Error=0, Warning=1 | production-shaped only |
| production contiguous depth RVV | `io/src/image_depth.cpp::fillDepthImageContiguousRVV` | `run_test_compare` production path hit | `prod_depth_full_640x480` median 1.42x；padded median 1.20x | production-linked `vle16` / `vfmul` / masked `vse32` | production-public Error=0 | adopted |
| production contiguous disparity RVV | `io/src/image_depth.cpp::fillDisparityImageContiguousRVV` | `run_test_compare` production path hit | `prod_disparity_full_640x480` median 1.80x | production-linked `vle16` / `vfrdiv` / masked `vse32` | Error=0, Warning=1 | adopted with variance warning |
| production downsample depth `vlse16` RVV | not connected in current production dispatch | `run_test_compare` verifies scalar fallback | `prod_depth_downsample_640x480_to_320x240` context median 0.99x，min 0.98x | not_applicable for production RVV | production fallback coverage, Error=0 | rejected for current production patch；fallback kept |
| production downsample disparity `vlse16` RVV | `io/src/image_depth.cpp::fillDisparityImageDownsampleRVV` | `run_test_compare` production path hit | `prod_disparity_downsample_640x480_to_320x240` median 1.34x | production-linked `vlse16` / `vfrdiv` / masked `vse32` | Error=0, Warning=1 long-tail | adopted with variance warning |
| `fillDepthImageRaw()` RVV | not implemented | not run | not run | not run | not run | deferred because full-size tight row uses `memcpy` and profile is missing |
| OpenNI legacy parity | not implemented | not run | not run | not run | not run | deferred until generic `image_depth.cpp` production direct is stable |

## 标量路径与 RVV 路径差异

标量路径逐像素读取 `uint16_t`，按 invalid predicate（无效值谓词）写 NaN 或 0，否则执行 `pixel * 0.001f` 或 `constant / pixel`。RVV path 把同一语义拆成 vector load（向量加载）、mask（掩码）、widen + convert（拓宽和转换）、multiply/divide（乘法 / 除法）和 masked store（带掩码写回）。downsample path 使用 `vlse16` 跨步加载，contiguous path 使用 `vle16` 连续加载。

## 结论边界

这些 candidate 已完成 bounded production probe，并已由用户确认接入有收益的优化。当前 adopted production behavior 是 depth contiguous、disparity contiguous 和 disparity downsample；depth downsample 已从当前生产 RVV 范围中排除并保持标量 fallback。长期生产文档为 `doc-rvv/io/image_depth-RVV.zh.md`，使用 production-public 板卡数据。
