# Trajkovic 3D Test Support Code Map

## 目录职责

| 路径 | 职责 | 证据角色 |
| --- | --- | --- |
| `include/trajkovic_3d.h` | topic 聚合头，暴露测试专用 helper。 | test support entry（测试支撑入口） |
| `include/impl/trajkovic_3d_response.hpp` | Std/RVV response helper、finite gate 和 RVV lane helper。 | diagnostic correctness / asm / board |
| `src/test_trajkovic_3d.cpp` | gtest correctness、public output 和 fallback 测试。 | correctness gate |
| `src/bench_trajkovic_3d.cpp` | diagnostic mode 和 public mode benchmark。 | performance wrapper |
| `script/generate_trajkovic_3d_evidence_manifest.py` | 从 repeated board raw logs 生成 summary 和 manifest。 | evidence manifest |
| `Makefile` | 构建、QEMU、asm、board、doctor 和 registry target。 | reproducibility |
| `board.mk` | 板卡运行参数。 | board execution |
| `doc/phases/**` | phase plan/result、matrix 和恢复入口。 | phase loop |
| `doc/*.zh.md` | topic-local doc suite。 | reviewability |
| `doc-rvv/keypoints/trajkovic_3d-RVV.zh.md` | adopted production 行为长期文档。 | production maintenance |

## 代码调用关系

```text
Detector::compute(output)
  -> TrajkovicKeypoint3D::detectKeypoints()
     -> production gate
        -> trajkovic3DFourCornersResponseRVV()   # adopted dense FOUR_CORNERS path
        -> trajkovic3DEightCornersResponseRVV()   # adopted dense EIGHT_CORNERS path
        -> original scalar response body         # fallback and non-adopted paths
     -> scalar NMS and output construction
```

测试专用链路：

```text
test_trajkovic_3d.cpp / bench_trajkovic_3d.cpp
  -> include/trajkovic_3d.h
     -> include/impl/trajkovic_3d_response.hpp
        -> computeFourCornersResponseStd/RVV()
        -> computeEightCornersResponseStd/RVV()
```

## 拆分审计

当前 topic 已使用 `src/`、`include/` 和 `include/impl/`。测试支撑代码没有旧 `test_support/` 目录；`include/impl/trajkovic_3d_response.hpp` 同时包含 reference、candidate 和 RVV lane helper，但文件规模和职责仍可审查，暂不继续拆分。若后续加入点类型扩展，应优先把 fixtures、response formulas 和 bench case registry 进一步拆分。
