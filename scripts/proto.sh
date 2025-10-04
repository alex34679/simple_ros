#!/bin/bash

# 获取脚本所在目录的绝对路径
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

# 定义 proto 文件目录和生成文件目录（基于脚本目录的相对路径）
PROTO_DIR="$SCRIPT_DIR/../proto"
GENERATED_DIR="$SCRIPT_DIR/../src/generated"

# 创建生成目录（如果不存在）
mkdir -p "$GENERATED_DIR"

# 遍历 proto 目录下所有 .proto 文件
for proto_file in "$PROTO_DIR"/*.proto; do
    proto_filename=$(basename "$proto_file")
    echo "Generating Protobuf C++ code for $proto_filename ..."
    protoc --proto_path="$PROTO_DIR" --cpp_out="$GENERATED_DIR" "$proto_file"

    # 如果文件中包含 service，则生成 gRPC 代码
    if grep -q "service " "$proto_file"; then
        echo "Generating gRPC C++ code for $proto_filename ..."
        protoc --proto_path="$PROTO_DIR" \
               --grpc_out="$GENERATED_DIR" \
               --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
               "$proto_file"
    fi
done

# 检查命令是否成功执行
if [ $? -eq 0 ]; then
    echo "Protobuf and gRPC code generated successfully!"
else
    echo "Failed to generate Protobuf and gRPC code."
    exit 1
fi
