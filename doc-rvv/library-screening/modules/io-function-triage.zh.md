# io 模块文件级筛查清单（全覆盖版，重评）

本版按与 `registration/surface/filters` 相同标准重评：全文件覆盖、实现优先、允许推翻旧结论；`3rdparty/**` 仅登记覆盖，不纳入候选。

## 1. 覆盖范围与口径

- 覆盖范围：`io/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `151`，已判定 `151`（`151/151` 全覆盖）。
- 目录拆分：`include` `95`，`src` `56`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 候选定义：仅 `high/mid` 计入候选。

## 2. 筛选出的文件清单（候选）

### 2.1 high 候选（11）

| file_path | 说明 |
| --- | --- |
| `io/src/obj_io.cpp` | 循环49处，编解码/IO转换计算密集，建议优先优化 |
| `io/include/pcl/compression/impl/entropy_range_coder.hpp` | 循环38处，编解码/IO转换计算密集，建议优先优化 |
| `io/src/ply_io.cpp` | 循环36处，编解码/IO转换计算密集，建议优先优化 |
| `io/src/pcd_io.cpp` | 循环31处，编解码/IO转换计算密集，建议优先优化 |
| `io/include/pcl/io/impl/vtk_lib_io.hpp` | 循环29处，编解码/IO转换计算密集，建议优先优化 |
| `io/src/debayer.cpp` | 循环12处，编解码/IO转换计算密集，建议优先优化 |
| `io/src/vtk_io.cpp` | 循环11处，编解码/IO转换计算密集，建议优先优化 |
| `io/src/openni_camera/openni_device.cpp` | 循环11处，编解码/IO转换计算密集，建议优先优化 |
| `io/src/openni2_grabber.cpp` | 循环10处，编解码/IO转换计算密集，建议优先优化 |
| `io/src/openni_grabber.cpp` | 循环10处，编解码/IO转换计算密集，建议优先优化 |
| `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` | 循环10处，编解码/IO转换计算密集，建议优先优化 |

### 2.2 mid 候选（48）

| file_path | 说明 |
| --- | --- |
| `io/src/openni_camera/openni_image_bayer_grbg.cpp` | 存在可向量化路径（循环19，数学项3），建议次优先 |
| `io/src/vtk_lib_io.cpp` | 存在可向量化路径（循环18，数学项108），建议次优先 |
| `io/include/pcl/io/impl/pcd_io.hpp` | 存在可向量化路径（循环17，数学项91），建议次优先 |
| `io/src/hdl_grabber.cpp` | 存在可向量化路径（循环16，数学项35），建议次优先 |
| `io/src/dinast_grabber.cpp` | 存在可向量化路径（循环12，数学项30），建议次优先 |
| `io/src/openni_camera/openni_driver.cpp` | 存在可向量化路径（循环12，数学项7），建议次优先 |
| `io/include/pcl/compression/organized_pointcloud_conversion.h` | 存在可向量化路径（循环10，数学项106），建议次优先 |
| `io/src/real_sense_grabber.cpp` | 存在可向量化路径（循环10，数学项36），建议次优先 |
| `io/src/openni2/openni2_device.cpp` | 存在可向量化路径（循环10，数学项8），建议次优先 |
| `io/include/pcl/io/impl/buffers.hpp` | 存在可向量化路径（循环10，数学项2），建议次优先 |
| `io/include/pcl/io/impl/lzf_image_io.hpp` | 存在可向量化路径（循环9，数学项78），建议次优先 |
| `io/src/image_grabber.cpp` | 存在可向量化路径（循环8，数学项107），建议次优先 |
| `io/src/oni_grabber.cpp` | 存在可向量化路径（循环8，数学项64），建议次优先 |
| `io/src/lzf.cpp` | 存在可向量化路径（循环8，数学项3），建议次优先 |
| `io/src/tim_grabber.cpp` | 存在可向量化路径（循环7，数学项8），建议次优先 |
| `io/src/ply/ply_parser.cpp` | 存在可向量化路径（循环7，数学项2），建议次优先 |
| `io/include/pcl/compression/impl/organized_pointcloud_compression.hpp` | 存在可向量化路径（循环6，数学项82），建议次优先 |
| `io/src/ensenso_grabber.cpp` | 存在可向量化路径（循环6，数学项65），建议次优先 |
| `io/src/image_depth.cpp` | 存在可向量化路径（循环6，数学项3），建议次优先 |
| `io/src/openni_camera/openni_depth_image.cpp` | 存在可向量化路径（循环6，数学项3），建议次优先 |
| `io/src/real_sense/real_sense_device_manager.cpp` | 存在可向量化路径（循环6，数学项2），建议次优先 |
| `io/src/image_yuv422.cpp` | 存在可向量化路径（循环6，数学项0），建议次优先 |
| `io/src/openni_camera/openni_image_yuv_422.cpp` | 存在可向量化路径（循环6，数学项0），建议次优先 |
| `io/include/pcl/compression/color_coding.h` | 存在可向量化路径（循环5，数学项46），建议次优先 |
| `io/src/image_rgb24.cpp` | 存在可向量化路径（循环5，数学项2），建议次优先 |
| `io/src/openni_camera/openni_image_rgb24.cpp` | 存在可向量化路径（循环5，数学项0），建议次优先 |
| `io/include/pcl/io/pcd_grabber.h` | 存在可向量化路径（循环4，数学项44），建议次优先 |
| `io/src/depth_sense/depth_sense_grabber_impl.cpp` | 存在可向量化路径（循环4，数学项28），建议次优先 |
| `io/src/ascii_io.cpp` | 存在可向量化路径（循环4，数学项27），建议次优先 |
| `io/src/vlp_grabber.cpp` | 存在可向量化路径（循环4，数学项5），建议次优先 |
| `io/src/openni2/openni2_device_manager.cpp` | 存在可向量化路径（循环4，数学项0），建议次优先 |
| `io/src/ifs_io.cpp` | 存在可向量化路径（循环3，数学项55），建议次优先 |
| `io/include/pcl/compression/impl/octree_pointcloud_compression.hpp` | 存在可向量化路径（循环2，数学项88），建议次优先 |
| `io/include/pcl/compression/point_coding.h` | 存在可向量化路径（循环2，数学项52），建议次优先 |
| `io/src/real_sense_2_grabber.cpp` | 存在可向量化路径（循环2，数学项25），建议次优先 |
| `io/src/robot_eye_grabber.cpp` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `io/src/lzf_image_io.cpp` | 存在可向量化路径（循环2，数学项17），建议次优先 |
| `io/src/pcd_grabber.cpp` | 存在可向量化路径（循环2，数学项11），建议次优先 |
| `io/include/pcl/io/pcd_io.h` | 存在可向量化路径（循环0，数学项307），建议次优先 |
| `io/include/pcl/io/ply_io.h` | 存在可向量化路径（循环0，数学项220），建议次优先 |
| `io/include/pcl/io/lzf_image_io.h` | 存在可向量化路径（循环0，数学项86），建议次优先 |
| `io/include/pcl/compression/octree_pointcloud_compression.h` | 存在可向量化路径（循环0，数学项42），建议次优先 |
| `io/include/pcl/io/vtk_lib_io.h` | 存在可向量化路径（循环0，数学项42），建议次优先 |
| `io/include/pcl/io/ascii_io.h` | 存在可向量化路径（循环0，数学项30），建议次优先 |
| `io/include/pcl/compression/organized_pointcloud_compression.h` | 存在可向量化路径（循环0，数学项29），建议次优先 |
| `io/include/pcl/io/point_cloud_image_extractors.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `io/include/pcl/io/auto_io.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `io/include/pcl/compression/entropy_range_coder.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |

