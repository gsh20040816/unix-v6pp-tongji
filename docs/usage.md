# 使用说明

本文面向课程作业和普通开发者，说明如何在本仓库中完成 `Unix V6++` 的环境准备、构建、运行和常见调试操作。

## 适用场景

本项目适合以下使用场景：

- 同济大学操作系统课程相关实验、课程设计和作业提交
- 在本地修改 `Unix V6++` 内核、用户程序或文件系统镜像后进行验证
- 使用 `QEMU` 或 `Bochs` 进行启动、交互、调试和回归测试
- 阅读和分析教学系统的构建链路、镜像生成流程与运行方式

## 平台兼容性说明

当前已明确验证的平台如下：

- `Arch Linux`：作者本机环境
- `GitHub CI` 提供的 `Ubuntu 24.04` 环境

请注意：

- 仓库顶层 `CMakeLists.txt` 当前显式限制为 `Linux host`；因此原生 Windows 环境不是本项目当前支持的构建宿主机。
- Windows 用户推荐通过 `WSL` 使用本项目，而不是在原生 Windows 环境中直接构建。
- 由于设备原因，当前**未进行 macOS 实机测试**。
- `macOS` 可能需要额外适配交叉编译工具链或构建脚本。
- 欢迎使用 `macOS` 的同学自行尝试，并通过 `PR` 改进兼容性。

## 推荐开发环境

推荐使用 `Visual Studio Code` 作为日常开发环境。

仓库当前已经提供了开箱即用的 VS Code 配置，包括：

- `.vscode/tasks.json`：已配置 `cmake/configure`、`cmake/build`、`qemu/run`、`qemu/debug`、`bochs/run` 等常用任务
- `.vscode/launch.json`：已配置 `QEMU` / `Bochs` 的 `gdb` 调试入口
- `.vscode/settings.json`：已启用 `CMake Presets` 相关设置

对课程作业和普通开发者来说，这意味着：

- 构建、运行、调试不必手工记忆完整命令
- 可以直接从 VS Code 启动 `QEMU` 或 `Bochs`
- 可以直接附加到 `gdbstub :1234` 做内核调试

如果你的开发环境是 `Linux` 或 `WSL + VS Code`，这套配置通常可以直接使用。

## Linux 原生环境

### 环境准备

建议使用常见的 `x86_64 Linux` 发行版，并准备一个普通开发目录，例如：

```bash
git clone https://github.com/gsh20040816/unix-v6pp-tongji.git
cd unix-v6pp-tongji
```

本项目默认使用 `CMake + Ninja` 构建，目标架构为 `i386`。构建时会依赖宿主机上的 32 位编译/链接能力，以及 `QEMU` 或 `Bochs` 等虚拟机工具。

### 依赖安装

至少需要以下工具：

- `cmake`（仓库要求 `>= 3.20`）
- `ninja`
- `gcc`
- `g++`
- `binutils`（提供 `objcopy`、`objdump`、`nm`）
- `nasm`
- 32 位多架构支持：通常需要 `gcc-multilib`、`g++-multilib` 或发行版等价包

常用可选工具：

- `QEMU` x86 软件包，要求宿主机上可执行 `qemu-system-i386`
- `bochs-gdb`
- `gdb`

如果你使用的是 `Ubuntu` / `Debian` 或基于它们的环境，可直接参考仓库 CI 当前使用的依赖集合：

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  cmake \
  ninja-build \
  nasm \
  binutils \
  gcc \
  g++ \
  gcc-multilib \
  g++-multilib \
  qemu-system-x86
