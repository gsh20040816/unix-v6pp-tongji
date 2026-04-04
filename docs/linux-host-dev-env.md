# UnixV6++ Linux 主开发环境记录

## 背景

当前环境的职责划分如下：

- Linux 共享目录 `~/shared/UNIX V6++V1/oos` 是主开发目录
- Windows 虚拟机继续保留原有的 `MinGW + 批处理 + Makefile` 构建链
- Bochs 运行和 `gdbstub` 继续放在 Linux 本机
- VS Code 在 Linux 上打开工作区，但真正执行调试器的是 Windows 上的 `gdb.exe`

这样做的目标是保留旧工程对 Windows 构建链和 MinGW GDB 的依赖，同时把日常编辑、脚本入口和运行环境统一到 Linux 主目录。

## 最终工作流

当前推荐工作流如下：

1. 在 Linux 共享目录中编辑代码
2. 触发 `OOS make`
3. `build.sh` 通过 SSH 连接 Windows 主机
4. Windows 端进入 `Z:\UNIX V6++V1\oos\tools`
5. Windows 端调用 `oosvars_mingw.bat` 和 `build.bat`
6. Windows 端完成增量编译，并更新 `tools/v6pp-fs-edit-2022/workspace` 下的 boot、kernel、programs 产物
7. Linux 端使用 `tools/v6pp-fs-edit-2022` 的 `filescanner | fsedit` 生成 `c.img`，并同步到 `targets/UNIXV6++/c.img`
8. 调试时，VS Code 先执行 `OOS debug prep`
9. `OOS debug prep` 顺序执行 `OOS make` 和 `OOS linux debug host`
10. `debug.sh` 在 Linux 本机启动 `bochs-gdb -q -f bochsrc.bxrc`
11. VS Code 再通过 SSH 到 Windows 启动 `gdb.exe`
12. Windows 上的 GDB 连接 Linux 上 `10.200.65.1:1234` 的 `gdbstub`

## 目录与主机约定

当前仓库默认约定如下：

- Linux 工作区路径为 `~/shared/UNIX V6++V1/oos`
- Windows 工作区路径为 `Z:\UNIX V6++V1\oos`
- Windows SSH 主机别名为 `win`
- Linux 运行机地址为 `10.200.65.1`
- Linux 上 `BXSHARE` 默认使用 `/usr/share/bochs`
- Windows 上的 GDB 使用 `Z:\UNIX V6++V1\MinGW\bin\gdb.exe`

### 可覆盖的环境变量

- `OOS_WIN_HOST`
  - Windows SSH 目标，默认 `win`
- `OOS_WIN_REPO`
  - Windows 仓库路径，默认 `Z:\UNIX V6++V1\oos`
- `OOS_WIN_BUILD_COMMAND`
  - Windows 端自定义构建命令
- `OOS_LINUX_BXSHARE`
  - Linux 端 `BXSHARE`，默认 `/usr/share/bochs`
- `OOS_LINUX_DEBUG_COMMAND`
  - Linux 端调试启动命令
- `OOS_LINUX_RUN_COMMAND`
  - Linux 端非调试运行命令

## 仓库内脚本

### `tools/ssh-win.sh`

- 统一封装 Linux 到 Windows 的 SSH 入口
- 默认连接 `win`
- 供 `build.sh` 和 VS Code `pipeTransport` 共用
- 支持 `--shell` 打开一个交互式 Windows 远端终端

### `tools/pipe-win-gdb.sh`

- 专门给 VS Code `pipeTransport` 使用
- 本地启动一个很薄的 MI 代理，再由代理 SSH 到 Windows 启动 `gdb.exe`
- 负责把带空格的 Windows `gdb.exe` 路径包装成 PowerShell 可执行形式
- 同时把 VS Code 发出的 Linux 本地绝对路径断点，例如 `/home/.../main.cpp:48`，改写成老 `gdb 7.2` 可以接受的 `main.cpp:48`
- 这是为了解决当前仓库路径和 Windows 路径都带空格时，老 GDB 无法稳定解析 `-break-insert <absolute-path>:<line>` 的问题

### `build.sh`

- 在 Linux 上执行
- 默认通过 `tools/ssh-win.sh` 连接 Windows
- 默认执行：

```text
cmd.exe /c "cd /d Z:\UNIX V6++V1\oos\tools && call oosvars_mingw.bat && call build.bat"
```

