# Evidence Manifest 与命名合同

本文定义 Evidence Doctor（证据体检）相关的命名和 manifest（证据清单）合同。它解决两个问题：

1. topic-local wrapper（主题本地包装脚本）应该是什么、如何命名、输出到哪里。
2. Makefile、测试代码、分析脚本和 output summary（输出摘要）在命名 evidence 字段前，应使用哪些规范字段，避免字段名漂移导致检查不稳定。

`test-rvv/script/evidence_doctor.py` 是执行检查的通用脚本。若二者冲突，以本文和 `evidence-doctor.zh.md` 的字段语义为长期合同，并同步修正脚本。
`test-rvv/script/evidence_registry.py` 是可选但推荐的通用登记脚本，用于记录 evidence output（证据输出）文件的 size / mtime / hash 和 producer target（生成动作），帮助恢复和提交前发现人工复跑或日志覆盖导致的 stale 文档。

## topic-local wrapper 是什么

topic-local wrapper（主题本地包装脚本）不是一个已经自动存在的文件，而是一类脚本。它放在具体 topic 的 `script/` 目录下，负责把该 topic 的 raw log（原始日志）、Markdown summary、case label（用例标签）、checksum summary（校验和摘要）和 asm attribution（反汇编归属）翻译成通用 `evidence_manifest.json`。

例如下面只是推荐命名，不表示当前仓库已经有这个文件：

```text
test-rvv/<module>/<topic>/script/generate_<topic_token>_evidence_manifest.py
```

以 `transformation_estimation_point_to_plane_lls_weighted` 为例，`generate_teptplw_evidence_manifest.py` 是一个合理的未来文件名，因为该 topic 已经有 `teptplw` 短 token。但只有在实际为该 topic 做 wrapper 试点时，才创建它。

全局 doctor 不应该理解每个 topic 的私有命名。分工应是：

| 层级                                   | 职责                                                                               |
| -------------------------------------- | ---------------------------------------------------------------------------------- |
| topic-local wrapper                    | 理解当前 topic 的 case label、helper 名、raw log 格式、checksum 字段和反汇编符号。 |
| `evidence_manifest.json`             | 用规范字段表达可比较证据；它是全局 doctor 的主要输入。                             |
| `test-rvv/script/evidence_doctor.py` | 对规范 manifest 做通用数据契约检查、异常信号检测和诊断建议。                       |
| `evidence_doctor.md`                 | 输出 Errors / Warnings / Suggestions，供 summary、evaluation 和 Handoff 引用。     |

## 推荐文件命名

在 topic 目录内，推荐使用下面的固定文件名：

```text
<topic-output>/evidence_manifest.json
<topic-output>/evidence_doctor.md
<topic-output>/evidence_doctor.json        # 可选，供机器读取
test-rvv/<module>/<topic>/log/evidence_registry.json
```

如果同一 topic 有多个 run label（运行标签），放在对应 run 目录下：

```text
test-rvv/<module>/<topic>/log/board/<run-label>/evidence_manifest.json
test-rvv/<module>/<topic>/log/board/<run-label>/evidence_doctor.md
```

如果是 QEMU correctness（QEMU 正确性）或 checksum-only（仅校验和）证据，也可以放在 `log/qemu/<run-label>/` 下；但 QEMU timing（QEMU 计时）仍不能写成性能结论。

`evidence_registry.json` 默认由 `artifact_layout.evidence_registry_template` 解析到 topic 的 `log/` 根下，记录多个 run-labelled 目录和易覆盖 summary 的当前状态。它不保存 raw log 内容，只保存路径、元数据和 digest（摘要指纹）。registry 本身只有在长期文档引用且不含私有路径时才进入提交候选。

## 推荐脚本命名

Topic-local manifest 生成脚本使用：

```text
script/generate_<topic_token>_evidence_manifest.py
```

当脚本只服务当前 topic 的某一类证据，可以加后缀：

```text
script/generate_<topic_token>_board_evidence_manifest.py
script/generate_<topic_token>_rvv_ba_evidence_manifest.py
script/generate_<topic_token>_asm_evidence_manifest.py
```

不要把 topic-specific parser（当前主题特定解析器）放进全局 `test-rvv/script/`，除非它已经不依赖 topic 名、case label、helper 名、字段布局或反汇编符号。

## 推荐 Makefile target 命名

在 topic-local Makefile 中，target 已经处于 topic 目录作用域内，优先使用短而稳定的本地 target：

```make
make generate_evidence_manifest
make run_evidence_doctor
make record_evidence_state
make check_evidence_freshness
make evidence_status
```

如果同一个 Makefile 已经有多组 board / QEMU / RVV-vs-RVV 证据，可以使用更具体的 target：

