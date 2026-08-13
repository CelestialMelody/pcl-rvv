# TEPTPLW Trace Summary

Input logs:
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run01_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run02_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run03_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run04_rvv.log (iterations=20, warmup=5)
- test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run05_rvv.log (iterations=20, warmup=5)

Checksum:
- all runs contain checksum rows: `yes`
- checksum sequences identical: `yes`
- final checksum: `5232.840035`

| case | runs | avg median ms | avg min ms | avg max ms | avg p10 ms | avg p90 ms | iter-median median ms | iter max max ms | avg values ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `weighted lls production-source-indices-detail block-fused-abcd-ilp pointnormal 262144` | 5 | 35.008155 | 33.790987 | 37.272969 | 34.173727 | 36.631267 | 33.761243 | 45.933169 | 35.008155, 34.747837, 33.790987, 37.272969, 35.668715 |
| `weighted lls production-source-indices-detail block-fused-abcd-ilp pointnormal 65536` | 5 | 8.077128 | 8.008686 | 8.085834 | 8.009845 | 8.082532 | 7.969622 | 9.431995 | 8.085834, 8.008686, 8.077580, 8.011583, 8.077128 |
| `weighted lls production-source-indices-detail block-fused-abcd-ilp pointxyz-to-pointnormal 262144` | 5 | 46.192100 | 32.849998 | 58.393524 | 33.719698 | 56.234386 | 38.645576 | 77.523114 | 35.024247, 52.995678, 32.849998, 46.192100, 58.393524 |
| `weighted lls production-source-indices-detail block-fused-abcd-ilp pointxyz-to-pointnormal 65536` | 5 | 7.909559 | 7.907128 | 8.042417 | 7.907817 | 7.990755 | 7.858413 | 11.052974 | 7.908851, 8.042417, 7.909559, 7.913261, 7.907128 |
| `weighted lls production-source-indices-detail block-fused-abcd-ilp pointxyz-to-pointxyzinormal 262144` | 5 | 50.587933 | 38.907111 | 53.569698 | 39.712467 | 52.903402 | 40.946293 | 89.976606 | 40.920501, 50.587933, 53.569698, 51.903958, 38.907111 |
| `weighted lls production-source-indices-detail block-fused-abcd-ilp pointxyz-to-pointxyzinormal 65536` | 5 | 7.931601 | 7.876021 | 8.121204 | 7.894861 | 8.106906 | 7.889455 | 9.035490 | 8.085459, 7.923120, 8.121204, 7.876021, 7.931601 |
| `weighted lls production-source-indices-detail component block-fused-abcd-ilp pointnormal no-solve 262144` | 5 | 38.389802 | 34.733462 | 55.762509 | 35.036891 | 49.980979 | 34.460127 | 67.004313 | 35.492034, 34.733462, 41.308685, 38.389802, 55.762509 |
| `weighted lls production-source-indices-detail component block-fused-abcd-ilp pointnormal no-solve 65536` | 5 | 8.020313 | 7.985129 | 8.304008 | 7.992188 | 8.204488 | 7.917830 | 12.575993 | 8.304008, 8.020313, 8.002777, 8.055207, 7.985129 |
| `weighted lls production-source-indices-detail component block-fused-abcd-ilp pointxyz-to-pointnormal no-solve 262144` | 5 | 33.204663 | 32.685948 | 45.607198 | 32.811720 | 42.584447 | 33.196298 | 77.278361 | 33.000379, 33.204663, 32.685948, 38.050321, 45.607198 |
| `weighted lls production-source-indices-detail component block-fused-abcd-ilp pointxyz-to-pointnormal no-solve 65536` | 5 | 8.067390 | 7.959812 | 8.168083 | 7.968761 | 8.158185 | 7.900434 | 11.989361 | 8.168083, 8.067390, 7.959812, 7.982185, 8.143337 |
| `weighted lls production-source-indices-detail component block-fused-abcd-ilp pointxyz-to-pointxyzinormal no-solve 262144` | 5 | 41.934602 | 33.571801 | 46.649816 | 35.465992 | 45.546058 | 35.021301 | 77.854410 | 43.890422, 46.649816, 38.307278, 33.571801, 41.934602 |
| `weighted lls production-source-indices-detail component block-fused-abcd-ilp pointxyz-to-pointxyzinormal no-solve 65536` | 5 | 8.138200 | 8.091997 | 8.519273 | 8.101832 | 8.370172 | 8.089811 | 11.526938 | 8.519273, 8.146521, 8.138200, 8.091997, 8.116585 |
| `weighted lls production-source-indices-detail component staged-gather pointnormal no-solve 262144` | 5 | 34.554384 | 33.834319 | 35.349349 | 34.068241 | 35.220095 | 34.490898 | 40.316389 | 35.349349, 34.419124, 35.026215, 33.834319, 34.554384 |
| `weighted lls production-source-indices-detail component staged-gather pointnormal no-solve 65536` | 5 | 7.676404 | 7.636756 | 7.966877 | 7.650388 | 7.852479 | 7.548430 | 13.282627 | 7.966877, 7.676404, 7.680881, 7.670837, 7.636756 |
| `weighted lls production-source-indices-detail component staged-gather pointxyz-to-pointnormal no-solve 262144` | 5 | 37.264900 | 36.536760 | 52.909521 | 36.634151 | 46.668625 | 36.782906 | 92.191134 | 36.780238, 52.909521, 37.307280, 36.536760, 37.264900 |
| `weighted lls production-source-indices-detail component staged-gather pointxyz-to-pointnormal no-solve 65536` | 5 | 8.167542 | 8.026671 | 8.254357 | 8.067099 | 8.239016 | 8.116791 | 10.522717 | 8.127741, 8.026671, 8.254357, 8.216005, 8.167542 |
| `weighted lls production-source-indices-detail component staged-gather pointxyz-to-pointxyzinormal no-solve 262144` | 5 | 36.476486 | 34.843724 | 44.305030 | 35.120285 | 42.434779 | 36.101961 | 93.926365 | 44.305030, 36.476486, 39.629403, 35.535126, 34.843724 |
| `weighted lls production-source-indices-detail component staged-gather pointxyz-to-pointxyzinormal no-solve 65536` | 5 | 8.059534 | 7.814166 | 8.208009 | 7.898977 | 8.162811 | 7.973394 | 11.124101 | 8.208009, 8.059534, 7.814166, 8.026194, 8.095014 |
| `weighted lls production-source-indices-detail staged-gather pointnormal 262144` | 5 | 46.686589 | 35.275971 | 62.496360 | 36.412545 | 60.611561 | 34.829819 | 77.072316 | 35.275971, 57.784362, 46.686589, 62.496360, 38.117405 |
| `weighted lls production-source-indices-detail staged-gather pointnormal 65536` | 5 | 7.821604 | 7.726476 | 7.981825 | 7.729902 | 7.941394 | 7.711911 | 9.189992 | 7.735042, 7.880748, 7.726476, 7.981825, 7.821604 |
| `weighted lls production-source-indices-detail staged-gather pointxyz-to-pointnormal 262144` | 5 | 39.148380 | 36.006024 | 43.765302 | 36.663117 | 41.979427 | 37.658606 | 90.004023 | 37.648757, 43.765302, 39.148380, 39.300614, 36.006024 |
| `weighted lls production-source-indices-detail staged-gather pointxyz-to-pointnormal 65536` | 5 | 8.154235 | 8.014910 | 8.366713 | 8.065579 | 8.297776 | 8.049123 | 11.298602 | 8.194371, 8.366713, 8.014910, 8.141583, 8.154235 |
| `weighted lls production-source-indices-detail staged-gather pointxyz-to-pointxyzinormal 262144` | 5 | 36.492782 | 36.002395 | 37.741173 | 36.054807 | 37.411727 | 36.648259 | 43.698974 | 36.133425, 36.492782, 37.741173, 36.002395, 36.917558 |
| `weighted lls production-source-indices-detail staged-gather pointxyz-to-pointxyzinormal 65536` | 5 | 8.133056 | 8.035471 | 9.649486 | 8.048631 | 9.043100 | 8.037395 | 11.195809 | 8.133520, 8.068370, 9.649486, 8.133056, 8.035471 |
