#pragma once

/*
 * 本文件做什么：
 * 这是 recognition/surface_normal_modality RVV topic 的测试支撑聚合入口。
 * 当前短标识 snm 对应完整 topic `surface_normal_modality`。首阶段只覆盖
 * SurfaceNormalModality::processInputData() 里 depth-to-normal（深度到法线）
 * 和 quantize（方向量化）的 production-shaped diagnostic（生产形态诊断）。
 *
 * 证据边界：
 * 这里的 helper 只属于 test-rvv（专项测试资产）。它复刻 production 中
 * computeAndQuantizeSurfaceNormals2() 的 depth image（深度图）语义，但还没有
 * 修改 production（生产源码）或证明真实 processInputData() dispatch（分流逻辑）。
 */

#include "impl/snm_surface_normal.hpp"