```make
make generate_board_evidence_manifest
make run_board_evidence_doctor
make generate_rvv_ba_evidence_manifest
make run_rvv_ba_evidence_doctor
```

如果 target 位于共享 Make include 或跨 topic 脚本入口中，才使用 topic token 前缀，例如：

```make
make generate_teptplw_evidence_manifest
```

Makefile 变量命名建议：

```make
EVIDENCE_MANIFEST := <output>/evidence_manifest.json
EVIDENCE_DOCTOR_MD := <output>/evidence_doctor.md
EVIDENCE_DOCTOR_JSON := <output>/evidence_doctor.json
```

在共享上下文中再加 topic token 前缀：

```make
TEPTPLW_EVIDENCE_MANIFEST := ...
TEPTPLW_EVIDENCE_DOCTOR_MD := ...
```

## Evidence Registry 字段

`evidence_registry.json` 使用 JSON object（对象）顶层结构。它可以由 `test-rvv/script/evidence_registry.py record` 生成，也可以由 topic-local wrapper 写出等价字段：

```json
{
  "schema_version": 1,
  "updated_at_epoch": 0,
  "files": {
    "test-rvv/<module>/<topic>/log/board/<run-label>/summary.md": {
      "state": {
        "path": "...",
        "exists": true,
        "size": 0,
        "mtime_ns": 0,
        "sha256": "sha256:..."
      },
      "target": "collect_board_...",
      "backend": "board",
      "run_label": "...",
      "case_filter": "...",
      "evidence_role": "post_production_performance",
      "related": {
        "summary_path": "...",
        "manifest_path": "...",
        "doctor_path": "..."
      },
      "doc_refs": [],
      "freshness_state": "recorded"
    }
  },
  "runs": []
}
```

字段语义：

| 字段 | 含义 |
| --- | --- |
| `state.path` | 仓库相对证据路径。 |
| `state.size` / `state.mtime_ns` | 快速变化信号；只能提示可能变化。 |
| `state.sha256` | 内容变化判断的主依据；大型 raw log 可用 summary digest 替代，但必须写清策略。 |
| `target` / `backend` / `run_label` / `case_filter` | 恢复时定位这次证据如何生成。 |
| `evidence_role` | 例如 `qemu_smoke_only`、`pre_production_diagnostic`、`post_production_performance`、`strict_ab`。 |
| `related` | 指向 summary / manifest / doctor 等配套摘要产物。 |
| `doc_refs` | 已知引用该 run 的 phase result、evaluation、topic 文档或 Handoff。 |
| `freshness_state` | `recorded`、`fresh`、`unregistered_change`、`unregistered_file`、`manual_run_detected`、`stale_doc_pending_refresh` 或 `historical`。 |

恢复和提交前检查时，worker 应运行 `check` 或等价扫描：

- 登记文件的 `sha256`、size 或 `mtime_ns` 与当前文件不一致：标记 `unregistered_change`。
- 扫描到未登记的 summary / manifest / doctor / analyze log：标记 `unregistered_file` 或 `manual_run_detected`。
- 当前 registry 指向的 run 未被文档引用：标记 `stale_doc_pending_refresh`。

常驻 watcher 或 git hook 可以作为辅助，但不能作为唯一机制。官方 Make / script target 应主动 record；agent S0 恢复、phase loop 恢复和提交前应主动 check。

## Manifest 顶层结构

`evidence_manifest.json` 使用 JSON object（对象）顶层结构：

```json
{
  "schema_version": 1,
  "summary": {
    "title": "...",
    "evidence_role": "strict_ab",
    "summary_path": "...",
    "metadata": {
      "device": "...",
      "iterations": 20,
      "warmup_iterations": 5,
      "run_count": 5,
      "taskset": "...",
      "governor": "...",
      "freq": "...",
      "temperature": "...",
      "vlen": "...",
      "binary_hash": "sha256:..."
    }
  },
  "checks": {
    "strict_ab": true
  },
  "comparisons": []
}
```

字段命名使用 `snake_case`。不要把 Makefile 变量名、C++ enum 名、case label 片段或日志原文直接当成 manifest 字段名。wrapper 应把它们翻译成规范字段。

## Comparison 字段

每个 `comparisons[]` entry（对比项）至少包含：

| 字段              | 含义                                                                                 |
| ----------------- | ------------------------------------------------------------------------------------ |
| `name`          | 可读 case 名；可以来自原 summary，但不能作为唯一 metadata 来源。                     |
| `case_kind`     | 证据形态，例如`production_direct`、`production_shaped`、`component_ablation`。 |
| `evidence_role` | 结论角色，例如`strict_ab`、`diagnostic`、`mixed_boundary_cross_check`。        |
| `point_type`    | 点类型或点型组合。                                                                   |
| `size`          | 输入规模。                                                                           |
| `group`         | 可选；用于同组 outlier（离群）检测，例如同一 row source / point type family。        |
| `ba_values`     | repeated B/A 值；`>1` 表示 candidate 比 baseline 更快。                            |
| `baseline`      | baseline 侧 metadata。                                                               |
| `candidate`     | candidate 侧 metadata。                                                              |

