# PCL RISC-V RVV 交叉编译指南

在阅读本文之前，请先阅读 [RISCV Environment Setup.zh.md](./RISCV Environment Setup.zh.md)，完成 RISC-V 交叉工具链、CMake、QEMU 或目标板运行环境的准备。

本文对应 `build_zlib.sh`、`build_lz4.sh`、`build_hdf5.sh`、`build_flann.sh`、`build_boost.sh`、`build_eigen.sh`、`build_gtest.sh`、`build_libpng.sh` 和 `build_pcl.sh` 的当前构建方式。示例中的路径变量故意留空，请按自己的机器或容器环境填写，不要直接复制成本机固定路径。

## 1. 目录与变量

建议把第三方依赖安装到同一个 RISC-V sysroot 风格目录下，把源码包和中间构建文件放到另一个工作目录下。PCL 源码目录可以单独放置。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export RV_AR="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ar"
export RV_RANLIB="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ranlib"

export VLEN=256
export DEFAULT_LMUL=2
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -fPIC -O3"
export EIGEN_ARCH_FLAGS="-mrvv-vector-bits=zvl -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=${DEFAULT_LMUL}"
```

变量含义：

- `RV_INSTALL_DIR`：所有交叉编译产物的安装前缀，例如 `${RV_INSTALL_DIR}/zlib`、`${RV_INSTALL_DIR}/boost`、`${RV_INSTALL_DIR}/pcl-rvv`。
- `RV_BUILD_DIR`：依赖库源码包和源码目录的工作目录。编译 PCL 时，该变量可直接指向 PCL 源码目录。
- `RISCV_TOOLCHAIN`：RISC-V 交叉工具链前缀目录。
- `VLEN`：目标机器的向量长度；Milk-V Jupyter 一类 RVV 256-bit 环境可使用 `256`。
- `DEFAULT_LMUL`：Eigen RVV 默认 LMUL，当前脚本使用 `2`。

容器或资源受限环境下，如果遇到 `Too many open files`，请提高宿主机和容器内的 `ulimit -n`；如果遇到 OOM，请降低并行度，例如把 `make -j16` 改成 `make -j8` 或更低。

## 2. 推荐构建顺序

PCL 依赖链比较长，建议按下面顺序构建：

| 顺序 | 脚本 | 产物目录 | 说明 |
| --- | --- | --- | --- |
| 1 | `build_zlib.sh` | `${RV_INSTALL_DIR}/zlib` | HDF5、Boost iostreams、libpng 等依赖 |
| 2 | `build_lz4.sh` | `${RV_INSTALL_DIR}/lz4` | FLANN/HDF5/PCL 链接时需要 |
| 3 | `build_hdf5.sh` | `${RV_INSTALL_DIR}/hdf5` | FLANN 依赖，启用 zlib，关闭 szip |
| 4 | `build_flann.sh` | `${RV_INSTALL_DIR}/flann` | PCL kdtree/search 等模块依赖 |
| 5 | `build_boost.sh` | `${RV_INSTALL_DIR}/boost` | PCL 核心依赖，显式接入 zlib |
| 6 | `build_eigen.sh` | `${RV_INSTALL_DIR}/eigen-rvv` | 使用 Eigen master 的 RVV10 支持 |
| 7 | `build_libpng.sh` | `${RV_INSTALL_DIR}/libpng` | PCL 可选图像相关依赖 |
| 8 | `build_gtest.sh` | `${RV_INSTALL_DIR}/gtest` | 测试程序需要；PCL 库本身不是必须 |
| 9 | `build_pcl.sh` | `${RV_INSTALL_DIR}/pcl-rvv` | 最终 PCL RVV 版本 |

## 3. zlib 1.3.2

zlib 使用自带 `configure`，通过环境变量识别交叉编译器。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_AR="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ar"
export RV_RANLIB="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ranlib"
export VLEN=256
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -fPIC -O3"

ZLIB_VERSION="zlib-1.3.2"
TAR_FILE="${ZLIB_VERSION}.tar.gz"
ZLIB_DOWNLOAD_URL="https://zlib.net/${TAR_FILE}"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

if [ ! -d "${ZLIB_VERSION}" ]; then
    [ -f "${TAR_FILE}" ] || wget "${ZLIB_DOWNLOAD_URL}"
    tar -xvf "${TAR_FILE}"
fi

cd "${ZLIB_VERSION}"

export CC="${RV_CC}"
export AR="${RV_AR}"
export RANLIB="${RV_RANLIB}"
export CFLAGS="${ARCH_FLAGS}"

./configure --prefix="${RV_INSTALL_DIR}/zlib" --shared
make -j16
make install

file "${RV_INSTALL_DIR}/zlib/lib/libz.so.1.3.2"
```

