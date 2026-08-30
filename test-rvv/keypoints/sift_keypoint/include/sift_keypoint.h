#pragma once

/*
 * 本文件做什么：
 * 这是 SIFT scale-space topic 的稳定测试入口。测试、bench 和诊断脚本只从这里
 * 进入 topic-local helper，避免每个源文件各自拼接同一套 hot loop 语义。
 *
 * 证据边界：
 * 这里的 helper 只服务 test-rvv/keypoints/sift_keypoint。它们用来钉住
 * computeScaleSpace / findScaleSpaceExtrema 这条诊断边界，不代表 production
 * 公开入口已经接入 RVV。
 */

#include "impl/sift_keypoint_scale_space.hpp"
