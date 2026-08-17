# Phase 050 计划：production patch replay 与用户验证

## 阶段意图和边界

本阶段只复现 Phase 030 的临时 production integration patch（生产接入补丁），让用户检查真实的
`Standard` / `RVV` 分层和公开入口分流。当前 patch 必须保持未提交、可见、可检查；本阶段不自动采纳，
也不自动回滚。

允许修改：

- `registration/include/pcl/registration/correspondence_rejection_poly.h`
- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- 当前 topic 的 phase / Handoff / evidence registry 摘要。

不扩大到：

- public API 签名、其它 registration 入口、其它点类型或 `Scalar=double`；
- `doc-rvv` 长期生产主题文档；
- 其它 topic、agent instruction 或无关 dirty 文件；
- production commit。

## 当前 production patch 形态

```text
getRemainingCorrespondences(...)
  -> __RVV10__ 下尝试 getRemainingCorrespondencesRVV(...)
  -> RVV gate 不满足或 helper 返回 false
  -> getRemainingCorrespondencesStandard(...)
```

`Standard` 保留原标量主体。RVV 只批量计算采样 polygon 的 xyz 边长相似度；随机采样、计数、接受率、
histogram、Otsu 和最终按输入顺序输出继续保留在 helper 内的标量阶段。

## 验证顺序

1. `git diff --check` 和目标 production diff 审查。
2. `make -C test-rvv/registration/correspondence_rejection_poly run_test_compare`：QEMU Std/RVV
   correctness；固定样本和固定种子随机压力样本均必须通过。
3. `make -C test-rvv/registration/correspondence_rejection_poly run_board_test_smoke`：板卡
   correctness。
4. `ALLOW_QEMU_BENCH_COMPARE=1 make -C ... run_bench_production_direct_smoke`：
   QEMU production-direct 日志形状和 manifest；这是通用 QEMU bench smoke 保护，QEMU 计时不作性能结论。
5. `make -C ... run_board_bench_production_direct_repeated`：
   5-run 板卡 production-direct；生成 summary、manifest 和 Evidence Doctor。
6. `python3 test-rvv/script/evidence_registry.py check --registry ...`：确认新输出已经登记。
7. 检查 production symbol / hot region 的反汇编归属，再进入 PI5 用户检查点。

## 证据与暂停条件

- 历史 Phase 030 的 board production-direct 负向结果不能替代当前 patch 的新 run；新 run 完成前只标为
  historical。
- QEMU 只支持 correctness、路径和日志形状；性能决策只使用板卡。
- 5-run 后无论正向还是负向，都保留当前 patch并暂停：
  - 正向：`pending_user_confirmation_adopt_production`
  - 负向：`pending_user_confirmation_rollback`
- 用户未明确确认前，不创建 production commit、不更新 adopted production 文档、不回滚 patch。

## 完成判据

本阶段在用户检查点完成，不以自动采纳或自动回滚作为完成条件。必须留下：

- 当前 production diff；
- Std/RVV 与 board correctness 结果；
- production-direct summary / manifest / Evidence Doctor；
- Evidence registry freshness；
- 当前 Handoff Packet 和下一步用户动作。