## 4. LZ4 1.10.0

LZ4 的 Makefile 可以直接接收 `CC`、`AR`、`RANLIB` 和 `CFLAGS`。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_AR="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ar"
export RV_RANLIB="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ranlib"
export VLEN=256
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -fPIC -O3"

LZ4_VERSION="lz4-1.10.0"
TAR_FILE="${LZ4_VERSION}.tar.gz"
LZ4_DOWNLOAD_URL="https://github.com/lz4/lz4/releases/download/v1.10.0/${TAR_FILE}"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

if [ ! -d "${LZ4_VERSION}" ]; then
    [ -f "${TAR_FILE}" ] || wget "${LZ4_DOWNLOAD_URL}"
    tar -zxvf "${TAR_FILE}"
fi

cd "${LZ4_VERSION}"

export CC="${RV_CC}"
export AR="${RV_AR}"
export RANLIB="${RV_RANLIB}"
export CFLAGS="${ARCH_FLAGS}"

make -j16 CC="${CC}" AR="${AR}" RANLIB="${RANLIB}" CFLAGS="${CFLAGS}"
make PREFIX="${RV_INSTALL_DIR}/lz4" CC="${CC}" AR="${AR}" RANLIB="${RANLIB}" CFLAGS="${CFLAGS}" install

file "${RV_INSTALL_DIR}/lz4/lib/liblz4.so.1.10.0"
```

## 5. HDF5 2.1.0

HDF5 需要显式指向 zlib，并关闭 Fortran、examples、tests 和 szip。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export VLEN=256
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -fPIC -O3"

HDF5_VERSION="hdf5-2.1.0"
TAR_FILE="${HDF5_VERSION}.tar.gz"
HDF5_DOWNLOAD_URL="https://github.com/HDFGroup/hdf5/releases/download/2.1.0/${TAR_FILE}"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

if [ ! -d "${HDF5_VERSION}" ]; then
    [ -f "${TAR_FILE}" ] || wget "${HDF5_DOWNLOAD_URL}"
    tar -zxvf "${TAR_FILE}"
fi

cd "${HDF5_VERSION}"
rm -rf build && mkdir build && cd build

cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
    -DCMAKE_C_COMPILER="${RV_CC}" \
    -DCMAKE_CXX_COMPILER="${RV_CXX}" \
    -DCMAKE_C_FLAGS="${ARCH_FLAGS}" \
    -DCMAKE_CXX_FLAGS="${ARCH_FLAGS}" \
    -DCMAKE_INSTALL_PREFIX="${RV_INSTALL_DIR}/hdf5" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${RV_INSTALL_DIR}/zlib" \
    -DZLIB_ROOT="${RV_INSTALL_DIR}/zlib" \
    -DCMAKE_EXE_LINKER_FLAGS="-L${RV_INSTALL_DIR}/zlib/lib -Wl,-rpath-link,${RV_INSTALL_DIR}/zlib/lib" \
    -DCMAKE_SHARED_LINKER_FLAGS="-L${RV_INSTALL_DIR}/zlib/lib -Wl,-rpath-link,${RV_INSTALL_DIR}/zlib/lib" \
    -DHDF5_BUILD_CPP_LIB=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DHDF5_BUILD_FORTRAN=OFF \
    -DHDF5_BUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DHDF5_ENABLE_ZLIB_SUPPORT=ON \
    -DHDF5_ENABLE_Z_LIB_SUPPORT=ON \
    -DHDF5_ENABLE_SZIP_SUPPORT=OFF \
    -DHDF5_ENABLE_SZIP_ENCODING=OFF

make -j16
make install

file "${RV_INSTALL_DIR}/hdf5/lib/libhdf5_cpp.so.320.1.0"
```

