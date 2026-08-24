/*
 * 本文件做什么：
 * 这是 point_coding topic 的稳定聚合入口。测试和 bench 只 include 这个头；
 * 具体 fixture（测试输入）、reference（标量参考）和 candidate（候选实现）
 * 放在 include/impl 中，避免入口源文件直接依赖内部拆分细节。
 *
 * 证据边界：
 * 这里的 helper 都是 test support（测试支撑代码），用于 component
 * ablation（组件消融）。它们不能证明 PointCoding 的 production dispatch
 * （生产分流）已经接入 RVV。
 */

#pragma once

#include "impl/point_coding_support.hpp"
