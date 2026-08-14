/*
 * 本文件做什么：
 * 这是 registration/correspondence_rejection_poly 的 test-rvv support（RVV 测试支撑）
 * 聚合入口。测试和 bench 只 include 这个稳定入口；内部 reference（参考链路）
 * 和 candidate（候选链路）放在 include/impl/ 中，便于 reviewer 从一个入口定位
 * 当前诊断代码。
 *
 * 证据边界：
 * 这里的 helper 只服务 correctness（正确性）、QEMU smoke（仿真器小型验证）、
 * asm smoke（反汇编路径验证）和 bench smoke（性能日志形状验证）。它没有修改
 * production（生产源码），也不能证明真实 public entry（公开入口）已经命中 RVV dispatch
 * （RVV 分流）。
 */

#pragma once

#include "impl/correspondence_rejection_poly_candidates.hpp"
