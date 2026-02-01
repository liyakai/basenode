# 中心路由模块设计文档（主动连接版本）

## 概述

本文档描述了中心路由模块（RouterModule）的重新设计。RouterModule 作为独立的进程运行，通过 Zookeeper 服务发现主动连接所有业务进程，并负责在不同进程间转发 RPC 请求/响应。

## 架构设计

### 星型架构（主动连接模式）

```
                    [RouterModule]
                         /|\
                        / | \
                       /  |  \
            [主动连接] /   |   \ [主动连接]
                     /    |    \
            [Process A] [Process B] [Process C]
            (被动接收)  (被动接收)  (被动接收)
```

- **RouterModule**：中心路由进程
  - 通过 Zookeeper 服务发现获取所有服务实例
  - 主动连接所有业务进程
  - 维护服务到连接的路由表
  - 在不同进程间转发 RPC 请求/响应

- **业务进程**：被动接收连接
  - 在 Zookeeper 注册服务
  - 接收来自 RouterModule 的连接
  - 处理来自 RouterModule 的 RPC 请求
  - 返回响应给 RouterModule

### 工作流程

#### 初始化流程

```
[RouterModule 启动]
    |
    | 1. 连接 Zookeeper
    | 2. 发现所有服务（遍历 /basenode/services）
    | 3. 监听服务目录变化
    v
[对每个服务]
    |
    | 1. 监听服务实例变化
    | 2. 获取服务实例列表
    | 3. 主动连接所有实例
    v
[建立连接]
    |
    | 1. 记录连接映射
    | 2. 更新路由表（service_id -> conn_id）
    v
[就绪]
```

#### 请求流程

```
[Process A - Play 模块]
    |
    | RPC 请求 (service_id)
    v
[ModuleRouter]
    |
    | 查找本地模块
    v
[未找到本地模块]
    |
    | 说明：服务不在本地，需要通过 RouterModule 转发
    | 但 RouterModule 已主动连接，请求会通过 RouterModule 转发
    v
[RouterModule]
    |
    | 1. 查找路由表 (service_id -> conn_id)
    | 2. 转发请求到目标进程
    v
[Process B - Module 模块]
    |
    | 处理请求并返回响应
    v
[RouterModule]
    |
    | 根据请求上下文路由响应
    v
[Process A - Play 模块]
```

## 核心组件

### RouterModule (`src/framework/router/router_module.h/cpp`)

中心路由模块，作为独立进程运行：

- **服务发现**：从 Zookeeper 获取所有服务实例
- **主动连接**：主动连接所有业务进程
- **路由表管理**：维护 `service_id -> conn_id` 的路由表
- **请求转发**：在不同进程间转发 RPC 请求
- **响应路由**：将响应路由回源进程

### 关键特性

1. **完全基于 Zookeeper**：
   - 所有信息从 Zookeeper 获取
   - 监听服务变化，自动更新连接和路由表
   - 不需要业务进程主动注册

2. **主动连接模式**：
   - RouterModule 主动连接业务进程
   - 业务进程只需在 Zookeeper 注册服务
   - 简化业务进程的实现

3. **自动路由表构建**：
   - 基于 Zookeeper 服务信息自动构建
   - 服务变化时自动更新
   - 连接断开时自动清理

## 实现细节

### 1. 服务发现

- **服务目录监听**：监听 `/basenode/services` 目录变化
- **服务实例监听**：对每个服务监听实例变化
- **自动连接**：发现新实例时自动连接

### 2. 连接管理

- **主动连接**：RouterModule 主动连接业务进程
- **连接映射**：维护 `instance_key -> conn_id` 的映射
- **连接状态**：跟踪连接状态，断开时自动清理

### 3. 路由表管理

- **服务路由**：维护 `service_id -> conn_id` 的映射
- **自动更新**：服务实例变化时自动更新
- **负载均衡**：选择健康的实例（可扩展）

### 4. 请求转发

- **请求接收**：接收来自业务进程的 RPC 请求
- **路由查找**：根据 `service_id` 查找目标连接
- **请求转发**：转发请求到目标进程

### 5. 响应路由

- **响应接收**：接收来自目标进程的响应
- **上下文查找**：根据请求上下文查找源连接
- **响应转发**：转发响应到源进程

## 配置

### RouterModule 配置 (`config/router.json`)

```json
{
    "router": {
        "process": {
            "process_id": "router-process-1",
            "server_name": "router",
            "server_type": "router"
        },
        "network": {
            "listen": {
                "ip": "0.0.0.0",
                "port": 0
            }
        },
        "service_discovery": {
            "zookeeper": {
                "hosts": "127.0.0.1:2181",
                "paths": ["/basenode"]
            }
        }
    }
}
```

注意：RouterModule 不需要监听端口，因为它主动连接业务进程。

### 业务进程配置

业务进程只需要在 Zookeeper 注册服务，无需特殊配置：

```json
{
    "service_discovery": {
        "zookeeper": {
            "hosts": "127.0.0.1:2181",
            "paths": ["/basenode"]
        },
        "service_hosts": "127.0.0.1:9527"
    }
}
```

## 使用方式

### 启动 RouterModule

```bash
./basenode config/router.json
```

### 启动业务进程

```bash
./basenode config/basenode.json
./basenode config/gatenode.json
```

### 模块调用

模块无需修改代码，当调用远程服务时：
- 如果服务在本地进程，直接路由到本地模块
- 如果服务不在本地进程，RouterModule 会自动转发

## 优势

1. **完全基于 Zookeeper**：所有信息从 Zookeeper 获取，无需额外注册机制
2. **主动连接模式**：RouterModule 主动连接，简化业务进程实现
3. **自动路由表构建**：基于服务发现自动构建，无需手动配置
4. **服务变化自动处理**：监听服务变化，自动连接/断开

## 注意事项

1. **RouterModule 是单点**：需要保证 RouterModule 的高可用
2. **Zookeeper 依赖**：RouterModule 完全依赖 Zookeeper，需要保证 Zookeeper 的可用性
3. **连接管理**：需要处理连接断开和重连的情况
4. **请求上下文**：需要正确维护请求上下文以路由响应

## 文件结构

```
src/
├── framework/
│   └── router/
│       ├── router_module.h      # RouterModule 头文件
│       └── router_module.cpp    # RouterModule 实现
config/
└── router.json                   # RouterModule 配置文件
```

## 总结

重新设计的 RouterModule 实现了：
- ✅ 完全基于 Zookeeper 服务发现
- ✅ 主动连接所有业务进程
- ✅ 自动构建和维护路由表
- ✅ 服务变化自动处理
- ✅ 代码简洁优雅

该设计简化了业务进程的实现，所有路由逻辑集中在 RouterModule，通过 Zookeeper 统一管理服务信息。




