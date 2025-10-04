## 项目简介

simple_ros是一个轻量级、高性能的机器人操作系统框架，采用发布-订阅通信模式，提供高效的节点间通信功能，可用于学习分布式通信架构等知识。

## 快速开始

### 1. Docker部署方式

项目提供了Docker环境支持，可以快速部署和运行：

```bash
# 克隆项目
# git clone https://github.com/yourusername/simple_ros.git
cd simple_ros/docker

# 构建 Docker 镜像（镜像名：simple_ros_env:ubuntu20）
docker build -t simple_ros_env:ubuntu20 -f dockerfile .

# 启动 Docker 容器（容器名：simple_ros_container）
docker run -it \
    --name simple_ros_container \
    -v $(pwd)/..:/simple_ros \
    --network host \
    simple_ros_env:ubuntu20 \
    /bin/bash

# 进入容器（如果容器已经在运行）
docker exec -it simple_ros_container bash

# 在容器中执行依赖安装
bash /simple_ros/docker/install_deps.sh

# 生成 proto 文件
bash /simple_ros/scripts/proto.sh

# 快速启动示例 编译proto->编译代码->启动四旋翼demo
bash /simple_ros/scripts/quick_start.sh

```


Docker镜像基于Ubuntu 20.04，已包含以下配置：

- 使用阿里云镜像源加速下载
- 预装基础工具：git、build-essential、cmake、eigen等
- 配置git代理支持

### 2. 普通部署方式

#### 2.1 安装依赖

项目提供了一键安装依赖脚本：

```bash
# 在项目根目录下运行
cd simple_ros/docker
chmod +x install_deps.sh
./install_deps.sh
```

该脚本会自动安装以下依赖：

- GoogleTest
- Muduo网络库
- gRPC v1.56.0
- foxglove SDK

#### 2.2 生成Protobuf和gRPC代码

项目提供了生成Protobuf和gRPC代码的脚本：

```bash
# 在simple_ros目录下运行
chmod +x scripts/proto.sh
./scripts/proto.sh
```

该脚本会遍历proto目录下所有.proto文件，生成对应的C++代码到src/generated目录。

#### 2.3 编译项目

```bash
# 编译项目
mkdir build && cd build
cmake ..
make -j8
```

### 3. 运行项目

#### 3.1 启动Master服务

在使用simple_ros之前，需要先启动Master服务：

```bash
# 在一个终端中启动Master服务
cd build
./bin/tools/master
```

#### 3.2 运行示例

项目提供了示例，四旋翼模拟器和可视化示例：

```bash
# 在另一个终端中运行四旋翼模拟器
cd build
./bin/examples/quad_simulator_node

# 在第三个终端中运行四旋翼可视化节点
cd build
./bin/examples/quad_visualizer_node

# 启动Foxglove Bridge以进行可视化
cd build
./bin/tools/foxglove_bridge_node
```

#### 3.3 查看可视化结果

1. 下载并安装 [Foxglove Studio](https://foxglove.dev/download)
2. 启动Foxglove Studio
3. 点击"Open Connection"按钮
4. 选择"Foxglove WebSocket"连接类型
5. 输入连接地址：ws://localhost:8765
6. 点击"Connect"按钮
7. 在左侧面板中选择要查看的主题（如visualization_marker）

## 使用示例

关于详细的使用示例和API说明，请参考[docs/示例代码解析.md](docs/示例代码解析.md)和[docs/API参考.md](docs/API参考.md)。

## 项目结构

```
simple_ros/
├── include/           # 头文件目录
├── src/               # 源代码目录
│   ├── generated/     # 自动生成的protobuf代码
│   └── ...            # 其他源代码文件
├── proto/             # Protocol Buffers 定义文件
├── examples/          # 示例代码
├── test/              # 测试代码
├── tools/             # 工具程序
├── scripts/           # 脚本文件
└── docs/              # 文档目录
```

## 代码统计

| 语言 (Language) | 文件数 (files) | 空行 (blank) | 注释 (comment) | 代码 (code) |
|------------------|----------------|---------------|----------------|--------------|
| C++              | 29             | 604           | 298            | 2885         |
| Markdown         | 6              | 682           | 0              | 2004         |
| C/C++ Header     | 14             | 187           | 214            | 688          |
| HTML             | 1              | 66            | 0              | 379          |
| Protocol Buffers | 5              | 44            | 38             | 190          |
| Bourne Shell     | 3              | 31            | 20             | 142          |
| CMake            | 1              | 24            | 13             | 134          |
| **合计 (SUM)**  | **59**         | **1638**      | **583**        | **6422**     |


## 文档

项目提供了详细的文档，位于 `docs`目录下：

- [项目总览](docs/项目总览.md)：项目的概述和基本信息
- [核心模块设计](docs/核心模块设计.md)：详细介绍各个核心模块的设计和实现
- [API参考](docs/API参考.md)：详细介绍系统提供的主要接口和使用方法
- [可视化模块](docs/可视化模块.md)：详细介绍Foxglove Bridge的集成和使用
- [示例代码解析](docs/示例代码解析.md)：详细解释示例代码的功能和使用方法

## 许可证

该项目采用MIT许可证 - 详见[LICENSE](LICENSE)文件

