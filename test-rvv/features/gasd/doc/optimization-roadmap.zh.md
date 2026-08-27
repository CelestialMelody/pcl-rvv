# Optimization Roadmap

## 当前边界

`gasd.hpp` 目前还停在 diagnostic 阶段，生产源码不改。Phase 000 的 fixed-grid histogram copy 已取得
weak-positive diagnostic（弱正向诊断）；Phase 010 的 shape sample projection staging 已取得
positive diagnostic（正向诊断）；Phase 020 的 color hue / hbin staging 已取得 positive diagnostic；
Phase 030 的 trilinear interpolation arithmetic / index staging（三线性插值算术 / 索引暂存）取得
positive diagnostic，但存在 run-to-run long-tail variance（跨 run 长尾波动）warning。Phase 040
加入 scalar flat histogram write（标量扁平直方图写回）后只剩 weak-positive diagnostic。Phase 050
改用 `std::vector<Eigen::VectorXf>` production-like layout（生产相似布局）后反转为 diagnostic-negative。
Phase 060 把 projection staging、trilinear Eigen write 和 shape copy 合并成 production-shaped diagnostic
后仍为 stable negative（稳定负向），board median 为 0.590x。这些结果只说明 test helper boundary
（测试 helper 边界）和 production-shaped diagnostic boundary（生产形态诊断边界）当前不支持 staged
shape family，不代表 production（生产源码）已经可接入或可直接拒绝。Phase 080 补充 public compute profile
（公开入口性能剖析），未改 production 的 shape / color 公开入口分别得到 median 1.220x / 1.110x；这是
build-level profile signal（构建层级性能信号），不能归因为当前手写 staged shape family。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| fixed-grid histogram copy | production 源码中 `copyShapeHistogramsToOutput` / `copyColorHistogramsToOutput` 的连续写回尾段 | shape / color 直拷贝 | 连续 load/store，易做 RVV copy | 只覆盖尾段，不代表完整 descriptor 主成本 | Std/RVV correctness、asm、board repeated | attempted / diagnostic-weak-positive | 000-current-state-and-gaps |
| shape sample projection | production 源码中 `p / max_coord / dbin` 的样本投影 | shape path | 逐样本热点可能更大 | interleaved interpolation 和 histogram 更新可能抵消收益 | component ablation、benchmark、board | attempted / diagnostic-positive | 010-shape-sample-projection-diagnostic |
| color hue projection | production 源码中 `max/min/diff_inv` 的 hue 分支 | color path | 逐样本 RGB 分支有独立 RVV 收益 | `max == min`、RGB tie-break、byte stride load 成本不能外推到 histogram 写回 | correctness、fallback、board | attempted / diagnostic-positive | 020-color-hue-diagnostic |
| trilinear interpolation arithmetic / index staging | production 源码中 `addSampleToHistograms` 的 `coords -= 0.5`、`floor`、`grid_idx/h_idx` 和 8 个 spatial weights | shape path | 改善每个样本的内层算术密度 | 不写 histogram，无法解释 scatter/write cost | component ablation、asm、board、Doctor | attempted / diagnostic-positive | 030-interpolation-ablation |
| histogram write probe | Phase 030 后的直接缺口 | shape path | 验证 `hists[...] += weight` 写回是否吞掉算术收益 | flat layout 不覆盖 production `Eigen::VectorXf` | same-chain correctness、QEMU smoke、asm、board repeated、Doctor | attempted / diagnostic-weak-positive | 040-histogram-write-probe |
| Eigen-backed histogram write probe | Phase 040 暴露的 flat-vs-Eigen mismatch（扁平布局和 Eigen 布局不一致） | shape path | 用 `std::vector<Eigen::VectorXf>` 复刻 production 写回容器，判断弱正向是否保留 | Eigen per-cell allocation、cache locality 和 `operator[]` 写回吞掉收益 | same-chain correctness、QEMU smoke、asm、board repeated、Doctor | attempted / diagnostic-negative | 050-eigen-backed-histogram-write-probe |
| production-shaped shape combined diagnostic | Phase 050 negative 后的组合边界审计 | shape path | 把 projection staging、trilinear Eigen write 和 shape copy 放到一个 test-only helper，判断上游收益能否抵消写回退化 | 仍不覆盖 public dispatch、transform 和 descriptor object state；当前已稳定负向 | correctness、QEMU smoke、asm、board repeated、Doctor | attempted / production-shaped-diagnostic-negative | 060-production-shaped-shape-combined-diagnostic |
| no-production closeout / profile recovery audit | Phase 060 stable negative 后的决策审计 | topic-local docs | 汇总不接入 production 的当前理由、profile 恢复条件和 bounded production probe 允许条件 | 文档动作不能替代 production evidence；必须避免把 diagnostic negative 写成全局 rejected | evaluation、phase result、matrix、roadmap、README、artifact tracking | adopted / no-production closeout | 070-no-production-closeout-profile-audit |
| public compute profile audit | 用户允许有价值的优化测试后补充完整入口 profile | shape / color public entries | 验证未改 production 的公开入口在 RISC-V / RVV build 下是否还有可测成本空间 | Std/RVV production source 相同，结果只能作为 build-level profile signal，不能替代手写候选族证据 | public bench repeated board、Doctor、mismatch audit | adopted / profile-only | 080-public-compute-profile-audit |
| quadrilinear color interpolation | public API 支持 `INTERP_QUADRILINEAR`，Phase 020 已有 hue staging | color path | 复用 hue/hbin 和 spatial weight 线索 | 默认 color interp 是 `INTERP_NONE`；hue 维度写回更复杂 | correctness、bench、board、Doctor | deferred / separate follow-up | 用户或 profile 指定 color quadrilinear 场景后再开 phase |