```

说明：

- CI 中安装的是 `qemu-system-x86` 包，它会提供本项目实际查找的 `qemu-system-i386` 可执行文件。
- `Bochs` 不是必需依赖；如果宿主机未安装 `bochs-gdb`，相关 `CMake` 目标不会生成。

### 构建步骤

仓库根目录提供了 `CMakePresets.json`，日常开发建议直接使用预设：

```bash
cmake --preset dev-ninja
cmake --build --preset image -j"$(nproc)"
```

构建完成后，常见产物包括：

- `build/c.img`：最终磁盘镜像
- `build/objs/kernel`：带调试信息的内核 `ELF`
- `build/objs/kernel.bin`：写入镜像用的裸二进制内核

如果只是想清理构建结果，可以执行：

```bash
cmake --build --preset clean
```

### 运行步骤

#### 1. 使用 QEMU 运行

推荐优先使用 `QEMU`，并采用 `stdio` 模式作为默认运行方式。对于课程作业、普通开发、命令行复现和调试，这通常是本仓库最直接、最稳定的工作流。

最常用的启动方式如下：

```bash
cmake --build --preset qemu -j
```

该模式会：

- 自动依赖并检查 `image` 目标，必要时先增量更新 `build/c.img`
- 通过 `stdin/stdout` 提供串口交互，适合在终端里直接使用系统 Shell
- 将诊断输出分流到 `stderr`

如果你使用 VS Code，也推荐直接使用仓库已配置好的任务：

- `qemu/run`：默认运行入口，采用 `stdin/stdout`
- `qemu/debug`：默认调试入口，采用 `gdbstub + stdin/stdout`

这两个任务已经配置好日志输出位置，基本可以做到开箱即用。

此外，仓库还提供以下常用目标：

- `cmake --build --preset qemu-curses -j`：终端界面模式
- `cmake --build --preset qemu-gui -j`：图形界面模式
- `cmake --build --preset qemu-gdb -j`：以 `gdbstub :1234` 方式启动，便于调试

#### 2. 直接调用运行脚本

如果你需要显式切换 `QEMU_MODE`，也可以直接调用脚本：

```bash
QEMU_MODE=stdio OOS_BUILD_DIR="$PWD/build" bash qemu/run_qemu.sh
```

脚本当前支持以下模式：

- `curses`
- `stdio`
- `gui`
- `headless`
- `gdb-curses`
- `gdb-stdio`

#### 3. 使用 Bochs 运行

如果本机安装了 `bochs-gdb`，可以使用：

```bash
cmake --build --preset bochs -j
```

若需要 `gdbstub`，则使用：

```bash
cmake --build --preset bochs-gdb -j
```

### 常见开发与调试方式

#### 1. 日常开发回路

对课程作业或普通内核开发，通常可以按下面的节奏工作：

1. 修改内核、用户程序或文件系统内容
2. 重新构建镜像：`cmake --build --preset image -j"$(nproc)"`
3. 启动系统：`cmake --build --preset qemu -j`
4. 在系统 Shell 中执行用户程序或手工验证功能

仓库当前会把一批用户程序打包进镜像，例如 `ls`、`cat`、`cp`、`echo`、`date`、`shutdown`、`testfork`、`argvdump` 等；它们适合用于最基本的交互验证和课程演示。

#### 2. 使用 GDB 调试内核

先启动带 `gdbstub` 的 QEMU：

```bash
cmake --build --preset qemu-gdb -j
```

再在另一个终端连接：

```bash
gdb build/objs/kernel
```

进入 `gdb` 后执行：

```gdb
target remote :1234
```

`build/objs/kernel` 是带调试信息的内核 `ELF`，适合用于符号调试。

如果你使用 VS Code，可以直接选择以下调试配置，而不必手工输入 `gdb` 命令：

- `debug/qemu gdb (linux local)`
- `debug/qemu gdb + user symbols (linux local)`

这两项配置会配合 `.vscode/tasks.json` 中的 `qemu/debug` 自动启动调试流程。

#### 3. 查看 QEMU 诊断输出

在 `stdio` / `gdb-stdio` 模式下：

- 交互输出走 `stdout`
- 内核诊断输出走 `stderr`

在 `headless` 模式下，诊断信息会写入日志文件，默认位置为：

- `build/qemu-debugcon.log`

这类日志在定位启动失败、异常退出或早期内核输出时通常更直接。

#### 4. 运行仓库现有测试

如果你希望按仓库当前 CI 思路做一次本地回归，可执行：

```bash
cmake --preset dev-ninja
cmake --build --preset ci-qemu-tests-build -j"$(nproc)"
ctest --preset ci-qemu-tests
```

该预设当前覆盖的主要是：

- `QEMU` 启动冒烟测试
- `QEMU stdio` 交互集成测试

### 常见问题与注意事项

#### 1. 配置阶段直接报错 “Linux hosts only”

这是仓库的当前设计约束，不是配置参数写错。顶层 `CMakeLists.txt` 明确要求构建宿主机为 `Linux`。如果你在 Windows 上工作，请改用 `WSL`。

#### 2. 没有生成 `qemu` 或 `bochs` 目标

这通常意味着宿主机上缺少对应可执行文件：

- `qemu` 相关目标要求能找到 `qemu-system-i386`
- `bochs` 相关目标要求能找到 `bochs-gdb`

可以重新检查安装情况，并重新执行一次：

```bash
cmake --preset dev-ninja
```

#### 3. 32 位相关链接或编译失败

本项目目标架构是 `i386`，若宿主机缺少 multilib 支持，通常会在编译或链接阶段失败。请确认已经安装发行版提供的 32 位开发支持包，例如 `gcc-multilib`、`g++-multilib` 或其等价包。

#### 4. 图形模式不如终端模式稳定时，优先使用 `stdio`

对课程作业和普通开发者而言，`stdio` 模式通常是最直接的使用方式：命令行可复现、便于记录输出、也更适合和 `gdbstub` 或自动化测试结合。若不需要图形界面，建议优先使用：

```bash
cmake --build --preset qemu -j
```

## Windows + WSL 环境

### 推荐方式

Windows 用户请优先使用 `WSL`，而不是在原生 Windows 环境中直接构建。原因很直接：

- 本项目当前构建系统显式要求 `Linux host`
- 构建链路依赖 `bash`、Linux 工具链和 `QEMU/Bochs` 运行方式
- 仓库已经清理过旧的 Windows 兼容路径，当前维护重点不在原生 Windows

因此，更稳妥的方式是：

1. 安装 `WSL 2`
2. 安装一个常见 Linux 发行版（推荐 `Ubuntu`）
3. 在 `WSL` 内完成依赖安装、构建和运行

### WSL 内的依赖与构建

在 `WSL` 的 `Ubuntu` 环境中，可直接使用与 CI 接近的依赖安装方式：

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  cmake \
  ninja-build \
  nasm \
  binutils \
  gcc \
  g++ \
  gcc-multilib \
  g++-multilib \
  qemu-system-x86
```