## 6. FLANN

FLANN 从源码仓库构建，链接前面安装的 zlib、LZ4 和 HDF5。当前脚本会把旧版 `cmake_minimum_required` 调整为 `3.5`，并屏蔽 FLANN 模板代码常见的 `-Woverloaded-virtual` 噪音。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export VLEN=256
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -fPIC -O3"
export CXX_WARNING_FLAGS="-Wno-overloaded-virtual"

export ZLIB_LIB="${RV_INSTALL_DIR}/zlib/lib"
export ZLIB_INC="${RV_INSTALL_DIR}/zlib/include"
export HDF5_LIB="${RV_INSTALL_DIR}/hdf5/lib"
export HDF5_INC="${RV_INSTALL_DIR}/hdf5/include"
export LZ4_LIB="${RV_INSTALL_DIR}/lz4/lib"
export LZ4_INC="${RV_INSTALL_DIR}/lz4/include"

FLANN_SOURCE_DIR="flann"
FLANN_GIT_URL="https://github.com/flann-lib/flann.git"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

[ -d "${FLANN_SOURCE_DIR}" ] || git clone "${FLANN_GIT_URL}" "${FLANN_SOURCE_DIR}"
cd "${FLANN_SOURCE_DIR}"

sed -i 's/cmake_minimum_required(VERSION [^)]*)/cmake_minimum_required(VERSION 3.5)/' CMakeLists.txt
rm -rf build && mkdir build && cd build

cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
    -DCMAKE_C_COMPILER="${RV_CC}" \
    -DCMAKE_CXX_COMPILER="${RV_CXX}" \
    -DCMAKE_POLICY_DEFAULT_CMP0074=NEW \
    -DCMAKE_POLICY_DEFAULT_CMP0135=NEW \
    -DCMAKE_FIND_ROOT_PATH="${RV_INSTALL_DIR}" \
    -DCMAKE_C_FLAGS="${ARCH_FLAGS} -I${LZ4_INC} -I${HDF5_INC} -I${ZLIB_INC}" \
    -DCMAKE_CXX_FLAGS="${ARCH_FLAGS} ${CXX_WARNING_FLAGS} -I${LZ4_INC} -I${HDF5_INC} -I${ZLIB_INC}" \
    -DCMAKE_EXE_LINKER_FLAGS="-L${LZ4_LIB} -L${HDF5_LIB} -L${ZLIB_LIB} -Wl,-rpath-link,${LZ4_LIB} -Wl,-rpath-link,${HDF5_LIB} -Wl,-rpath-link,${ZLIB_LIB}" \
    -DCMAKE_SHARED_LINKER_FLAGS="-L${LZ4_LIB} -L${HDF5_LIB} -L${ZLIB_LIB} -Wl,-rpath-link,${LZ4_LIB} -Wl,-rpath-link,${HDF5_LIB} -Wl,-rpath-link,${ZLIB_LIB}" \
    -DCMAKE_INSTALL_PREFIX="${RV_INSTALL_DIR}/flann" \
    -DCMAKE_PREFIX_PATH="${RV_INSTALL_DIR}/hdf5;${RV_INSTALL_DIR}/zlib;${RV_INSTALL_DIR}/lz4" \
    -DHDF5_ROOT="${RV_INSTALL_DIR}/hdf5" \
    -DZLIB_ROOT="${RV_INSTALL_DIR}/zlib" \
    -DLZ4_ROOT="${RV_INSTALL_DIR}/lz4" \
    -DHDF5_USE_STATIC_LIBRARIES=OFF \
    -DBUILD_PYTHON_BINDINGS=OFF \
    -DBUILD_MATLAB_BINDINGS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_DOC=OFF

make -j16
make install

