# quantizable_modality RVV 生产说明

## 当前状态

`recognition/src/quantizable_modality.cpp` 的 `QuantizedMap::spreadQuantizedMap()` 已接入 production-detail（生产细节 helper）RVV 分流。当前采用的实现是：

- `spreadQuantizedMapStd()` 保留原标量两遍 OR window 语义。
- `spreadQuantizedMapRVV()` 在 `__RVV10__` 下处理默认 `spreading_size == 8` 的 byte-map spread。
- `QuantizedMap::spreadQuantizedMap()` 先尝试 RVV helper，失败后自然回退到 Std helper。

当前生产行为已经通过板卡重复测试采纳，文档以此作为长期维护事实。

## 函数入口作用

`QuantizedMap::spreadQuantizedMap()` 是 recognition 模块的公共 helper。它接收一个 quantized byte map，把每个位置附近的 quantized bin bit（量化桶 bit）传播到邻域，供 LINEMOD / DOTMOD modality 后续 template matching（模板匹配）使用。

当前已知 production 调用方包括：

- `ColorModality<PointInT>::processInputData()`
- `ColorGradientModality<PointInT>::processInputData()`
- `ColorGradientModality<PointInT>::processInputDataFromFiltered()`
- `SurfaceNormalModality<PointInT>::spreadFilteredQuantizedSurfaceNormalsStd()`

本主题证明公共 helper 本身的收益，不把该收益直接外推为所有 caller 的端到端收益。

## 标量路径

标量路径先创建同尺寸临时 map，再进行两遍 spread：

1. 横向 pass：每个 active pixel 读取同一行内连续 `spreading_size` 个 byte，按位 OR 后写到 `tmp_map(col + spreading_size / 2, row)`。
2. 纵向 pass：再从 `tmp_map(col, row)` 开始按 `width` 跨行读取 `spreading_size` 个 byte，按位 OR 后写到 `output_map(col, row + spreading_size / 2)`。

active 区域保持原边界：`row < height - spreading_size - 1` 且 `col < width - spreading_size - 1`。未覆盖区域仍依赖 `QuantizedMap` resize / 初始化行为，与原标量路径一致。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | adopted | public helper 只做 RVV 短路和 Std fallback，公开 API 不变 | `run_test_compare`、fallback test | 非 RVV 构建、非默认 spread 和小尺寸回退标量 |
| RVV 组织方式 | adopted | 两遍固定 8-wide byte OR 可以按 VL chunk 批量 load / OR / store | asm、board repeated | 当前只覆盖 `spreading_size == 8` |
| 输入布局 | adopted | `QuantizedMap` 是连续 byte 存储，适合 unit-stride load/store | `bench_qm.cpp`、checksum | 不涉及 point type 或 `Scalar` |
| 维护成本 | adopted | helper 小、fallback 清晰、test hook 能隔离路径命中 | Evidence Doctor 0/0/4 | 后续只继续独立 candidate |
| 替代方案 | deferred | 非默认 spread 泛化缺少热点证据 | roadmap / matrix | 需要 caller profile 或新 phase |

## 覆盖范围与 fallback

| 范围 | 当前状态 | 证据 / 原因 |
| --- | --- | --- |
| 默认 `spreading_size == 8` | adopted | correctness、asm 和 board repeated 均覆盖。 |
| `QuantizedMap` 连续 byte storage | adopted | helper 只读写 `unsigned char` 数据。 |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 未启用时只编译 Std helper。 |
| 非默认 spreading size | scalar fallback | `spreading_size=5` 的 gtest 覆盖该 fallback。 |
| 小尺寸 map | scalar fallback | width / height 不足时不进入 RVV helper。 |
| caller public entry 端到端收益 | unvalidated | 当前 bench 只测 helper，不测完整 modality。 |

## 详细设计

RVV helper 的组织方式是：

1. 计算 active width / height，保持和标量循环相同的边界。
2. 横向 pass 按 VL chunk 读取 `input_row + 0` 到 `input_row + 7`，连续执行 `vor`，写入临时 map 的半窗口偏移位置。
3. 纵向 pass 按 VL chunk 读取 `tmp_row + 0 * width` 到 `tmp_row + 7 * width`，执行相同 OR reduction（按位或规约），写入 output map。
4. 每个 chunk 使用 `__riscv_vsetvl_e8m2()` 处理 tail lanes（尾部向量通道），避免要求宽度是 VL 的整数倍。

