# OpenNI2 Grabber Optimization Evidence

| candidate family | code path | correctness | board evidence | asm | doctor handling | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `depth_xyz_contiguous_rvv` | `openni2_grabber.cpp` production detail helper | pass；production-detail hook bitwise passed | production-detail median 1.19x, min 1.15x, max 1.22x | `vle16` / `vfcvt` / `vfmul` / `vmseq` / `vsse32` | production-detail Doctor 0/0/0 | adopted / completed |
| `mismatch_stride_mapping` | `fillXYZMappedRVV<PointXYZRGBA>` with `step > 1` | pass | median 0.98x, 3/5 below 1 | `vsse32` path exists | Doctor Error: degradation frequency | attempted, no production candidate |
| `rgb_overlay_contiguous_rvv` | `fillRGBOverlayRVV` | pass | median 1.02x, near threshold | `vlseg3e8` / `vsse32` | near-threshold suggestion | attempted, no production candidate |
| `ir_intensity_contiguous_rvv` | current candidate falls back to scalar | pass | median 1.01x, 2/5 below 1 | no IR RVV candidate expected | Doctor Error: degradation frequency | deferred; needs focused candidate before any production plan |

## 采用 / 暂缓理由

`depth_xyz_contiguous_rvv` 的接入后 production-detail 收益超过 positive bucket，且生产接入很窄：只覆盖 `convertToXYZPointCloud` 的 `PointXYZ` 连续 depth path，保留 resize / RGB / IR / legacy 路径边界。用户已确认有收益即可采纳，当前写成 adopted。

`mismatch_stride_mapping` 和 `rgb_overlay_contiguous_rvv` 都需要更复杂的 point stride / color store 维护成本，但收益接近 1.0 或方向摇摆；当前不满足 weak speedup（弱收益）接入条件。`ir_intensity_contiguous_rvv` 还没有真正 RVV candidate，Phase 000 的结果只说明把 IR case 纳入当前候选没有收益。

## 恢复条件

| candidate | resume condition |
| --- | --- |
| RGB overlay | 有新的 pack/store 实现，或 profile 显示 RGB overlay 是实际 bottleneck（瓶颈），再做 focused ablation。 |
| mismatch stride | 有 production 需求证明 depth/image mismatch 是常见路径，并能降低 strided store 成本。 |
| IR intensity | 先实现 focused RVV intensity candidate，并单独跑 correctness、asm、board repeated 和 Doctor。 |
| OpenNI legacy | OpenNI2 production probe 稳定后另开 legacy parity phase，不能把当前结果直接外推。 |
| public OpenNI2 entry | 当前交叉依赖未启用 OpenNI2；若要补 production-public 证据，需要 OpenNI2-enabled RISC-V 构建或等价设备对象 smoke。 |