file "${RV_INSTALL_DIR}/flann/lib/libflann_cpp.so.1.9.2"
```

## 7. Boost 1.88.0

Boost 使用 `bootstrap.sh` 生成 `b2`，再通过 `user-config.jam` 指定 RISC-V g++。`boost.iostreams` 需要通过 `-sZLIB_INCLUDE` 和 `-sZLIB_LIBPATH` 显式开启 zlib 支持。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export VLEN=256
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -fPIC -O3"

export ZLIB_LIB="${RV_INSTALL_DIR}/zlib/lib"
export ZLIB_INC="${RV_INSTALL_DIR}/zlib/include"

BOOST_VERSION="1.88.0"
BOOST_VERSION_UNDERSCORE="${BOOST_VERSION//./_}"
BOOST_SOURCE_DIR="boost_${BOOST_VERSION_UNDERSCORE}"
TAR_FILE="${BOOST_SOURCE_DIR}.tar.gz"
BOOST_DOWNLOAD_URL="https://archives.boost.io/release/${BOOST_VERSION}/source/${TAR_FILE}"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

if [ ! -d "${BOOST_SOURCE_DIR}" ]; then
    [ -f "${TAR_FILE}" ] || wget "${BOOST_DOWNLOAD_URL}"
    tar -zxvf "${TAR_FILE}"
fi

cd "${BOOST_SOURCE_DIR}"

./bootstrap.sh --prefix="${RV_INSTALL_DIR}/boost"

cat > user-config.jam << EOF
using gcc : riscv64 : ${RV_CXX} : <cflags>"${ARCH_FLAGS}" <cxxflags>"${ARCH_FLAGS}" ;
EOF

./b2 -j16 \
    --user-config=./user-config.jam \
    --build-dir=./build \
    toolset=gcc-riscv64 \
    --prefix="${RV_INSTALL_DIR}/boost" \
    architecture=riscv \
    abi=sysv \
    address-model=64 \
    target-os=linux \
    link=shared \
    threading=multi \
    runtime-link=shared \
    -sZLIB_INCLUDE="${ZLIB_INC}" \
    -sZLIB_LIBPATH="${ZLIB_LIB}" \
    -sZLIB_BINARY=z \
    install

file "${RV_INSTALL_DIR}/boost/lib/libboost_system.so.1.88.0"
```

## 8. Eigen RVV

Eigen 是 header-only，但仍建议通过 CMake 安装，以便生成 `Eigen3Config.cmake`，供 PCL 的 `find_package(Eigen3)` 使用。

当前脚本从 Eigen `master` 拉取最新代码；RVV10 支持位于 `Eigen/src/Core/arch/RVV10`，历史背景可参考 Eigen RVV1.0 支持相关讨论。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export VLEN=256
export DEFAULT_LMUL=2
export MARCH="rv64gcv_zvl${VLEN}b_zfh_zvfh"
export EIGEN_ARCH_FLAGS="-march=${MARCH} -mabi=lp64d -O3 -mrvv-vector-bits=zvl -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=${DEFAULT_LMUL}"

EIGEN_SOURCE_DIR="eigen"
EIGEN_GIT_URL="https://gitlab.com/libeigen/eigen.git"
EIGEN_BRANCH="master"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

[ -d "${EIGEN_SOURCE_DIR}" ] || git clone "${EIGEN_GIT_URL}" "${EIGEN_SOURCE_DIR}"
cd "${EIGEN_SOURCE_DIR}"
git fetch origin "${EIGEN_BRANCH}"
git checkout -B "${EIGEN_BRANCH}" "origin/${EIGEN_BRANCH}"

rm -rf build && mkdir build && cd build

cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
    -DCMAKE_C_COMPILER="${RV_CC}" \
    -DCMAKE_CXX_COMPILER="${RV_CXX}" \
    -DCMAKE_CXX_FLAGS="${EIGEN_ARCH_FLAGS}" \
    -DCMAKE_INSTALL_PREFIX="${RV_INSTALL_DIR}/eigen-rvv" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DEIGEN_BUILD_TESTING=OFF

make install -j16

find "${RV_INSTALL_DIR}/eigen-rvv" -name Eigen3Config.cmake -print
```

## 9. GoogleTest

GoogleTest 主要用于测试程序。PCL 库本身编译可以不依赖它。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export VLEN=256
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -O3 -fPIC"

GTEST_SOURCE_DIR="googletest"
GTEST_GIT_URL="https://github.com/google/googletest.git"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

[ -d "${GTEST_SOURCE_DIR}" ] || git clone "${GTEST_GIT_URL}" "${GTEST_SOURCE_DIR}"
cd "${GTEST_SOURCE_DIR}"

rm -rf build && mkdir -p build && cd build

cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
    -DCMAKE_C_COMPILER="${RV_CC}" \
    -DCMAKE_CXX_COMPILER="${RV_CXX}" \
    -DCMAKE_INSTALL_PREFIX="${RV_INSTALL_DIR}/gtest" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_C_FLAGS="${ARCH_FLAGS}" \
    -DCMAKE_CXX_FLAGS="${ARCH_FLAGS}" \
    -Dgtest_disable_pthreads=OFF

make -j16
make install

file "${RV_INSTALL_DIR}/gtest/lib/libgtest.so"
```

