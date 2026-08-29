#pragma once

/*
 * 本文件做什么：
 * 这是 BRISK 2D keypoint topic 的测试支撑聚合入口。测试和 benchmark
 * 从这里进入 downsample reference（下采样参考链路）和 shared fixture
 * （共享输入构造），避免在每个源文件里复制尺度空间语义。
 *
 * 证据边界：
 * 这些 helper 只服务 test-rvv。真实 production（生产源码）是否接入 RVV
 * 仍以 `keypoints/src/brisk_2d.cpp`、生产直连测试、反汇编和板卡证据为准。
 */

#include "impl/brisk_2d_downsample_reference.hpp"
