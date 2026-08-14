/*
 * 本文件做什么：
 * 这是 transformation_estimation_svd 的 test-rvv support 聚合入口。`tesvd` 是
 * transformation_estimation_svd 的短标识，供测试和 bench 使用稳定 include 路径。
 *
 * 证据边界：
 * 本目录只包含 test-only diagnostic（测试专用诊断）代码。它不会修改 production
 * dispatch（生产分流），也不能把 dense ordered-cloud-pair（稠密顺序点云对）候选直接外推到
 * indices（索引路径）、correspondences（对应关系路径）或 `Scalar=double`。
 */

#pragma once

#include "impl/tesvd_candidates.hpp"
