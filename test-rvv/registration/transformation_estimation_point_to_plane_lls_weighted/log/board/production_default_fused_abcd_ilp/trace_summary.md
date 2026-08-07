# TEPTPLW Trace Summary

Input logs:
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_default_fused_abcd_ilp/run01_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_default_fused_abcd_ilp/run02_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_default_fused_abcd_ilp/run03_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_default_fused_abcd_ilp/run04_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_default_fused_abcd_ilp/run05_rvv.log (iterations=20, warmup=5)

Checksum:
- all runs contain checksum rows: `yes`
- checksum sequences identical: `yes`
- final checksum: `2184.271025`

| case | runs | avg median ms | avg min ms | avg max ms | avg p10 ms | avg p90 ms | iter-median median ms | iter max max ms | avg values ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `weighted lls production-default full-cloud fused-abcd-ilp pointnormal 262144` | 5 | 19.550460 | 19.519066 | 24.223807 | 19.529394 | 22.438944 | 19.509770 | 36.145628 | 19.550460, 19.544885, 24.223807, 19.761650, 19.519066 |
| `weighted lls production-default full-cloud fused-abcd-ilp pointxyz-to-pointnormal 262144` | 5 | 17.934504 | 17.809369 | 18.044780 | 17.842342 | 18.038421 | 17.832103 | 20.879183 | 18.044780, 18.028882, 17.809369, 17.891801, 17.934504 |
| `weighted lls production-default full-cloud fused-abcd-ilp pointxyz-to-pointxyzinormal 262144` | 5 | 17.930604 | 17.887966 | 18.037949 | 17.896934 | 17.996345 | 17.841686 | 20.081631 | 17.887966, 17.910387, 17.930604, 17.933939, 18.037949 |
