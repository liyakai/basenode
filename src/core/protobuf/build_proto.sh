#!/bin/bash

# 定义 protoc 路径
PROTOC="../../../3rdparty/toolbox/3rdparty/protobuf_3.21.11/bin/protoc"

# 检查 protoc 是否存在
if [ ! -f "$PROTOC" ]; then
    echo "Error: protoc compiler not found"
    exit 1
fi

# 创建输出目录
OUTPUT_DIR="pb_out"
mkdir -p $OUTPUT_DIR || { echo "Failed to create output directory"; exit 1; }

# 收集当前目录下所有的 .proto 文件
PROTO_FILES=$(find . -maxdepth 1 -name "*.proto" -type f)

if [ -z "$PROTO_FILES" ]; then
    echo "Error: No .proto files found in current directory"
    exit 1
fi

# 遍历所有 proto 文件并生成代码
for PROTO_FILE in $PROTO_FILES; do
    echo "Processing $PROTO_FILE..."
    
    # Generate C++ code
    echo "  Generating C++ pb code..."
    $PROTOC --cpp_out $OUTPUT_DIR $PROTO_FILE || { echo "Failed to generate C++ code for $PROTO_FILE"; exit 1; }
    
    # # Generate Go code
    # echo "  Generating Go pb code..."
    # $PROTOC --go_out $OUTPUT_DIR $PROTO_FILE || { echo "Failed to generate Go code for $PROTO_FILE"; exit 1; }
    
    echo "  Completed $PROTO_FILE"
    echo ""
done

echo "All proto files processed successfully!"

