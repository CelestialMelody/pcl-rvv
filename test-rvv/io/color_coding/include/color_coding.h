#pragma once

/*
 * 本文件做什么：
 * 这是 color_coding topic 的稳定聚合入口。测试和 bench 只 include
 * 这个头；内部 reference（参考链路）和 candidate（候选链路）放在
 * include/impl 中，避免入口源文件直接依赖内部拆分细节。
 *
 * 证据边界：
 * 这里的 helper 都是 test support（测试支撑代码），不能证明
 * production dispatch（生产分流）已经接入 RVV。
 */

#include "impl/color_coding_support.hpp"
