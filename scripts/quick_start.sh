#!/bin/bash
set -e

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"      # scripts 所在目录
BUILD_DIR="${SCRIPT_DIR}/../build"
TOOLS_DIR="${BUILD_DIR}/bin/tools"
EXAMPLES_DIR="${BUILD_DIR}/bin/examples"

# 设置环境变量
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

# 确保编译目录存在并构建
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake ..
make -j$(nproc)
cd "${SCRIPT_DIR}"

# 定义清理函数
cleanup() {
    echo "正在关闭所有节点..."
    pkill -f "${TOOLS_DIR}/master" || true
    pkill -f "${EXAMPLES_DIR}/quad_simulator_node" || true
    pkill -f "${EXAMPLES_DIR}/quad_visualizer_node" || true
    pkill -f "${TOOLS_DIR}/foxglove_bridge_tool" || true
    exit 0
}
trap cleanup SIGINT

# 启动 Master
if ! pgrep -f "${TOOLS_DIR}/master" > /dev/null; then
    echo "启动 Master..."
    "${TOOLS_DIR}/master" &
    sleep 1
else
    echo "Master 已经在运行"
fi

# 启动四旋翼模拟器
echo "启动四旋翼模拟器..."
"${EXAMPLES_DIR}/quad_simulator_node" &
sleep 1

# 启动四旋翼可视化
echo "启动四旋翼可视化..."
"${EXAMPLES_DIR}/quad_visualizer_node" &
sleep 1

# 启动 Foxglove Bridge
echo "启动 Foxglove Bridge..."
"${TOOLS_DIR}/foxglove_bridge_tool" &
sleep 1

echo "所有节点已启动！"
echo "请打开 Foxglove Studio 并连接到 ws://localhost:8765 查看可视化效果。"
echo "按 Ctrl+C 停止所有服务。"

# 等待所有后台进程
wait
