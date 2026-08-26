/*
 * 本文件做什么：
 * 这是 MomentInvariantsEstimation 的 test-rvv 聚合入口。测试和 bench
 * 通过这里使用同一组 reference（参考链路）和 candidate（候选实现）helper。
 *
 * 证据边界：
 * 这些 helper 只用于 diagnostic（诊断）和 component ablation（组件消融），
 * 不修改 production（生产源码）中的 moment_invariants.hpp。
 */

#pragma once

#include "impl/moment_invariants_reductions.hpp"