## 10. libpng 1.6.54

libpng 通过 autotools 构建，需要显式指定 zlib 的 include 和 lib 路径。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export RV_AR="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ar"
export RV_RANLIB="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-ranlib"
export VLEN=256
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -O3 -fPIC"

export ZLIB_LIB="${RV_INSTALL_DIR}/zlib/lib"
export ZLIB_INC="${RV_INSTALL_DIR}/zlib/include"

LIBPNG_VERSION="libpng-1.6.54"
TAR_FILE="${LIBPNG_VERSION}.tar.gz"
LIBPNG_DOWNLOAD_URL="http://prdownloads.sourceforge.net/libpng/${TAR_FILE}"

mkdir -p "${RV_BUILD_DIR}"
cd "${RV_BUILD_DIR}"

if [ ! -d "${LIBPNG_VERSION}" ]; then
    [ -f "${TAR_FILE}" ] || wget "${LIBPNG_DOWNLOAD_URL}"
    tar -xzf "${TAR_FILE}"
fi

cd "${LIBPNG_VERSION}"

./configure \
    --host=riscv64-unknown-linux-gnu \
    --prefix="${RV_INSTALL_DIR}/libpng" \
    --enable-shared \
    --disable-static \
    CC="${RV_CC}" \
    CXX="${RV_CXX}" \
    AR="${RV_AR}" \
    RANLIB="${RV_RANLIB}" \
    CFLAGS="${ARCH_FLAGS}" \
    CXXFLAGS="${ARCH_FLAGS}" \
    CPPFLAGS="-I${ZLIB_INC}" \
    LDFLAGS="-L${ZLIB_LIB} -Wl,-rpath-link=${ZLIB_LIB}"

make -j16
make install

file "${RV_INSTALL_DIR}"/libpng/lib/libpng16.so.16.*
ls -l "${RV_INSTALL_DIR}/libpng/lib"
```

## 11. PCL RVV

PCL 使用 `CelestialMelody/pcl.git` 源码。当前脚本中 `RV_BUILD_DIR` 表示 PCL 源码目录，而不是依赖库源码工作目录；`RV_PCL_DIR` 直接引用它。

如果目录不存在，脚本会 clone；如果路径已存在但不是 Git 仓库，脚本会退出，避免误把空目录当成源码。

```bash
export RV_INSTALL_DIR=
export RV_BUILD_DIR=
export RISCV_TOOLCHAIN=

export RV_CC="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-gcc"
export RV_CXX="${RISCV_TOOLCHAIN}/bin/riscv64-unknown-linux-gnu-g++"
export VLEN=256
export DEFAULT_LMUL=2
export EIGEN_ARCH_FLAGS="-mrvv-vector-bits=zvl -DEIGEN_RISCV64_USE_RVV10 -DEIGEN_RISCV64_DEFAULT_LMUL=${DEFAULT_LMUL}"
export ARCH_FLAGS="-march=rv64gcv_zvl${VLEN}b -mabi=lp64d -O3 -fPIC -D__RVV10__"

export BOOST_ROOT="${RV_INSTALL_DIR}/boost"
export EIGEN_ROOT="${RV_INSTALL_DIR}/eigen-rvv"
export FLANN_ROOT="${RV_INSTALL_DIR}/flann"
export LZ4_ROOT="${RV_INSTALL_DIR}/lz4"
export HDF5_ROOT="${RV_INSTALL_DIR}/hdf5"
export LIBPNG_ROOT="${RV_INSTALL_DIR}/libpng"
export ZLIB_ROOT="${RV_INSTALL_DIR}/zlib"

