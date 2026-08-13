/*
 * 本文件做什么：
 * 这是 registration/correspondence_types 的 test-rvv support 聚合入口。
 * 测试和 bench 只 include 这个稳定入口，内部 candidate（候选实现）放在
 * include/impl/ 中，避免把测试支撑细节暴露给调用方。
 *
 * 证据边界：
 * 这里的 RVV helper 只服务测试和性能诊断，不是 production dispatch（生产分流）。
 * 它不能证明 production 头文件已经接入 RVV，也不能把 QEMU timing（QEMU 计时）
 * 写成目标硬件性能结论。
 */

#pragma once

#include "impl/correspondence_types_candidates.hpp"
