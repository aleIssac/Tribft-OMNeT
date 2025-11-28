# TriBFT-OMNeT++

基于 OMNeT++ 和 Veins 的车联网拜占庭容错（BFT）共识协议仿真项目。

## 项目概述

TriBFT 是一种专为车联网设计的基于信誉机制的拜占庭容错共识协议。本项目在 OMNeT++ 离散事件仿真框架中实现了 TriBFT 协议，并结合 Veins 进行车载网络仿真。

## 仿真环境

### 软件依赖

| 软件 | 版本 |
|------|------|
| SUMO (城市交通仿真) | 1.21 |
| OMNeT++ | 6.2 |
| Veins | 5.3.1 |

### 仿真地图

本仿真使用**中国广西南宁市**的真实路网数据，具体覆盖**青秀区**与**江南区**的部分区域。

**地图规格：**
- **东西跨度：** 9.64 公里
- **南北跨度：** 5.84 公里
- **总面积：** 约 56.32 平方公里

## 项目结构

```
Tribft-OMNeT/
├── src/
│   ├── application/      # TriBFT 应用层
│   ├── blockchain/       # 轻量级区块链同步
│   ├── common/           # 公共定义与类型
│   ├── consensus/        # HotStuff 共识引擎与 VRF 选择器
│   ├── messages/         # OMNeT++ 消息定义
│   ├── nodes/            # 节点定义
│   ├── reputation/       # 车辆信誉管理（VRM）
│   └── shard/            # 区域分片管理
├── simulations/
│   └── veins-base/       # 仿真配置与场景
└── Makefile
```

## 主要特性

- **基于 HotStuff 的 BFT 共识：** 三阶段共识协议（PREPARE、PRE-COMMIT、COMMIT）
- **基于 VRF 的领导者选举：** 使用可验证随机函数实现公平的领导者选择
- **双信誉模型：** 全局信誉与本地信誉的双重评分系统
- **区域分片：** 基于地理位置的分片管理，提高可扩展性
- **轻量级同步：** 普通节点仅同步区块头

## 编译

```bash
# 配置并编译
make makefiles
make
```

## 运行仿真

```bash
cd simulations/veins-base
# 首先启动 SUMO launcher
./run
```

## 许可证

本项目仅供研究使用。

## 说明

> 核心实现细节已隐藏，将在项目完成后公开发布。