如果只有单侧 Std/RVV speedup，没有 baseline / candidate 两侧 metadata，只能作为 summary-only 或 diagnostic 输入，不能写成 strict A/B。

## Baseline / Candidate 字段

strict A/B 或 production direct 默认要求 baseline 和 candidate 共享以下字段：

| 字段                | 含义                                                                                        |
| ------------------- | ------------------------------------------------------------------------------------------- |
| `boundary`        | 实现边界，例如 public overload、production helper、test support helper。                    |
| `wrapper`         | bench 或被测 wrapper 名。                                                                   |
| `row_source`      | 行来源策略，例如`full_cloud`、`source_indexed`、`dual_indices`、`correspondences`。 |
| `solve`           | 计时是否包含 solve 阶段。                                                                   |
| `checksum_policy` | checksum 覆盖对象，例如 matrix、normal equation、accepted points。                          |
| `timer_boundary`  | 计时边界，例如 estimate-only、include-setup、include-index-expand。                         |
| `gate`            | RVV gate 或同边界验收条件。                                                                 |
| `mask`            | finite mask 或 predicate 策略。                                                             |
| `reduction`       | reduction / staging 口径。                                                                  |

其它常用字段：

| 字段                | 含义                                           |
| ------------------- | ---------------------------------------------- |
| `checksum`        | 当前 checksum policy 下的 checksum 值。        |
| `asm_boundary`    | 反汇编归属边界或 hot symbol。                  |
| `rvv_instr_count` | 当前边界内 RVV 指令计数或 RVV line count。     |
| `std_ms`          | 可选；Std build 下该侧耗时，用于方向冲突诊断。 |
| `rvv_ms`          | 可选；RVV build 下该侧耗时，用于方向冲突诊断。 |

## Alias 只用于迁移

`test-rvv/script/evidence_doctor.py` 可以识别少量旧字段 alias（别名），例如：

| 规范字段              | 可兼容旧名示例                       |
| --------------------- | ------------------------------------ |
| `row_source`        | `rowSource`、`row_source_policy` |
| `timer_boundary`    | `timing_boundary`                  |
| `warmup_iterations` | `warmup_iters`、`warmup`         |
| `run_count`         | `runs`                             |
| `binary_hash`       | `binary_sha256`                    |

使用 alias 时，doctor 会输出 `noncanonical_field_alias` suggestion。这个 suggestion 不是阻塞项，但它表示 wrapper 或旧 summary 还没有完全归一化。

不要把 alias 当成长期合同。新 wrapper 和新测试资产必须输出规范字段。未知拼写错误，例如 `boundry` 或 `rowSoruce`，不应被猜测修正；strict A/B 下应触发缺字段 Error。

## Case label 字典

复杂 topic 不应只靠 case 名字符串推断证据。wrapper 应维护 case label 字典，把可读 label 翻译成 manifest metadata。例如：

```python
CASE_LABELS = {
    "weighted lls production-dispatch full-cloud pointnormal 262144": {
        "case_kind": "production_direct",
        "point_type": "pointnormal",
        "row_source": "full_cloud",
        "size": 262144,
        "boundary": "production_dispatch",
        "solve": True,
    }
}
```

如果 case label 字典无法识别某一行，wrapper 应把该 comparison 标记为 `evidence_role=summary_only_unknown` 或直接失败，不要猜测为 production direct。

## Wrapper 完成条件

一个 topic-local wrapper 交给 reviewer 前，应满足：

- 能从当前 topic 的 summary / raw log / checksum / asm 输入生成 `evidence_manifest.json`。
- manifest 使用本文规范字段，而不是 raw log 的任意字段名。
- 无法解析的 case label 会 fail closed（失败关闭）或降级为 `summary_only_unknown`。
- 运行全局 doctor 后生成 `evidence_doctor.md`。
- Handoff Packet 记录输入文件、输出 manifest、doctor report、Errors / Warnings / Suggestions 和处理动作。

## 当前没有 wrapper 时怎么办

如果某个 topic 还没有 `generate_<topic_token>_evidence_manifest.py`，可以先用：

```bash
python3 test-rvv/script/evidence_doctor.py \
  --summary-md <topic-output>/summary.md \
  --output <topic-output>/evidence_doctor.md \
  --fail-on never
```

这只是过渡方式。它会保留 `summary_only_metadata_missing` warning，提醒 reviewer 不能把旧 summary 当作完整证据。下一步应为该 topic 增加 wrapper，而不是扩大全局 doctor 去猜 topic 私有语义。
