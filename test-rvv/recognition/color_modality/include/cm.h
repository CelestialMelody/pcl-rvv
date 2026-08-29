#pragma once

/*
 * 本文件做什么：
 * 这是 recognition/color_modality RVV topic 的测试支撑聚合入口。当前覆盖
 * ColorModality 的 color quantize（颜色量化，把 RGB 映射到 8 个颜色 bin）、
 * 3x3 dominant filter（主 bin 滤波）和 production direct（真实生产路径）对拍。
 *
 * 证据边界：
 * 这里的 helper 只属于 test-rvv（专项测试资产）。这些 helper 属于 test-rvv（专项测试资产），用于复核当前生产源码中
 * PointXYZRGB 窄范围 RVV 路径的 correctness（正确性）和 bench（性能测试）边界。
 */

#include "impl/cm_color_quantize.hpp"