export EXTRA_INCLUDES="-I${BOOST_ROOT}/include \
                       -I${EIGEN_ROOT}/include/eigen3 \
                       -I${FLANN_ROOT}/include \
                       -I${LZ4_ROOT}/include \
                       -I${HDF5_ROOT}/include \
                       -I${LIBPNG_ROOT}/include \
                       -I${ZLIB_ROOT}/include"

export EXTRA_LIB_DIRS="-L${BOOST_ROOT}/lib \
                       -L${LZ4_ROOT}/lib \
                       -L${HDF5_ROOT}/lib \
                       -L${LIBPNG_ROOT}/lib \
                       -L${ZLIB_ROOT}/lib \
                       -L${FLANN_ROOT}/lib"

export EXTRA_RPATH_LINKS="-Wl,-rpath-link=${BOOST_ROOT}/lib \
                          -Wl,-rpath-link=${LZ4_ROOT}/lib \
                          -Wl,-rpath-link=${HDF5_ROOT}/lib \
                          -Wl,-rpath-link=${LIBPNG_ROOT}/lib \
                          -Wl,-rpath-link=${ZLIB_ROOT}/lib \
                          -Wl,-rpath-link=${FLANN_ROOT}/lib"

export RV_PCL_DIR="${RV_BUILD_DIR}"
PCL_GIT_URL="https://github.com/CelestialMelody/pcl.git"

if [ ! -d "${RV_PCL_DIR}/.git" ]; then
    if [ -e "${RV_PCL_DIR}" ]; then
        echo "Path ${RV_PCL_DIR} already exists, but it is not a git repository."
        exit 1
    fi

    mkdir -p "$(dirname "${RV_PCL_DIR}")"
    git clone "${PCL_GIT_URL}" "${RV_PCL_DIR}"
fi

cd "${RV_PCL_DIR}"
rm -rf build && mkdir build && cd build

cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
    -DCMAKE_C_COMPILER="${RV_CC}" \
    -DCMAKE_CXX_COMPILER="${RV_CXX}" \
    -DCMAKE_INSTALL_PREFIX="${RV_INSTALL_DIR}/pcl-rvv" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${ZLIB_ROOT};${LIBPNG_ROOT};${LZ4_ROOT};${HDF5_ROOT};${FLANN_ROOT};${EIGEN_ROOT};${BOOST_ROOT}" \
    -DCMAKE_LIBRARY_PATH="${LZ4_ROOT}/lib;${ZLIB_ROOT}/lib;${HDF5_ROOT}/lib;${BOOST_ROOT}/lib;${FLANN_ROOT}/lib;${LIBPNG_ROOT}/lib" \
    -DCMAKE_INCLUDE_PATH="${BOOST_ROOT}/include;${EIGEN_ROOT}/include/eigen3;${FLANN_ROOT}/include;${LZ4_ROOT}/include;${HDF5_ROOT}/include;${LIBPNG_ROOT}/include;${ZLIB_ROOT}/include" \
    -DCMAKE_FIND_ROOT_PATH="${BOOST_ROOT};${EIGEN_ROOT};${FLANN_ROOT};${LZ4_ROOT};${HDF5_ROOT};${LIBPNG_ROOT};${ZLIB_ROOT}" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DBOOST_ROOT="${BOOST_ROOT}" \
    -DBoost_NO_SYSTEM_PATHS=ON \
    -DEigen3_DIR="${EIGEN_ROOT}/share/eigen3/cmake" \
    -DFlann_DIR="${FLANN_ROOT}/share/flann" \
    -DHDF5_ROOT="${HDF5_ROOT}" \
    -DPNG_ROOT="${LIBPNG_ROOT}" \
    -DZLIB_ROOT="${ZLIB_ROOT}" \
    -DCMAKE_C_FLAGS="${ARCH_FLAGS} ${EXTRA_INCLUDES}" \
    -DCMAKE_CXX_FLAGS="${ARCH_FLAGS} ${EIGEN_ARCH_FLAGS} ${EXTRA_INCLUDES}" \
    -DCMAKE_EXE_LINKER_FLAGS="${EXTRA_LIB_DIRS} ${EXTRA_RPATH_LINKS}" \
    -DCMAKE_SHARED_LINKER_FLAGS="${EXTRA_LIB_DIRS} ${EXTRA_RPATH_LINKS}" \
    -DPCL_ENABLE_SSE=OFF \
    -DPCL_ENABLE_AVX=OFF \
    -DWITH_CUDA=OFF \
    -DWITH_OPENGL=OFF \
    -DWITH_LIBUSB=OFF \
    -DWITH_PCAP=OFF \
    -DWITH_QT=OFF \
    -DWITH_VTK=OFF \
    -DWITH_TESTS=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_examples=OFF \
    -DBUILD_global_tests=OFF \
    -DCMAKE_POLICY_DEFAULT_CMP0144=NEW

