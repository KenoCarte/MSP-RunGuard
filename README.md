# PoseQtClient 使用说明

本目录是 OpenPose 项目的 Qt6 前端客户端，主要用于：

- 选择视频或摄像头作为输入源
- 调用 Python 后端执行时序姿态分析
- 在界面内显示实时预览画面（Live Monitor）
- 展示风险等级、风险分数、风险标记与建议
- 记录并导出运行日志

## 目录结构

- `qt_client/CMakeLists.txt`：Qt 工程构建文件
- `qt_client/src/main.cpp`：程序入口
- `qt_client/src/MainWindow.h`：主窗口声明
- `qt_client/src/MainWindow.cpp`：主窗口实现（UI、流程、日志、风险面板）

## 运行依赖

## 1. Qt 与编译环境

- Qt6（Widgets）
- CMake 3.16+
- C++17 编译器（g++/clang++/MSVC 任一）

## 2. Python 后端

前端通过 `QProcess` 调用 Python 脚本，所以需要保证：

- Python 环境可用
- OpenPose 项目依赖安装完成（torch、opencv、numpy 等）
- 以下脚本存在：
  - `infer/infer_once.py`
  - `analysis/run_temporal_analysis.py`

## 构建方法

以下示例在 `qt_client` 目录执行。

### Linux / WSL（推荐）

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
```

### Windows + MinGW（如果已配置 Qt + MinGW）

```powershell
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build . -j 4
```

如果提示 `cmake` 找不到，请先把 CMake 加入 PATH，或使用你本机 Qt Creator 自带工具链构建。

## 启动程序

在构建输出目录执行生成的可执行文件，例如：

- Linux/WSL: `./PoseQtClient`
- Windows: `PoseQtClient.exe`

## 首次配置建议

启动后建议先配置以下路径：

- Python：你的解释器路径
- Config：`experiments/vgg19_368x368_sgd.yaml`
- Weight：`network/weight/best_pose.pth`
- Video：待分析视频（Video 模式）
- Analysis Out Dir：分析输出目录
- Risk Config：`analysis/risk_config.json`

可点击 `Validate Config` 做基本检查。

## 使用流程

## A. 视频文件分析

1. Source 选择 `Video File`
2. 选择 Video 路径
3. 设置输出目录与参数（Frame Stride、是否保存 Overlay）
4. 点击 `Run Video Analysis`
5. 观察：
   - Live Monitor 实时刷新
   - Run Log 输出处理进度
   - Risk Feedback 显示等级和建议

## B. 摄像头分析

1. Source 选择 `Camera`
2. 可先点击 `Detect Camera` 自动探测可用索引
3. 设置 Camera Index 与 Max Seconds
4. 点击 `Run Video Analysis`

说明：摄像头模式下支持 `Show Live Preview`，会额外弹出后端 OpenCV 预览窗口；界面内的 Live Monitor 会持续显示预览图。

## 输出文件说明

每次分析输出目录通常包含：

- `features_per_frame.csv`：逐帧特征
- `summary.json`：风险汇总
- `overlay.mp4`：骨架叠加视频（开启保存时）
- `live_preview.jpg`：实时预览中间图（运行时持续更新）

## 常见问题

## 1. 摄像头打不开 / 超时

现象：`cannot open camera index` 或 `select() timeout`

建议：

- 先用 `Detect Camera`，选择可读帧的索引
- 在 WSL 中确认 `/dev/video*` 存在
- 避开 metadata 节点（常见为可打开但不可读帧）
- 若仍不稳定，可先切换 Video 模式

## 2. 界面无实时画面

检查：

- 分析是否已启动
- 输出目录下是否持续生成 `live_preview.jpg`
- 日志中是否有 Python 侧报错

## 3. 风险建议显示异常

程序会优先读取 `summary.json` 的 `analysis.advice`。若检测到旧格式编码问题，会自动回退到英文建议。

## 开发说明

- 日志采用按行缓冲刷新，避免半行输出造成可读性问题
- summary 文件使用文件监听自动刷新
- 紧凑模式会隐藏部分非主流程控件，便于演示

如需扩展：

- 可以在 `MainWindow.cpp` 中继续添加指标卡片/图表
- 可以将 `summary.json` 扩展字段映射到新的 UI 控件
