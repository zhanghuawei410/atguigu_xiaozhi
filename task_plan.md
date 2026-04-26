# 尚硅谷AI小智项目 — 开发计划

**创建时间**: 2026-04-15  
**项目状态**: 开发阶段（音频闭环已完成，WebSocket 集成中）

---

## 一、项目现状

### 已完成模块
| 模块 | 状态 | 说明 |
|------|------|------|
| 音频硬件编解码 | ✅ | ES8311 驱动，I2S 接口 |
| 语音识别 (AFE) | ✅ | WakeNet 唤醒 + VAD 检测 |
| Opus 编码/解码 | ✅ | PCM ↔ Opus 转换 |
| 音频闭环测试 | ✅ | 采集→编码→解码→播放 |
| WiFi 配网 | ✅ | BLE 配网实现 |
| 显示模块 | ✅ | LVGL + LCD |
| 按键模块 | ✅ | ADC 按键 |
| 设备激活 | ✅ | HTTPS 请求 |

### 进行中模块
| 模块 | 状态 | 说明 |
|------|------|------|
| WebSocket | 🔄 | 框架已搭建，核心逻辑待实现 |

### 待完成
- [ ] 完成 WebSocket 核心功能
- [ ] 集成音频 ↔ WebSocket 数据流
- [ ] 恢复完整初始化流程
- [ ] 端到端测试

---

## 二、待办任务

### 任务 1: 完成 WebSocket 模块核心功能
**优先级**: 🔴 高  
**文件**: `main/internet/xiaozhi_ws.c`

- [ ] 实现 WebSocket 连接建立
- [ ] 实现消息接收回调处理
- [ ] 实现音频数据发送逻辑
- [ ] 处理连接断开和重连
- [ ] 集成到 `xiaozhi_handle`

### 任务 2: 替换测试任务，集成 WebSocket 数据流
**优先级**: 🔴 高  
**文件**: `main/main.c`

- [ ] 创建 WebSocket 发送任务（编码器 → WS）
- [ ] 创建 WebSocket 接收任务（WS → 解码器）
- [ ] 删除 `encoder_decoder_test_task`

### 任务 3: 恢复完整初始化流程
**优先级**: 🟡 中  
**文件**: `main/main.c`

- [ ] 取消注释显示初始化
- [ ] 取消注释 WiFi 配网
- [ ] 取消注释按键初始化和回调
- [ ] 取消注释激活请求
- [ ] 添加激活状态检查

### 任务 4: 端到端测试
**优先级**: 🟡 中

- [ ] 测试语音唤醒 → AI 响应 → 语音播放
- [ ] 测试 WiFi 重置功能
- [ ] 测试设备激活流程

---

## 三、技术要点

### WebSocket 数据协议
```
发送格式: JSON + binary (Opus音频)
接收格式: JSON + binary (Opus音频)
关键字段: type, audio, text
```

### RingBuffer 集成
```
现有拓扑:
sr2encoder (8KB)     → opus_encoder
encoder2ws (16KB)    → [测试任务] → decoder_in (16KB)
                         ↓
                     WebSocket (待替换)

新拓扑:
sr2encoder (8KB)     → opus_encoder
encoder2ws (16KB)    → WebSocket 发送任务 → 网络
                                            ↓
网络 ← WebSocket 接收任务 ← decoder_in (16KB)
```

---

## 四、里程碑

| 里程碑 | 目标 | 状态 |
|--------|------|------|
| M1 | 音频闭环测试通过 | ✅ 完成 |
| M2 | WebSocket 基础连接 | 🔄 进行中 |
| M3 | 语音交互端到端 | ⏳ 待开始 |
| M4 | 正式版固件发布 | ⏳ 待开始 |

---

## 五、风险与依赖

### 风险
1. WebSocket 服务器 API 可能变更
2. 音频同步延迟可能影响体验
3. 网络不稳定时的重连逻辑

### 依赖
1. `esp_websocket_client` 组件
2. AI 服务器 WebSocket 服务可用性
3. 设备激活成功