make -j16
make install

file "${RV_INSTALL_DIR}"/pcl-rvv/lib/libpcl_common.so*
```

关键点：

- `-D__RVV10__` 用于进入 PCL RVV 代码分支；如果要对比标量版本，需要移除该宏后重新配置和编译。
- `PCL_ENABLE_SSE` 和 `PCL_ENABLE_AVX` 必须关闭，因为目标是 RISC-V。
- CUDA、OpenGL、USB、PCAP、Qt、VTK 和 tests/examples 当前默认关闭，以降低交叉编译依赖复杂度。
- `Eigen3_DIR` 指向 `${RV_INSTALL_DIR}/eigen-rvv/share/eigen3/cmake`，确保 PCL 使用 RVV 版本 Eigen。
- `Flann_DIR`、`HDF5_ROOT`、`PNG_ROOT`、`ZLIB_ROOT` 用来减少 CMake 查找系统库的概率。

## 12. 增量重编 PCL 模块

如果只修改 `common` 下少量源文件，例如 `common/src/gaussian.cpp`，可以只重编 `pcl_common` 并复制生成的库，避免触发全量安装。

首次建立 `build` 目录时仍需执行与第 11 节一致的 `cmake` 配置。后续 CMake 选项不变时，可直接在 `build` 目录中执行：

```bash
cd "${RV_PCL_DIR}/build"
make -j16 pcl_common

install -d "${RV_INSTALL_DIR}/pcl-rvv/lib"
cp -a lib/libpcl_common.so* "${RV_INSTALL_DIR}/pcl-rvv/lib/"

file "${RV_INSTALL_DIR}"/pcl-rvv/lib/libpcl_common.so*
```

如果修改了 `ARCH_FLAGS`、`EIGEN_ARCH_FLAGS`、依赖路径或开关选项，请重新执行 CMake 配置，必要时清理 `build` 目录。

## 13. 常见警告

当前构建过程中可能出现少量 warning：

- FLANN 可能出现 `-Woverloaded-virtual`，脚本已通过 `-Wno-overloaded-virtual` 屏蔽。
- PCL `KdTreeFLANN` 模板实例化时，GCC 16 可能报告 `-Wstringop-overread`。如果构建成功且库文件为 RISC-V shared object，通常可以先保留观察。
- OpenMP 5.1 可能提示 `#pragma omp master` 已 deprecated，建议后续源码层面再决定是否改成 `masked`。

判断是否需要处理 warning 时，优先查找真正的错误信号：

```bash
rg -n "error:|undefined reference|cannot find|No such file|fatal error|CMake Error|collect2: error" build.log
```

## 14. 验证安装结果

每个库安装后都建议使用 `file` 检查目标架构。例如：

```bash
file "${RV_INSTALL_DIR}/zlib/lib/libz.so.1.3.2"
file "${RV_INSTALL_DIR}/lz4/lib/liblz4.so.1.10.0"
file "${RV_INSTALL_DIR}/hdf5/lib/libhdf5_cpp.so.320.1.0"
file "${RV_INSTALL_DIR}/flann/lib/libflann_cpp.so.1.9.2"
file "${RV_INSTALL_DIR}/boost/lib/libboost_system.so.1.88.0"
file "${RV_INSTALL_DIR}/gtest/lib/libgtest.so"
file "${RV_INSTALL_DIR}"/libpng/lib/libpng16.so.16.*
file "${RV_INSTALL_DIR}"/pcl-rvv/lib/libpcl_common.so*
```

期望输出中应包含 `RISC-V`、`64-bit`、`shared object` 等信息。