## 优先级和早停规则

fixed-grid copy 已取得 weak-positive diagnostic 证据，但它只覆盖尾段。shape sample projection、color
hue / hbin staging 和 trilinear arithmetic / index staging 已取得 positive diagnostic 证据。Phase 040
证明 flat histogram write 后仍有 weak-positive，但 Phase 050 证明 Eigen-backed write probe 为 negative。
Phase 060 证明 production-shaped shape combined diagnostic 仍为 stable negative。Phase 070 已把当前
staged shape family 收口为 no-production closeout，并记录恢复条件。Phase 080 的 public compute profile
给出稳定正向 build-level signal，但没有提供新的手写 RVV candidate。默认恢复动作现在仍是
`stop_for_user_review_no_production_closeout`。

## 默认恢复队列

| phase | status | reason | resume condition |
| --- | --- | --- | --- |
| 010-shape-sample-projection-diagnostic | attempted / diagnostic-positive | Phase 010 已闭合，board median 1.690x，Doctor Errors=0 | 无需恢复；当前证据见 `log/board/repeated_phase010_shape_projection_fast/` |
| 020-color-hue-diagnostic | attempted / diagnostic-positive | Phase 020 已闭合，board median 1.920x，Doctor Errors=0 | 无需恢复；当前证据见 `log/board/repeated_phase020_color_hue/` |
| 030-interpolation-ablation | attempted / diagnostic-positive | Phase 030 已闭合，board median 1.950x，Doctor Errors=0、Warnings=1；该阶段不写 histogram | 无需恢复；当前证据见 `log/board/repeated_phase030_trilinear_interpolation/` |
| 040-histogram-write-probe | attempted / diagnostic-weak-positive | Phase 040 已闭合，board median 1.060x，Doctor Errors=0；flat histogram 写回后收益大幅收窄 | 无需恢复；当前证据见 `log/board/repeated_phase040_histogram_write_probe/` |
| 050-eigen-backed-histogram-write-probe | attempted / diagnostic-negative | Phase 050 已闭合，board median 0.820x，Doctor Error=1 为 5/5 退化频率；不能直接推出 no-production | 无需恢复；当前证据见 `log/board/repeated_phase050_eigen_histogram_write_probe/` |
| 060-production-shaped-shape-combined-diagnostic | attempted / production-shaped-diagnostic-negative | Phase 060 已闭合，board median 0.590x，Doctor Error=1 为 5/5 退化频率；不能直接推出 rejected | 无需恢复；当前证据见 `log/board/repeated_phase060_shape_combined/` |
| 070-no-production-closeout-profile-audit | adopted / no-production closeout | Phase 070 已闭合当前 staged shape family 的不接入生产判断和恢复条件 | 默认停在用户审查；继续 production probe 需用户授权 |
| 080-public-compute-profile-audit | adopted / profile-only | Phase 080 已闭合，public shape / color median 分别为 1.220x / 1.110x，Doctor 均为 0E/0W/2S；不改变 no-production 判断 | 无需恢复；若继续深挖，先开 production segment profile |
| color quadrilinear follow-up | deferred / separate follow-up | 颜色四线性插值不属于当前 staged shape family closeout；需要明确输入场景或 profile 优先级 | 用户或 profile 指定后创建独立 phase |

## 与 optimization matrix 的关系

roadmap 记录长期搜索空间；matrix 记录本 phase 的尝试和证据状态。不要把一个 candidate 已经进 roadmap 当成已经值得生产接入。

## 与 evaluation 的关系

evaluation 负责当前决策，roadmap 负责下一步还能试什么。两者必须一致。
