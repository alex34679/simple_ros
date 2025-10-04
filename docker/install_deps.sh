#!/bin/bash
set -e

# ===================== 公共函数 =====================
check_installed() {
    local header=$1
    local lib=$2

    if [[ -f "/usr/local/include/$header" ]] && [[ -f "/usr/local/lib/$lib" ]]; then
        return 0
    else
        return 1
    fi
}

# ===================== GoogleTest =====================
echo "安装 GoogleTest..."
if check_installed "gtest/gtest.h" "libgtest.a"; then
    echo "GoogleTest 已安装，跳过。"
else
    git clone https://github.com/google/googletest.git /tmp/googletest
    cd /tmp/googletest
    mkdir build && cd build
    cmake ..
    make -j$(nproc)
    make install
    cd /
    rm -rf /tmp/googletest
    echo "GoogleTest 安装完成。"
fi


# ===================== muduo =====================
echo "安装 muduo..."
if check_installed "muduo/base/Logging.h" "libmuduo_base.a"; then
    echo "muduo 已安装，跳过。"
else
    git clone https://github.com/chenshuo/muduo.git /tmp/muduo
    cd /tmp/muduo
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=14
    make -j$(nproc)
    make install
    cd /
    rm -rf /tmp/muduo
    echo "muduo 安装完成。"
fi


# ===================== gRPC =====================
echo "安装 gRPC v1.56.0..."
if check_installed "grpcpp/grpcpp.h" "libgrpc++.a"; then
    echo "gRPC 已安装，跳过。"
else
    git clone --recurse-submodules -b v1.56.0 https://github.com/grpc/grpc /tmp/grpc
    cd /tmp/grpc
    mkdir -p cmake/build && cd cmake/build
    cmake ../.. -DgRPC_BUILD_TESTS=OFF -DgRPC_INSTALL=ON -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    make install
    ldconfig
    cd /
    rm -rf /tmp/grpc
    echo "gRPC 安装完成。"
fi


# ===================== Foxglove SDK =====================
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
THIRD_PARTY_DIR="$SCRIPT_DIR/../third_party"
FOXGLOVE_VERSION="v0.14.2"
FOXGLOVE_ZIP="foxglove-$FOXGLOVE_VERSION-cpp-x86_64-unknown-linux-gnu.zip"
FOXGLOVE_URL="https://github.com/foxglove/foxglove-sdk/releases/download/sdk%2F${FOXGLOVE_VERSION}/${FOXGLOVE_ZIP}"
FOXGLOVE_DIR="$THIRD_PARTY_DIR/foxglove"

echo "安装 Foxglove SDK $FOXGLOVE_VERSION..."
if [ -d "$FOXGLOVE_DIR" ]; then
    echo "Foxglove SDK 已存在于 $FOXGLOVE_DIR，跳过下载和解压。"
else
    mkdir -p "$THIRD_PARTY_DIR"
    TMP_ZIP="/tmp/${FOXGLOVE_ZIP}"

    echo "下载 $FOXGLOVE_URL ..."
    wget -e use_proxy=yes \
     -e http_proxy=http://127.0.0.1:7890 \
     -e https_proxy=http://127.0.0.1:7890 \
     -O "$TMP_ZIP" "$FOXGLOVE_URL"

    echo "解压到 $THIRD_PARTY_DIR ..."
    unzip -q "$TMP_ZIP" -d "$THIRD_PARTY_DIR"

    rm "$TMP_ZIP"
    echo "Foxglove SDK 已安装到 $FOXGLOVE_DIR"
fi


echo "✅ 第三方库安装完成！"
