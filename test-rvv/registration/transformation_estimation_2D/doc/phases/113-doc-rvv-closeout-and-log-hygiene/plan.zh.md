# Phase 113: doc-rvv closeout and log hygiene plan

## 阶段意图和边界

本阶段只做 `registration/transformation_estimation_2D` 的 production closeout hygiene（生产收尾卫生）：

- 把 `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` 从 freshness sync（新鲜度同步）升级为真正的 production doc closeout（生产长期文档收尾）。
- 停止跟踪 topic 内已经被 Git 追踪的 generated log（生成日志），保留本地文件，由 `test-rvv/.gitignore` 接管。
- 刷新 phase index、roadmap、matrix、Handoff 和验证记录，让下一轮短 prompt 能恢复到当前事实。

本阶段不修改 production source（生产源码），不新增 RVV candidate，不回滚 Phase 091 / Phase 112 已采纳的 exact gate，不把 Phase 103/104 guarded probe 或 Phase 106 generic widening 写成 adopted。

## 当前状态清单

| area | 当前事实 | 本阶段动作 |
| --- | --- | --- |
| production truth | 当前生产补丁已由 `d40e67467 registration: adopt RVV 2D source-indexed PointXYZI path` 保留；采用 ordered generic、source-indexed exact `PointXYZ`、source-indexed exact `PointXYZI`、dual-indexed exact `PointXYZ`。 | 文档只写这些 adopted / retained 行为。 |
| `doc-rvv` | 上次提交前只做了小幅 freshness sync，未完整执行 `Production Doc Closeout Gate`。 | 大改长期文档，补当前采用方式、证据链、Traceability Map、VL chunk 示例、fallback 矩阵和未采纳原因。 |
| source-indexed generic | Phase 103/104 是 guarded probe；Phase 106 representative 20-run negative；Phase 111 Normal 类不接入。 | 写成 rejected / not planned，不写成 adopted。 |
| generated logs | 15 个 TE2D `log/**` 文件仍被 Git 跟踪，复跑后出现 tracked modifications；`.gitignore` 已有 topic-specific `registration/transformation_estimation_2D/log/**`。 | `git rm --cached` 这些 tracked log 文件，保留本地文件，不提交 raw logs。 |
| unrelated dirty | 工作区还有 SVD scale、surface、library-screening 等无关修改。 | 不读取为当前事实、不回滚、不 stage。 |

## Production Doc Closeout Gate

本阶段必须显式审计以下 area，不能以“长期文档已同步少量数值”关闭：

| area | required content | 完成判据 |
| --- | --- | --- |
| 当前状态 | production-adopted 范围一句话，列出真实覆盖入口。 | 表中 row source / 点型 / `Scalar` 与源码 gate 一致。 |
| 函数语义 | 公开入口、`ConstCloudIterator` 标量路径、centroid / demean / correlation / 2D solve / matrix 输出。 | 不看源码也能理解标量流程。 |
| 当前采用的优化方式 | dispatch / fallback、layout gate、两遍 RVV accumulator、VL chunk 内部流程、标量 tail。 | 有采用理由和边界，不复制阶段流水。 |
| 范围决策表 | ordered、source-indexed、dual-indexed、correspondence、generic / Normal / double / custom 的 adopted / rejected / scalar-only。 | 未覆盖范围是一等公民。 |
| Traceability Map | production helper、public entry、test、bench、script、summary、phase、evaluation 和长期文档互相定位。 | 表中每行有 evidence role。 |
| 数值算例 / VL chunk | 用小型点对解释两遍 chunk 如何得到 centroid 和 2x2 `H`。 | 读者能手工对齐一次 chunk 或单点贡献。 |
| Bench 与 Evidence | QEMU、asm、board repeated、Doctor、registry 和提交边界。 | QEMU 不写性能结论；board 数值与当前 phase 一致。 |
| 正确性与高效性证据链 | correctness、path / asm、performance、boundary、risk 分层。 | EvidenceDecision 不超过证据范围。 |
| 生产 closeout | 文件、helper、dispatch、public API、证据、回滚边界。 | 说明没有 public API 变化，fallback 保留原标量语义。 |

## Log Hygiene Gate

| 检查 | 判据 |
| --- | --- |
| tracked log list | `git ls-files test-rvv/registration/transformation_estimation_2D/log` 中不再出现 generated summary / asm / doctor 文件。 |
| local file preservation | `git rm --cached` 只从索引移除，不删除本地 evidence files。 |
| ignore behavior | `git status --short --untracked-files=all -- test-rvv/registration/transformation_estimation_2D/log` 不再列出这些日志为 untracked。 |
| commit boundary | 本阶段提交只包含文档、phase metadata、ignore / index 删除；不包含 raw/generated log additions。 |

## 执行动作

1. 新建本 phase plan。
2. 重写 `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md`，按 closeout gate 补齐长期生产事实。
3. 同步 `doc/phases/README.zh.md`、`doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md`，记录 Phase 113 与“不再推进 Normal”的正式状态。
4. 用 `git rm --cached` 移除 15 个 tracked generated log 文件。
5. 写 `result.zh.md`，回填 doc closeout、log hygiene、Evidence Doctor / registry 和提交边界。
6. 更新当前 Handoff YAML / Markdown。

## 验证命令

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
python3 -c 'import yaml; yaml.safe_load(open("tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.yaml")); print("ok")'
git status --short --untracked-files=all -- test-rvv/registration/transformation_estimation_2D/log
git ls-files test-rvv/registration/transformation_estimation_2D/log
git diff --check -- doc-rvv/registration/transformation_estimation_2D-RVV.zh.md test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D
```

## 继续 / 停止条件

本阶段完成后，默认创建一个 follow-up topic-only commit，提交长期文档 closeout、phase / roadmap / matrix 同步和 tracked-log 删除。若 `evidence_status` 出现 stale / unregistered change、YAML parse 失败、artifact scan 显示日志仍未被 ignore，或 staged set 混入无关 topic，则停止并先修复，不提交。