## 3. 全量文件覆盖表（151/151）

| file_path | file_priority | 是否候选 | 判断依据 | impl关联文件 |
| --- | --- | --- | --- | --- |
| `io/include/pcl/compression/color_coding.h` | `mid` | 是 | 存在可向量化路径（循环5，数学项46），建议次优先 | `-` |
| `io/include/pcl/compression/compression_profiles.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/compression/entropy_range_coder.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `io/include/pcl/compression/impl/entropy_range_coder.hpp` |
| `io/include/pcl/compression/impl/entropy_range_coder.hpp` | `high` | 是 | 循环38处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/include/pcl/compression/impl/octree_pointcloud_compression.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项88），建议次优先 | `-` |
| `io/include/pcl/compression/impl/organized_pointcloud_compression.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项82），建议次优先 | `-` |
| `io/include/pcl/compression/libpng_wrapper.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/compression/octree_pointcloud_compression.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项42），建议次优先 | `io/include/pcl/compression/impl/octree_pointcloud_compression.hpp` |
| `io/include/pcl/compression/organized_pointcloud_compression.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项29），建议次优先 | `io/include/pcl/compression/impl/organized_pointcloud_compression.hpp` |
| `io/include/pcl/compression/organized_pointcloud_conversion.h` | `mid` | 是 | 存在可向量化路径（循环10，数学项106），建议次优先 | `-` |
| `io/include/pcl/compression/point_coding.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项52），建议次优先 | `-` |
| `io/include/pcl/io/ascii_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项30），建议次优先 | `io/include/pcl/io/impl/ascii_io.hpp` |
| `io/include/pcl/io/auto_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `io/include/pcl/io/impl/auto_io.hpp` |
| `io/include/pcl/io/buffers.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `io/include/pcl/io/impl/buffers.hpp` |
| `io/include/pcl/io/davidsdk_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/debayer.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/depth_sense/depth_sense_device_manager.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/depth_sense/depth_sense_grabber_impl.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/depth_sense_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/dinast_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/ensenso_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/file_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/file_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/fotonic_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/hdl_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/ifs_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/image_depth.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/image_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/image_ir.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/image_metadata_wrapper.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/image_rgb24.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/image_yuv422.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/impl/ascii_io.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/impl/auto_io.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/impl/buffers.hpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项2），建议次优先 | `-` |
| `io/include/pcl/io/impl/lzf_image_io.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项78），建议次优先 | `-` |
| `io/include/pcl/io/impl/pcd_io.hpp` | `mid` | 是 | 存在可向量化路径（循环17，数学项91），建议次优先 | `-` |
| `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` | `high` | 是 | 循环10处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/include/pcl/io/impl/synchronized_queue.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/impl/vtk_lib_io.hpp` | `high` | 是 | 循环29处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/include/pcl/io/io_exception.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/low_level_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/lzf.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/lzf_image_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项86），建议次优先 | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `io/include/pcl/io/obj_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/oni_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_convert.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_device.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_device_info.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_device_manager.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_frame_listener.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_metadata_wrapper.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_timer_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni2_video_mode.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2/openni_shift_to_depth_conversion.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni2_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_depth_image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_device.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_device_kinect.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_device_oni.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_device_primesense.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_device_xtion.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_driver.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_exception.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_image_bayer_grbg.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_image_rgb24.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_image_yuv_422.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_ir_image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_camera/openni_shift_to_depth_conversion.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/openni_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/pcd_grabber.h` | `mid` | 是 | 存在可向量化路径（循环4，数学项44），建议次优先 | `-` |
| `io/include/pcl/io/pcd_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项307），建议次优先 | `io/include/pcl/io/impl/pcd_io.hpp` |
| `io/include/pcl/io/ply/byte_order.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/ply/io_operators.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/ply/ply.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/ply/ply_parser.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/ply_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项220），建议次优先 | `-` |
| `io/include/pcl/io/png_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/point_cloud_image_extractors.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` |
| `io/include/pcl/io/real_sense/real_sense_device_manager.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/real_sense_2_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/real_sense_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/robot_eye_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/split.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/tar.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/tim_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/timestamp.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/vlp_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/vtk_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/include/pcl/io/vtk_lib_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项42），建议次优先 | `io/include/pcl/io/impl/vtk_lib_io.hpp` |
| `io/src/ascii_io.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项27），建议次优先 | `-` |
| `io/src/auto_io.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/compression.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/davidsdk_grabber.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/debayer.cpp` | `high` | 是 | 循环12处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/depth_sense/depth_sense_device_manager.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/depth_sense/depth_sense_grabber_impl.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项28），建议次优先 | `-` |
| `io/src/depth_sense_grabber.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/dinast_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项30），建议次优先 | `-` |
| `io/src/ensenso_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项65），建议次优先 | `-` |
| `io/src/hdl_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环16，数学项35），建议次优先 | `-` |
| `io/src/ifs_io.cpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项55），建议次优先 | `-` |
| `io/src/image_depth.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项3），建议次优先 | `-` |
| `io/src/image_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项107），建议次优先 | `-` |
| `io/src/image_ir.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/image_rgb24.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项2），建议次优先 | `-` |
| `io/src/image_yuv422.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项0），建议次优先 | `-` |
| `io/src/io_exception.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/libpng_wrapper.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/lzf.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项3），建议次优先 | `-` |
| `io/src/lzf_image_io.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项17），建议次优先 | `-` |
| `io/src/obj_io.cpp` | `high` | 是 | 循环49处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/oni_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项64），建议次优先 | `-` |
| `io/src/openni2/openni2_convert.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni2/openni2_device.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项8），建议次优先 | `-` |
| `io/src/openni2/openni2_device_info.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni2/openni2_device_manager.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项0），建议次优先 | `-` |
| `io/src/openni2/openni2_timer_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni2/openni2_video_mode.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni2_grabber.cpp` | `high` | 是 | 循环10处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/openni_camera/openni_depth_image.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项3），建议次优先 | `-` |
| `io/src/openni_camera/openni_device.cpp` | `high` | 是 | 循环11处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/openni_camera/openni_device_kinect.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni_camera/openni_device_oni.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni_camera/openni_device_primesense.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni_camera/openni_device_xtion.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni_camera/openni_driver.cpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项7），建议次优先 | `-` |
| `io/src/openni_camera/openni_exception.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni_camera/openni_image_bayer_grbg.cpp` | `mid` | 是 | 存在可向量化路径（循环19，数学项3），建议次优先 | `-` |
| `io/src/openni_camera/openni_image_rgb24.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项0），建议次优先 | `-` |
| `io/src/openni_camera/openni_image_yuv_422.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项0），建议次优先 | `-` |
| `io/src/openni_camera/openni_ir_image.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/openni_grabber.cpp` | `high` | 是 | 循环10处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/pcd_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项11），建议次优先 | `-` |
| `io/src/pcd_io.cpp` | `high` | 是 | 循环31处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/ply/ply_parser.cpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项2），建议次优先 | `-` |
| `io/src/ply_io.cpp` | `high` | 是 | 循环36处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/png_io.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `io/src/real_sense/real_sense_device_manager.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项2），建议次优先 | `-` |
| `io/src/real_sense_2_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项25），建议次优先 | `-` |
| `io/src/real_sense_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项36），建议次优先 | `-` |
| `io/src/robot_eye_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `-` |
| `io/src/tim_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项8），建议次优先 | `-` |
| `io/src/vlp_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项5），建议次优先 | `-` |
| `io/src/vtk_io.cpp` | `high` | 是 | 循环11处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io/src/vtk_lib_io.cpp` | `mid` | 是 | 存在可向量化路径（循环18，数学项108），建议次优先 | `-` |

## 4. 简要统计

- 模块统计：high `11`，mid `48`，low `92`。
- 候选占比：`59/151 = 39.1%`。
- include 口径：候选 `22/95`。
- src 口径：候选 `37/56`。
- thirdparty 口径：`0` 文件已登记，候选 `0`。