该实现没有改变 caller 调用方式，也没有把非默认 spread 硬塞进 RVV 分支。

## Bench case 说明

| case | 入口 | 数据 | 计时边界 | 证明点 |
| --- | --- | --- | --- | --- |
| `shared_spread_320x240` | `QuantizedMap::spreadQuantizedMap()` | synthetic quantized byte map 320x240 | 横向 spread + 纵向 spread | 常规尺寸 helper 收益 |
| `shared_spread_641x481_tail` | 同上 | synthetic quantized byte map 641x481 | 同上，覆盖 RVV tail lanes | 非整齐宽度收益是否保持 |

## 测试、QEMU、反汇编和板卡证据

| 证据层 | 路径 | 结果 | 能证明什么 |
| --- | --- | --- | --- |
| correctness | `test-rvv/recognition/quantizable_modality/src/test_qm.cpp` | `run_test_compare` 3/3 | forced scalar / RVV 对拍、tail 和 fallback 语义 |
| asm | `test-rvv/recognition/quantizable_modality/Makefile` | `check_qm_rvv_asm` 通过 | RVV helper 命中 byte load / OR / store 指令 |
| board repeated | `test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/summary.md` | median `4.670x` / `4.770x`，`0/5` 退化 | 当前 helper RVV path 快于当前 helper scalar path |
| Evidence Doctor | `test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/evidence_doctor.md` | `Errors=0 / Warnings=0 / Suggestions=4` | 无阻塞异常，但环境 metadata 可补强 |

## 正确性与高效性证据链

当前证据链覆盖：

- correctness：RVV build 命中 RVV path，forced scalar / RVV 输出逐 byte 一致，非默认 spread 回退标量。
- performance：5-run repeated board 的 positive 结果，`shared_spread_320x240` median `4.670x`，`shared_spread_641x481_tail` median `4.770x`。
- boundary：只覆盖公共 helper 的默认 spread 8；性能结论来自板卡，不使用 QEMU timing。
- risk：caller 端到端收益未由本主题证明，后续应按 caller topic 单独取证。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `QuantizedMap::spreadQuantizedMap` | production helper | 公共 byte-map spread 入口 | recognition modality callers | Std / RVV helper | production boundary | `recognition/src/quantizable_modality.cpp` |
| `spreadQuantizedMapStd` | production Std helper | 标量事实来源 | public helper fallback | output map | fallback source of truth | 同上 |
| `spreadQuantizedMapRVV` | production RVV helper | 默认 spread 8 的两遍 byte OR | public helper RVV dispatch | output map | adopted production RVV path | 同上 |
| `test_qm.cpp` | correctness target | forced scalar / RVV 对拍 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/recognition/quantizable_modality/src/test_qm.cpp` |
| `bench_qm.cpp` | bench wrapper | production-detail board 入口 | repeated board target | summary / doctor / registry | performance input | `test-rvv/recognition/quantizable_modality/src/bench_qm.cpp` |
| `generate_qm_evidence_manifest.py` | analysis script | manifest 和 summary 生成 | `record_evidence_state_repeated` | evidence registry | evidence metadata | `test-rvv/recognition/quantizable_modality/script/generate_qm_evidence_manifest.py` |
| Phase 000 result | phase result | 生产接入审计 | phase doc | evaluation / closeout | adoption audit | `test-rvv/recognition/quantizable_modality/doc/phases/000-shared-spread-rvv/result.zh.md` |
| production-detail summary | evidence output summary | repeated board 摘要 | board repeated target | evaluation / long-term doc | board performance | `test-rvv/recognition/quantizable_modality/log/board/repeated_phase000_shared_spread_rvv/summary.md` |

## 后续方向

当前 adopted production behavior 已闭合。当前 topic 内不建议继续扩大：非默认 spread 缺少热点证据，caller public entry 端到端收益需要另开 topic，`QuantizedMap::getSubMap()` 属于另一个容器 helper。
