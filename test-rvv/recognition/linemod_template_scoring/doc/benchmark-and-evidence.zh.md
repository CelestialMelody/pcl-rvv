# LINEMOD Template Scoring Benchmark 与证据

## Bench Label 与边界

| label | 证据角色 | 计时边界 | 当前结论 |
| --- | --- | --- | --- |
| `LINEMOD score accumulation` | production-shaped diagnostic（生产形态诊断） | 已 linearized map 到 `u16 score_sums` | Phase 020 median `0.862x`，attempted-negative |
| `LINEMOD score scan` | production-shaped diagnostic | threshold count、max 和候选 index append | Phase 010 median `0.601x`，attempted-negative |
| `LINEMOD energy map generation` | production-shaped diagnostic | synthetic quantized bytes 到默认合并 energy maps | Phase 030 median `0.954x`，attempted-negative |
| `LINEMOD linearized map copy` | production-shaped diagnostic | 单个 energy map 到 64 个 offset maps | Phase 040 median `2.160x`，partial-production-candidate |
| `LINEMOD full-chain total` | production-shaped diagnostic | energy、linearized copy、accumulation、scan 的 full-chain split | Phase 050 median `1.565x`，production-ready input |
| `LINEMOD production matchTemplates total` | production-public（真实公开入口） | 当前源码 `LINEMOD::matchTemplates` public entry | Phase 070 median `1.138x`，adopted |
| `LINEMOD production detectTemplates total` | production-public | 当前源码 `LINEMOD::detectTemplates` public entry | Phase 070 median `1.129x`，adopted |
| `LINEMOD production semi-scale detectTemplates total` | production-public | 当前源码 `LINEMOD::detectTemplatesSemiScaleInvariant` public entry | Phase 080 median `1.136x`，adopted |

## 当前 Production Evidence

Phase 070 使用 `BENCH_ARGS="4096 96 200 5"`，含 200 次计时迭代和 5 次 warm-up。板卡 summary 路径：

- `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-evidence-manifest.json`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/070-production-integration-loop/production-direct-repeated-evidence-doctor.json`

Evidence Doctor 结果为 `Errors=0 / Warnings=0 / Suggestions=0`。manifest 记录 binary hash：`sha256:cd16a4e705ba652a92718ebba7c25bfaac2543d93bca0fc3ded803552465b658`。

Phase 080 使用同样输入规模单独采集 semi-scale 公开入口。板卡 summary 路径：

- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-summary.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-manifest.json`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.md`
- `test-rvv/recognition/linemod_template_scoring/doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.json`

Phase 080 Evidence Doctor 结果同为 `Errors=0 / Warnings=0 / Suggestions=0`。manifest 记录 binary hash：`sha256:a11ed5192759f85aacab784a2af65b0326152e6974a53fcd453a8f1fd6cf2f36`。

## Evidence Registry

`test-rvv/recognition/linemod_template_scoring/log/evidence_registry.json` 记录 Phase 000-080 的 summary / manifest / doctor 文件状态。准备 closeout 或提交前运行：

```bash
make -C test-rvv/recognition/linemod_template_scoring check_production_direct_evidence_freshness
make -C test-rvv/recognition/linemod_template_scoring check_semiscale_production_direct_evidence_freshness
```

该检查要求登记过的证据文件未被覆盖，并且 summary / doctor / run label 能从 README、evaluation、phase result、正式 `doc-rvv` 或 queue 文档中找到引用。
