# bilateral_upsampling 阶段索引

| phase | 状态 | 默认恢复动作 | 说明 |
| --- | --- | --- | --- |
| `000-current-state-and-diagnostic-staged-window` | done | 不接 production | staged-window-reduction 正确性通过但板卡退化。 |
| `010-column-stride-depth-direct-probe` | done | 进入 phase 020 | direct-depth 诊断正向，EvidenceDecision 为 `partial-production-candidate`。 |
| `020-column-stride-depth-production-probe` | done / historical | 进入 phase 030/040/050 复核 | production public 曾给出负向证据；后续 board run 继续用于刷新当前判断。 |
| `030-production-public-overhead-ablation` | done / historical | 进入 phase 040 | steady-state public shell 消融未形成稳定采纳证据。 |
| `040-production-detail-helper-only-ablation` | done / historical + unstable | 进入 phase 050 | helper-only 两次 bounded board run 方向冲突，Evidence Doctor `Errors=3`，不能采纳当前 helper。 |
| `050-production-detail-mask-chunk-ablation` | done / historical | 进入 phase 060 | local nan-mask k64 为 `0.98x/0.92x/1.01x`，Evidence Doctor `Errors=2`，仍不支持旧 family 采纳。 |
| `060-production-detail-color-gather-ablation` | done | 进入 phase 070 | color-gather bench-local helper 形成正向 precursor，Evidence Doctor `Errors=0`。 |
| `070-production-detail-color-gather-production-probe` | done / adopted | 维护 adopted closeout 资料 | production public / steady public 已转正，并已用户确认保留。 |
| `071-production-finite-mask-correctness-refresh` | done / superseded by phase 073 board refresh | 见 phase 073 | 修复 color-gather finite mask，使 RVV 同标量 `std::isfinite` 一样跳过 NaN 和 infinity；当时 QEMU Std/RVV 11/11 passed，asm 可见 `vfabs` / `vmflt`。 |
| `072-production-same-type-gate-alignment` | done / historical freshness | 见 phase 073 | 将 production RVV gate 收窄到 same-type RGB/RGBA；曾刷新 QEMU 11/11、asm、board 和 Evidence Doctor，后续由 phase 073 扩展。 |
| `073-cross-rgb-rgba-production-probe` | done / adopted refreshed | 若扩其它点型则另开 phase | 将 production RVV gate 扩展到 RGB/RGBA exact family；当前二进制 QEMU 13/13、asm、board 和 Evidence Doctor 均已刷新，public 为 `1.23x/1.09x/1.13x/1.10x/1.10x`，steady 为 `1.21x/1.09x/1.16x/1.09x/1.10x`。 |

当前 topic 已完成 production integration loop、helper-only 消融、mask/chunk 组件消融、color-gather 生产探针，以及交叉 RGB/RGBA production probe。当前实现族已经出现正向 RGB/RGBA exact-family production board 采用证据，并已被用户确认允许“有收益先接入”，进入 adopted 语义。phase 073 之后当前二进制的 board freshness 已刷新；后续只在需要扩其它点型 / layout / `Scalar` 时再开新 phase。