随后按 Linux 原生环境相同方式操作：

```bash
cmake --preset dev-ninja
cmake --build --preset image -j"$(nproc)"
cmake --build --preset qemu -j
```

### WSL 下的使用建议

- 如果只是做课程作业、代码修改和基础验证，优先使用 `QEMU stdio` 模式。
- 如果你通过 `VS Code + Remote - WSL` 打开仓库，现有 `.vscode/tasks.json` 和 `.vscode/launch.json` 通常可以直接复用。
- 如果你需要运行图形界面模式，通常还需要依赖 `WSLg` 或额外的图形转发能力；这一路径当前**未单独验证**。
- 若你的工作主要是调试和回归测试，`stdio`、`headless` 与 `gdbstub` 一般已经足够。

## macOS 说明

由于设备原因，当前**未进行 macOS 实机测试**。

已知风险和限制如下：

- 仓库当前构建逻辑只支持 `Linux host`
- `macOS` 可能需要额外适配交叉编译工具链或构建脚本
- 本仓库目前没有提供经过验证的 `macOS` 安装和运行流程

如果你使用 `macOS`，欢迎自行尝试，并提交 `PR` 改进兼容性。

## 相关入口

- 根说明：`README.md`
- `QEMU` 运行脚本：`qemu/run_qemu.sh`
- `Bochs` 运行脚本：`bochs/run_bochs.sh`
- `CMake` 预设：`CMakePresets.json`
