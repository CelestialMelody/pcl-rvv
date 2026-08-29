# Benchmark And Evidence

## Bench 入口

`src/bench_ism.cpp` 输出五条 case：

| case | 计时边界 | 输入规模默认值 | 证据角色 |
| --- | --- | --- | --- |
| `descriptor_cluster_distance` | 单个 descriptor 对所有 cluster center 的平方 L2 distance | `--clusters 184`、`FeatureSize=153` | diagnostic |
| `descriptor_batch_assignment` | 多个 descriptor 的 `descriptor_sum` gate + 最近 cluster assignment | `--descriptors 512`、`--clusters 184` | production-shaped diagnostic |
| `public_find_objects_descriptor_assignment` | 真实 `findObjects()` 公开入口和 production helper；不含训练和文件 I/O | `--descriptors 512`、`--clusters 184` | production direct |
| `sigma_pairwise_max_dot` | 单个 training cloud 的 pairwise max-dot + sqrt | `--points 768` | diagnostic |
| `vote_density_gaussian_sum` | radiusSearch 结果上的 `strength * exp(-d/sigma^2)` 求和 | `--votes 8192` | diagnostic |

默认 diagnostic repeated 参数来自 `ISM_REPEATED_BENCH_ARGS`：

```bash
--case-filter all --clusters 184 --points 768 --votes 8192 --iterations 100 --warmup-iterations 5
```

当前 production direct 参数来自 `ISM_PUBLIC_ENTRY_BENCH_ARGS`：

```bash
--case-filter public_find_objects_descriptor_assignment --clusters 184 --descriptors 512 --iterations 80 --warmup-iterations 5
```

## 当前 production direct 证据

| 字段 | 值 |
| --- | --- |
| run label | `ism_phase020_public_entry_findobjects_production_direct_repeated` |
| summary | `log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md` |
| manifest | `log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_manifest.json` |
| Evidence Doctor | `log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_doctor.md` |
| case | `public_find_objects_descriptor_assignment` |
| evidence role | production direct（真实生产路径证据） |
| A/B boundary | `public_overload` |
| timer boundary | public `findObjects()` entry with deterministic test-only feature estimator; excludes training and file I/O |
| checksum | `semantic:public_votes=494:votes_match=True:peak_density_match=True:peak_fingerprint_match=True`，raw bit checksum `7645179244906730525` |
| board result | median `1.060x`，min `1.040x`，max `1.070x`，`B/A < 1` 为 `0/5` |
| decision bucket | `weak_positive` |
| Doctor result | `Errors=0 / Warnings=0 / Suggestions=2` |

## Evidence Doctor 和 registry

- Phase 000 diagnostic summary：`log/board/repeated_phase000_ism_local_formula_diagnostic/summary.md`
- Phase 010 production-shaped summary：`log/board/repeated_phase010_production_shaped_descriptor_diagnostic/summary.md`
- Phase 020 production direct summary：`log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md`
- registry：`log/evidence_registry.json`

Evidence Doctor（证据体检）出现 Error 时不能关闭阶段；Warning 必须解释；Suggestion 可写入 roadmap。
Phase 020 只有两个 Suggestion：缺少环境字段和 binary identity（二进制身份）字段。它们不阻塞当前弱正向
采纳，但如果后续出现方向反转、长尾或 reviewer 要求复核，应优先补 device、taskset、governor、freq、
temperature 和 binary hash。

## 提交边界

默认只提交 summary / manifest / doctor / registry 等摘要证据。raw board run logs、QEMU logs、build
目录、本机 `config.mk` 和私有 board 地址默认不提交。`run_*` 目录用于本地复核，不是默认提交候选。
