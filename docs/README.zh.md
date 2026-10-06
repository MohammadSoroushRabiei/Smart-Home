<div align="center">

[🇬🇧 English](../README.md) · [🇮🇷 فارسی](README.fa.md) · **🇨🇳 中文**

# 🏠 智能家居 — 基于 ESP32-S3 的边缘优先家庭自动化

**一个自托管的智能家居平台：片上人脸识别、芯片内自学习行为代理、完全本地化的服务栈 — 无需云端。**

[![MCU](https://img.shields.io/badge/ESP32--S3-N16R8-E7352C?logo=espressif&logoColor=white)](hardware/pin-mapping.md)
[![Framework](https://img.shields.io/badge/ESP--IDF-6.0.2-3C5CA8?logo=espressif&logoColor=white)](https://docs.espressif.com/projects/esp-idf/)
[![RTOS](https://img.shields.io/badge/FreeRTOS-dual--core-2B6CB0)](#)
[![UI](https://img.shields.io/badge/LVGL-9.5-18BC9C)](#)
[![C](https://img.shields.io/badge/C-11-555555?logo=c&logoColor=white)](#)
[![C++](https://img.shields.io/badge/C%2B%2B-ESP--DL-00599C?logo=cplusplus&logoColor=white)](#)

[![MQTT](https://img.shields.io/badge/MQTT-Mosquitto-660066?logo=mqtt&logoColor=white)](#)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-integration-41BDF5?logo=home-assistant&logoColor=white)](#)
[![FastAPI](https://img.shields.io/badge/FastAPI-servers-009688?logo=fastapi&logoColor=white)](#)
[![Docker](https://img.shields.io/badge/Docker-deployment-2496ED?logo=docker&logoColor=white)](#)
[![Python](https://img.shields.io/badge/Python-3-3776AB?logo=python&logoColor=white)](#)
[![Google Sheets](https://img.shields.io/badge/Google%20Sheets-archive-34A853?logo=googlesheets&logoColor=white)](#)

<img src="../report/figs/fig1_architecture.png" width="100%" alt="系统架构 — 手机、ESP32-S3 边缘节点、家庭服务器、可选互联网"/>

*所有智能都在边缘节点上运行 — 手机只是出借它的摄像头，服务器保持本地。*

</div>

## ✨ 亮点

- 🔓 **微控制器上的人脸识别门锁** — ESP-DL CNN 流水线（检测 → 嵌入 → 余弦匹配）
  完全运行在 ESP32-S3 上；手机仅通过 HTTPS 上传一张 JPEG，图像在本地处理，绝不存储。
- 🧠 **会学习用户习惯的行为代理** — 两个逻辑回归头（灯、风扇）离线预训练达到
  **94.2% / 93.6%** 的验证准确率，随后根据你的每一次手动操作**在芯片上**继续进行 SGD 在线学习。
- 🖥️ **完整的触摸仪表盘** — 320×480 的 LVGL 9 界面：Wi-Fi 配置、MQTT 配置、
  系统设置、考勤与亮度控制，全部在设备上完成。
- 📅 **离线优先的考勤** — LCD 上显示二维码令牌，手机摄像头进行人脸匹配，
  记录进入 NVS 发件箱队列，并同步到 FastAPI/SQLite、Google Sheets（波斯历）与 Bale 消息通知。
- 🏡 **双向 Home Assistant** — 所有设备状态通过 MQTT 发布，可从 HA 仪表盘和
  自动化中控制；Bale 机器人增加聊天远程控制，包含 8 个命令、事件通知与安全日志。
- 🛡️ **为持续运行而工程化** — 双 I²C 总线、触摸控制器硬件复位看门狗、任务看门狗、
  网络感知的 Wi-Fi/MQTT 重连，以及 NVS 持久化状态，从容应对断电断网。
- 🔒 **架构层面的隐私** — 所有核心服务都运行在家庭局域网；互联网是可选的，
  仅用于出站同步与通知。

## 📸 真实硬件 — 照片与视频

以下内容没有任何渲染或模型 — 全部来自运行中的系统：实体板卡、真实的 LCD、
已部署的容器以及各个应用。

### 🖥️ 设备端界面 — 来自真实 LCD 的截图

| | | |
|:---:|:---:|:---:|
| <img src="../presentation/Images/LCD%20UI/Main%20Screen.png" width="300" alt="主屏 — Wi-Fi 状态、考勤与网页仪表盘按钮、灯/风扇/ML 磁贴与环境读数"/> | <img src="../presentation/Images/LCD%20UI/Unlock%20Screen.png" width="300" alt="解锁屏 — 人脸解锁与手动控制"/> | <img src="../presentation/Images/LCD%20UI/Wifi%20List.png" width="300" alt="Wi-Fi 列表 — 设备端扫描与选择网络"/> |
| <img src="../presentation/Images/LCD%20UI/Wifi%20Enter%20Password.png" width="300" alt="屏幕键盘输入 Wi-Fi 密码"/> | <img src="../presentation/Images/LCD%20UI/Mqtt%20Setting.png" width="300" alt="设备端配置 MQTT 代理"/> | <img src="../presentation/Images/LCD%20UI/Setting%20Screen.png" width="300" alt="设置 — 密码、亮度与系统选项"/> |
| <img src="../presentation/Images/LCD%20UI/Faces%20List.png" width="300" alt="人脸列表 — 在设备上管理已录入人脸"/> | <img src="../presentation/Images/LCD%20UI/Attend%20QR%20Code.png" width="300" alt="带倒计时的考勤二维码令牌"/> | <img src="../presentation/Images/LCD%20UI/Web%20QR%20Code.png" width="300" alt="在任意手机上打开网页仪表盘的二维码"/> |

*主仪表盘 · 人脸解锁 · 屏幕键盘扫描并连接 Wi-Fi · MQTT 配置 · 设置与密码 ·
人脸录入 · 双模式考勤二维码 · 在任意手机上打开网页仪表盘的二维码。*

*真实显示屏上的实测 — 触摸、开锁、传感器（38 秒）：*

<div align="center">

<video controls width="360" src="https://github.com/user-attachments/assets/3fca52ed-e168-4575-a4f6-300e99fccbfe"></video>

</div>

### 🌐 网页仪表盘 — 由考勤服务器提供

<div align="center">

<img src="../presentation/Images/Web%20Dashboard.png" width="360" alt="网页仪表盘 — 设备磁贴、带 PIN 与人脸识别的门锁、实时环境读数、ML 代理状态"/>

</div>

设备磁贴、门锁（PIN + 人脸识别）、实时环境读数与 ML 代理的 SHADOW/AUTO 状态 —
在局域网内提供服务；任何手机扫描 LCD 上显示的二维码即可访问。

*仪表盘实测 — 开关、开锁、实时数值（43 秒）：*

<div align="center">

<video controls width="300" src="https://github.com/user-attachments/assets/7c6d9063-2cdd-453c-a8b2-fb2aef746ac0"></video>

</div>

### 🏠 Home Assistant — 真实手机上的伴侣应用

<div align="center">

<img src="../presentation/Images/Smart%20Home%20%E2%80%93%20Home%20Assistant_Dashboard.png" width="420" alt="手机上的 Home Assistant 仪表盘 — 环境卡片、设备控制、门禁状态与自动化"/>

</div>

环境卡片、灯 / 风扇 / ML 自主控制、门禁状态与四个真实自动化 — 所有状态通过
MQTT 双向同步。

*移动应用实测 — 实时状态与控制（58 秒）：*

<div align="center">

<video controls width="300" src="https://github.com/user-attachments/assets/a053339e-497f-4105-baa8-8d17881ceb84"></video>

</div>

### 🤖 Bale 机器人 — 通过聊天远程控制

| | | |
|:---:|:---:|:---:|
| <img src="../presentation/Images/Bale_1.jpg" width="300" alt="Bale 机器人 — 回复键盘控制与即时确认"/> | <img src="../presentation/Images/Bale_2.jpg" width="300" alt="Bale 机器人 — 通过两分钟一次性密码开门"/> | <img src="../presentation/Images/Bale_3.jpg" width="300" alt="Bale 机器人 — 完整的斜杠命令菜单"/> |

*回复键盘控制与即时确认 · 通过 2 分钟一次性密码开门 · 完整的斜杠命令菜单。*

### 📅 考勤 — 端到端

记录落在本地 FastAPI/SQLite 服务器上，以波斯历日期与出入类型归档到 Google
Sheets，Bale 机器人同步播报 — 全程只需几秒。

<div align="center">

<img src="../presentation/Images/Google%20Sheet.png" width="480" alt="Google Sheet 考勤归档 — 波斯历日期、时间、姓名与出入行"/>

</div>

*人脸考勤实测 — 扫码 → 人脸匹配 → 记录 + 通知（30 秒）：*

<div align="center">

<video controls width="300" src="https://github.com/user-attachments/assets/4096e28d-9eb9-4661-957a-d4dcb7e40f97"></video>

</div>

### 🐳 本地服务栈

<div align="center">

<img src="../presentation/Images/Docker.png" width="90%" alt="Docker Desktop — 考勤服务器、Mosquitto、Home Assistant 与 Bale 机器人容器全部运行中"/>

</div>

整个平台的后端都在一台家庭服务器上：考勤服务、Mosquitto、Home Assistant 与
Bale 机器人 — 四个容器，仅占用几百兆内存。

## 🎬 交互式实时中枢 — 在浏览器中运行整个系统

[![交互式实时中枢 — 在浏览器中打开](img/motion-hub.png)](https://mohammadsoroushrabiei.github.io/Smart-Home/presentation/motion-hub.html)

**[`presentation/motion-hub.html`](../presentation/motion-hub.html)** 是整个运行
系统的自包含、零依赖复刻。在任何浏览器中打开它 — 无需构建、无需服务器、无需
硬件 — 平台的每个部分都在一块屏幕上活起来：

- 🖥️ **真实的 LCD** — 可用的键盘 + 人脸解锁二维码、Wi-Fi 扫描与连接、MQTT
  配置、设置（录入二维码、管理人脸、门锁/设置密码）以及双模式考勤界面，与固件完全一致。
- 🌐 **实时网页仪表盘** — 每个设备的磁贴与三个实时图表；**Google Sheet** 标签页
  记录每一次考勤扫码（奇数次 = 入，偶数次 = 出）。
- 🏠 **Home Assistant** — 包含 `automations.yaml` 中四个真实自动化的网页面板，
  以及伴侣应用视图；每一次状态变化都是双向的。
- 🤖 **Bale 机器人** — 完整命令集（`/status`、带 2 分钟密码 TTL 的 `/open`、
  `/light_on`、`/fan_on` …）并推送门禁与考勤事件通知。
- 🧠 **实时 ML 代理** — 带有已训练权重的两个逻辑头；你点击的每一次手动操作
  都会成为页面上真实的一次 SGD 训练步骤。
- 🎙️ **引导式自动演示** — 以上全部内容的 12 步聚光灯导览。
- 🌍 **三语界面** — English · فارسی · 中文，可从顶栏的切换按钮中切换。

**▶ 在线打开：** <https://mohammadsoroushrabiei.github.io/Smart-Home/presentation/motion-hub.html>
— 或下载 `presentation/motion-hub.html` 后双击；包括字体在内的一切都嵌入在这一个文件中。

## 🏗️ 架构

板卡即边缘节点：它拥有界面、人脸引擎、ML 代理与全部 I/O。Docker 家庭服务器
运行 Mosquitto、Home Assistant 与考勤服务。一切都在本地 Wi-Fi 局域网内通信 —
云端只是一条虚线。

<img src="../report/figs/fig7_block_diagram.png" width="100%" alt="固件框图 — ESP32-S3 边缘节点内部与外设"/>

固件子系统之间的运行时数据流：

<img src="img/fig_runtime_flow.png" width="100%" alt="运行时数据流 — LVGL 界面、传感器任务、ML 代理、HTTPS 服务器与 MQTT 客户端围绕共享的 app_state"/>

## 🧠 片上机器学习

一个刻意精简的模型与一条严肃的流水线：12 个上下文特征（循环的时段编码、
日历、在场、光照、温度、湿度）馈入两个 sigmoid 头 — 每个设备一个 — 在 6,912
条样本的合成数据集上预训练，然后在**芯片本身**上精调：每一次手动操作都成为
在线 SGD 的训练样本，最多每 30 秒持久化一次到 NVS。

<img src="../report/figs/fig_ml_model.png" width="100%" alt="ML 设计 — 特征、单层网络、决策规则、晋升门控"/>

| | 灯 | 风扇 |
|---|---|---|
| 验证准确率（离线预训练） | **94.2%** | **93.6%** |
| 在线学习 | 片上 SGD（η = 0.08） | 片上 SGD（η = 0.08） |
| 持久化 | NVS 命名空间 `mlbrain` | NVS 命名空间 `mlbrain` |

安全优先的生命周期让代理保持诚实：它从 **SHADOW** 模式开始（只预测、上报
MQTT、不执行动作），只有在一个滚动窗口内达到 ≥ 15 次决策且准确率 ≥ 85% 后
才会晋升为 **AUTO** — 而手动操作永远立即获胜。**门锁绝不受模型控制。**

观看代理从零学习 — 每次手动操作都会拨动权重，概率曲线逐渐成形，一旦满足
晋升门控，模式即切换为 AUTO：

<img src="../report/figs/fig_ml_learning.gif" width="90%" alt="动画 — 用户行为的在线学习，每次手动操作都更新权重"/>

<details>
<summary>🔍 深入了解：SHADOW → AUTO 晋升门控</summary>
<br>
<img src="../report/figs/fig5_ml_agent.png" width="100%" alt="ML 代理生命周期 — 影子模式、晋升门控、自动模式、在线学习"/>
</details>

## 🔓 人脸识别与门禁控制

整条流水线都在板卡上：浏览器拍摄照片，硬件 JPEG 解码器将其转为 PSRAM 中的
RGB565 缓冲区，ESP-DL CNN 生成嵌入向量，与设备上的人脸数据库进行匹配 —
余弦相似度，阈值 0.70，多样本录入。

<img src="../report/figs/fig2_face_pipeline.png" width="100%" alt="人脸识别流水线 — 手机摄像头、HTTPS 上传、硬件 JPEG 解码、ESP-DL、匹配决策"/>

- 在设备界面或网页仪表盘（密码保护）中录入 / 编辑 / 删除人脸
- 带二维码令牌的录入链接，3 分钟内过期
- 开锁事件按来源（人脸、键盘、网页、机器人）记录并推送通知

## 📅 考勤系统

零云端依赖的三层架构：LCD 显示轮换的二维码令牌（10 分钟有效期 + 倒计时），
员工手机打开页面并进行人脸匹配，事件落在本地 FastAPI/SQLite 服务器上 —
服务器再以波斯历日期归档到 Google Sheets 并通知 Bale 机器人。若服务器或
互联网不可用，记录在板卡上的 32 槽 NVS 发件箱中等待，稍后补发。

<img src="../report/figs/fig4_attendance.png" width="100%" alt="考勤架构 — 设备层、本地服务器层、可选互联网交付"/>

## 🧱 技术栈

<img src="../report/figs/fig6_stack.png" width="100%" alt="软件栈 — 硬件、ESP-IDF + FreeRTOS、库、应用模块、本地基础设施"/>

## 🔌 硬件

| 部件 | 规格 | 接口 |
|---|---|---|
| MCU | ESP32-S3-DevKitC-1（N16R8 — 16 MB Flash，8 MB 八线 PSRAM） | — |
| 显示屏 | 320×480 TFT（ST7796），8 位并口，PWM 背光 | GPIO 总线 |
| 触摸 | GT911 电容式控制器 | I²C（独立总线） |
| 环境 | BME280（温度 / 湿度 / 气压）+ LDR（光照） | I²C / ADC |
| 门锁 | 继电器驱动的电磁锁 | GPIO |
| 其他 | 状态 LED、实体按键、板载 WS2812 | GPIO |

完整引脚映射与空闲/占用 GPIO：[`hardware/pin-mapping.md`](hardware/pin-mapping.md)

## 📁 仓库结构

```
Smart-Home/
├── main/                  ESP-IDF 固件（应用 + display/ 界面 + certs/）
├── demo/                  PC 演示 — 在 SDL2 虚拟硬件上运行真实固件逻辑与界面
├── server/
│   ├── attendance/        考勤服务器（FastAPI + SQLite + Sheets + Bale）
│   └── bale_bot/          Bale 消息机器人（FastAPI + SQLite）
├── homeassistant/         HA docker-compose 配置 + Mosquitto 配置
├── ml/                    ML 流水线：合成数据 → 训练 → C 权重头文件
├── tools/                 宿主机侧工具（串口日志查看器、人脸 API 测试笔记本）
├── docs/                  数据手册、引脚映射、设计文档、开题报告、波斯语 README
├── report/                项目报告（docx/pdf 生成器 + 插图）
├── presentation/          答辩幻灯片（pptx 生成器）
├── CMakeLists.txt         ESP-IDF 6.0.2 工程
├── partitions.csv         Flash 分区表
└── sdkconfig.defaults     构建默认配置
```

## 🚀 快速开始

**固件**（需要 [ESP-IDF 6.0.2](https://docs.espressif.com/projects/esp-idf/)）：

```bash
git clone https://github.com/MohammadSoroushRabiei/Smart-Home.git
cd Smart-Home
idf.py set-target esp32s3
idf.py build flash monitor
```

**桌面演示** — 相同的固件逻辑与界面运行在 SDL2 虚拟硬件上（无需开发板）：

```bash
cmake -S demo -B demo/build && cmake --build demo/build
./demo/build/smartdemo
```

**服务**（Docker，在各服务目录下）：

```bash
cd server/attendance && docker compose up -d --build
cd server/bale_bot    && docker compose up -d --build
```

**ML 流水线** — 重新生成并训练行为模型：

```bash
python ml/generate_data.py   # 30 天合成数据集
python ml/train.py           # 生成 main/ml_model_weights.h
idf.py build                 # 固件加载新权重
```

## 📚 文档

- 📄 [项目报告（PDF，53 页）](../report/render/SmartHome-Project-Report.pdf)
- 🔌 [引脚映射与空闲 GPIO 参考](hardware/pin-mapping.md)
- 🧠 [ML 系统设计](ml-design.md)
- 📅 [考勤系统搭建指南](attendance/README.md)
- 💬 [Bale 机器人指南](../server/bale_bot/README.md)
- 🇬🇧 [English README](../README.md) · 🇮🇷 [مستندات فارسی](README.fa.md)

---

## 许可证

本项目为专有项目 — **保留所有权利**。参见 [LICENSE](../LICENSE)。
未经书面许可，不得复用、再分发或进行衍生创作。
联系邮箱：mohammadsoroushrabiei@gmail.com

<div align="center">

<img src="proposal/logo_iut.png" width="64" alt="伊斯法罕理工大学校徽"/><br>

**Mohammad Soroush Rabiei** — 本科毕业设计，[伊斯法罕理工大学](https://www.iut.ac.ir/)

[![GitHub](https://img.shields.io/badge/GitHub-@MohammadSoroushRabiei-181717?logo=github&logoColor=white)](https://github.com/MohammadSoroushRabiei)

</div>