- 作用是使用 Windows 原构建链完成增量编译，并把产物同步到 `tools/v6pp-fs-edit-2022/workspace`
- 编译完成后，`build.sh` 会在 Linux 本机通过 CMake 构建并运行 `tools/v6pp-fs-edit-2022`，生成 `workspace/c.img`，再复制到 `targets/UNIXV6++/c.img`
- `tools/v6pp-fs-edit-2022` 的镜像工具构建系统已切换为 Linux-only CMake，不再维护 Windows 兼容构建
- CMake 中间构建目录默认在 `.build-cache/v6pp-fs-edit-2022-cmake`，避免污染源码树
- 现在 `src` 下的 Makefile 已改成真正的增量行为：
  - 内核目标无变化时，不重新链接 `kernel.exe`
  - `lib`、`shell`、`program` 不再因 `build` 目标而全量重编
  - 只有发生变化的用户态程序才会重新生成并复制到 workspace
  - `c.img` 由 Linux 侧的 `filescanner | fsedit` 统一生成，不再依赖 Windows 执行 `filescanner.exe | fsedit.exe`

### `debug.sh`

- 在 Linux 本机执行
- 进入 `targets/UNIXV6++`
- 导出 `BXSHARE`
- 默认启动：

```text
bochs-gdb -q -f bochsrc.bxrc
```

- 用于调试前启动本机的 `gdbstub`

### `run.sh`

- 在 Linux 本机执行
- 进入 `targets/UNIXV6++`
- 导出 `BXSHARE`
- 默认启动：

```text
bochs -q -f bochsrc_nodebug.bxrc
```

- 用于不接 GDB 的普通运行

## VS Code 配置

### `.vscode/tasks.json`

当前任务已经调整为 Linux 主工作区模型：

- `OOS make`
  - 通过本机 `bash -lc ./build.sh`
  - Windows 端负责编译，Linux 端负责生成 `c.img`

- `OOS linux debug host`
  - 通过本机 `bash -lc ./debug.sh`
  - 作为后台任务运行
  - 当输出 `Enabled gdbstub` 时，认为 Linux 端调试环境已准备完成

- `OOS debug prep`
  - 顺序依赖 `OOS make` 和 `OOS linux debug host`
  - 统一处理调试前的构建和启动

### `.vscode/launch.json`

当前调试配置改成：

- `Bochs GDB (Windows GDB -> Linux 10.200.65.1)`
  - `preLaunchTask` 使用 `OOS debug prep`
  - `pipeTransport` 通过 `tools/pipe-win-gdb.sh` SSH 到 Windows
  - 远端调试器使用 `Z:\UNIX V6++V1\MinGW\bin\gdb.exe`
  - 程序符号文件使用 Windows 侧共享路径 `Z:\UNIX V6++V1\oos\targets\objs\kernel.exe`
  - `miDebuggerServerAddress` 连接 Linux 上的 `10.200.65.1:1234`
  - 通过 `sourceFileMap` 把 Windows 编译路径映射回 Linux 工作区

这样 VS Code 虽然运行在 Linux 上，但真正解析 PE 调试信息和执行 GDB 命令的是 Windows 端 MinGW GDB。

### `.vscode/settings.json`

当前只保留 Linux 本地会直接用到的设置：

- 继续使用 `clangd`
- 关闭 `C_Cpp` 的 IntelliSense 引擎
- 去掉 Windows 专用的 `--query-driver`
- `C_Cpp.default.compilerPath` 改成 Linux 本地的 `/usr/bin/g++`

## 手动使用方式

### 手动构建

```bash
bash build.sh
```

### 手动启动 Linux 调试目标

```bash
bash debug.sh
```

### 手动普通运行

```bash
bash run.sh
```

### 手动打开 Windows 远端终端

```bash
bash tools/ssh-win.sh --shell
```

## 这套方案解决了什么问题

- Linux 共享目录成为唯一主工作区
- Windows 旧构建链不需要迁移
- Bochs 仍然运行在更稳定的 Linux 环境
- 调试器继续使用兼容这套工程的 Windows MinGW GDB
- VS Code 的构建、运行、调试入口都统一到了仓库脚本中

## 当前限制

- `launch.json` 仍依赖较大的显式 `sourceFileMap`
- Windows GDB 依然要求 `win` SSH 可达，且共享盘映射保持一致
- `clangd` 现在不再查询 Windows 编译器内置头文件，若后续需要更完整的诊断，需要再补一层本地交叉工具链或更稳定的编译数据库转换
- Linux 主机构建镜像工具需要可用的 `cmake` 与 C++17 编译器
