# DOTMOD Template Matching Optimization Roadmap

## 当前边界

当前 topic 覆盖 `recognition/src/dotmod.cpp` 的 `pcl::DOTMOD::detectTemplates()`。
已采纳范围是 `QuantizedMap` row-major `uint8_t` 图像和模板数据上的滑窗匹配：
RVV 构建直接从原图窗口按行读取，用 byte AND（字节按位与）、nonzero mask（非零掩码）
和 `vcpop` 统计命中；非 RVV 构建继续走原 `getSubMap()` 标量路径。

本 topic 不覆盖 DOTMOD modality（模态）预处理、`QuantizedMap::getSubMap()` standalone
优化、真实 RGB-D workload 输入分布 profile（性能剖析）或其它公开 API。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `submap-direct-window-score-rvv` | 队列表和当前源码 shape scan | one window x one template x one modality diagnostic helper | 避免 submap 分配 / 拷贝，并用 RVV byte mask popcount 加速计数 | 不覆盖多模板 responses 分配和 detection 输出 | correctness、bench、asm、board、Evidence Doctor | adopted for diagnostic | `000-current-state-and-submap-direct-window` completed |
| `full-detecttemplates-shaped-diagnostic` | Phase 000 结果 | 多 modality、多 template、threshold detection 输出的 test-only 完整形态 | 判断 direct-window helper 放回完整检测循环后是否仍有收益 | synthetic maps/templates 不等于真实生产对象状态 | public-shaped smoke、bench、doctor、board | adopted as production probe input | `010-full-detecttemplates-shaped-diagnostic` completed |
| `production-direct-window-rvv` | Phase 010 结果 | real `DOTMOD::detectTemplates()` public entry | 在生产路径去掉 RVV 构建下的 per-window `getSubMap()` 并向量化计数 | 只覆盖当前 byte map 布局和测试输入边界 | production direct tests、asm、board repeated、doctor、registry | adopted | `020-production-integration-direct-window` completed |
| `response-buffer-reuse` | Phase 020 plan 和源码窗口循环 | real `DOTMOD::detectTemplates()` public entry | 避免每个 row/col 窗口重复构造 `responses` vector | 与 Phase 020 baseline 的同边界 RVV-vs-RVV A/B 只有历史摘要，最终采纳以 post-patch Std/RVV 为准 | production direct tests、asm、board repeated、doctor、registry | adopted | `030-response-buffer-reuse` completed |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000` | full detectTemplates-shaped diagnostic | 单窗口正向不能覆盖多模板、多模态和输出顺序 | full-shaped correctness、bench、board | completed |
| `010` | production direct probe | 完整形态诊断 median `2.124x`，支持进入有界生产接入 | 真实 `createAndAddTemplate()` + `detectTemplates()` correctness、asm、board | completed |
| `020` | response-buffer-reuse | 生产接入后仍看到窗口内 `responses` 分配；这是当前函数内独立且低风险的实现形态候选 | 生产直连重跑，确认 speedup 仍 positive 且 RVV 绝对时间未退化 | completed |
| `030` | no high-priority unblocked candidate | 当前生产公开入口 median `3.298x` 且无 Evidence Doctor Error/Warning；继续扩大将进入 modality、输入分布 profile 或 standalone `getSubMap()`，这些已超出本 topic 当前生产边界 | 另开 topic 或 profile 触发后重启 | not_applicable for this closeout |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| fused multi-template accumulation | 当前实现每个 template 独立 score 后进入 `responses`，融合多模板会改变寄存器压力、cache 行为和阈值输出审计成本；当前 production-public 证据已足够正向 | profile 显示 `detectTemplates()` 仍是主热点，且需要继续压低模板数较大场景 |
| threshold / detection output RVV | detection push 需要保持输出顺序，当前输出尾段不是性能证据中的主要风险 | 真实 workload 中 detection 数很大，并有输出保序 staging 设计 |
| `QuantizedMap::getSubMap()` standalone RVV | 当前 adopted path 在 RVV 构建中绕开 `getSubMap()`；standalone 容器 copy 优化不属于 `detectTemplates()` 生产路径的必要动作 | 其它 caller profile 指向 `getSubMap()` 本身 |
| DOTMOD modality preprocessing | 属于 `color_gradient_dot_modality` 或其它 modality topic，不应并入当前 source-file closeout | 对应 topic 或 profile 明确要求 |

## 默认恢复动作

`next_phase_default`: `ready_for_review`。

`unblocked_next_actions`: none inside current topic boundary。当前授权范围内的生产补丁、生产直连测试、
反汇编、板卡 repeated summary、Evidence Doctor、registry、evaluation、phase docs、长期 `doc-rvv`
和队列表已经闭合。继续优化需要新的 profile 或扩大到其它 topic / caller。
