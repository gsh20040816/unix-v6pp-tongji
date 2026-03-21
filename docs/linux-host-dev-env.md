# UnixV6++ Linux 宿主机开发环境搭建记录

## 背景

这套环境的目标是把职责拆开：

- Windows 虚拟机内继续使用原有的 `MinGW + 批处理 + Makefile` 构建链
- Linux 宿主机负责运行 `bochs-gdb` 的 `term` 模式
- VS Code 在 Windows 虚拟机内编辑代码，并通过 GDB 远程连接宿主机上的 `bochs-gdb`

这样可以同时保留现有工程的 Windows 构建链，又获得 Linux 宿主机上更稳定的 `Bochs term` 调试体验。

## 最终工作流

当前工作流如下：

1. 在 Windows 虚拟机内编辑代码
2. 触发 `OOS make`
3. `build.sh` 调用 `tools/all.bat`
4. `make all` 增量编译，并更新 `targets/UNIXV6++/c.img`
5. 调试时，VS Code 先执行 `OOS debug prep`
6. `OOS debug prep` 顺序执行 `OOS make` 和 `OOS host run`
7. `runhost.sh` 通过 SSH 启动 Linux 宿主机上的 `bochs-gdb`
8. VS Code 再用 GDB 远程连接宿主机的 `gdbstub`

## 目录映射和宿主机约定

当前环境采用以下约定：

- Windows 虚拟机中的仓库路径是 `Z:\UNIX V6++V1\oos`
- Linux 宿主机中的共享目录路径是 `~/shared/UNIX V6++V1/oos`
- Linux 宿主机 SSH 用户是 `gsh`
- Linux 宿主机地址是 `10.200.65.1`
- 宿主机上 `BXSHARE` 使用 `/usr/share/bochs`
- 宿主机运行命令为 `bochs-gdb -q -f bochsrc.bxrc`

## 仓库内新增和修改的脚本

### 根目录脚本

- `build.sh`
  - 提供 Git Bash 入口
  - 先切到 `tools`
  - 清理 Git Bash 自带的 `PATH` 干扰，避免 `sh.exe` 影响 MinGW `make`
  - 载入 `oosvars_mingw.bat`
  - 调用 `all.bat`
  - 当前作用是增量编译并更新 `c.img`

- `run.sh`
  - 在本机工作目录 `targets/UNIXV6++` 下启动 `bochs -q -f bochsrc.bxrc`
  - 同样先加载 Windows 构建环境

- `runhost.sh`
  - 提供 Windows 虚拟机到 Linux 宿主机的统一运行入口
  - 默认连接 `gsh@10.200.65.1`
  - 自动推导宿主机共享路径 `~/shared/UNIX V6++V1/oos`
  - 如果本地没有 SSH 密钥，会自动生成 `~/.ssh/oos_host_ed25519`
  - 首次连接时会自动安装公钥到宿主机
  - 通过 `ssh -tt` 分配终端，并补上 `TERM`
  - 默认在宿主机的 `targets/UNIXV6++` 目录中执行 `bochs-gdb -q -f bochsrc.bxrc`

### 原有批处理链的使用方式

- `tools/build.bat`
  - 原本只执行 `make build`

- `tools/all.bat`
  - 执行 `make all`
  - `all` 会串联 `build` 和 `deploy`
  - `deploy` 会更新 `c.img`

当前已经把根目录的 `build.sh` 改成调用 `tools/all.bat`，因此 `OOS make` 现在不再只是编译内核，而是会同步更新镜像。

## VS Code 配置修改

### 任务

`.vscode/tasks.json` 已调整为当前工作流：

- `OOS make`
  - 通过 `C:\Program Files\Git\bin\bash.exe -lc ./build.sh` 触发完整增量构建

- `OOS host run`
  - 通过 `C:\Program Files\Git\bin\bash.exe -lc ./runhost.sh` 启动宿主机上的 `bochs-gdb`
  - 作为后台任务运行
  - 使用自定义 `problemMatcher`
  - 当终端输出 `Enabled gdbstub` 时，认为宿主机已准备好供 GDB 连接

- `OOS debug prep`
  - 顺序依赖 `OOS make` 和 `OOS host run`
  - 用于调试前统一准备

### 调试

`.vscode/launch.json` 已配置 GDB 远程调试：

- `Bochs GDB (Host 10.200.65.1)`
  - 使用 `preLaunchTask: "OOS debug prep"`
  - 程序符号文件使用 `targets/objs/kernel.exe`
  - 通过 `miDebuggerServerAddress: 10.200.65.1:1234` 连接宿主机 `gdbstub`

- `Bochs GDB (Localhost 1234)`
  - 保留本机回环地址调试入口

为兼容当前这套 `MinGW + GDB + 老工程` 组合，`launch.json` 中保留了显式 `sourceFileMap`。原因是这份 GDB 对断点路径格式很敏感，仅用目录级映射不足以稳定命中断点。

## 调试信息格式修改

为了修复源码行号和断点错位问题，`src/Makefile.inc` 的调试参数做了调整：

- 从旧的 `-g`
- 改为 `-g3 -gdwarf-2`

这样生成的 `kernel.exe` 使用的是 DWARF 调试信息，而不是旧式的 STABS，更适合现在的 GDB 和 VS Code 联动。

## clangd 和 IntelliSense 相关改动

### 选择 clangd

当前工作区已经切换为 `clangd` 作为主要语言服务：

- `.vscode/settings.json`
  - 关闭 `C_Cpp` 的 IntelliSense 引擎
  - 保留 `cpptools` 仅用于调试
  - 指定 `clangd` 使用 `.vscode/compile_commands.json`
  - 配置 `--query-driver=Z:/UNIX V6++V1/MinGW/bin/*`

- `.clangd`
  - 指定 `CompilationDatabase: .vscode`
  - 补充 freestanding 工程所需的基本编译选项

### 生成 compile_commands.json

新增 `tools/update_compile_commands.ps1`，作用如下：

- 通过 `make -B -n all` 抓取完整 dry-run 编译命令
- 解析多层 `make` 的 `Entering/Leaving directory`
- 只提取 `gcc/g++` 的实际源文件编译命令
- 将相对 `-I` 路径转换为绝对路径
- 将源文件路径转换为绝对路径
- 统一把 `directory` 写成仓库根路径
- 通过临时文件再覆盖的方式生成 `.vscode/compile_commands.json`

之所以这样做，是为了缓解共享盘路径和旧构建脚本对 `clangd` 的影响。

### 规范化 include 路径

为了避免 `clangd` 在跳转头文件时生成 `.` 开头的伪路径，工程里一批 `#include` 被统一从反斜杠风格改成了正斜杠风格，例如：

- `#include "..\test\TestInclude.h"` 改成 `#include "../test/TestInclude.h"`
- `#include "mm\TestAllocator.h"` 改成 `#include "mm/TestAllocator.h"`

这类修改主要出现在：

- `src/kernel/main.cpp`
- `src/test/*`

目的是提升头文件跳转和索引稳定性，不改变编译语义。

## 当前宿主机调试入口

现在推荐的使用方式是：

### 手动构建

```bash
bash build.sh
```

### 手动启动宿主机上的 Bochs

```bash
bash runhost.sh
```

### 在 VS Code 中调试

直接选择：

```text
Bochs GDB (Host 10.200.65.1)
```

VS Code 会自动：

1. 执行 `OOS make`
2. 增量编译并更新 `c.img`
3. 执行 `OOS host run`
4. 连接宿主机上的 `gdbstub`

## 这套方案解决了什么问题

- 保留了工程原本的 Windows 构建链，不需要整体迁移到 WSL
- 利用了 Linux 宿主机更适合 `Bochs term` 的运行环境
- 避免了每次手工登录宿主机再敲启动命令
- 让 VS Code 调试前自动做增量构建和镜像更新
- 让 `clangd` 在这套旧工程上基本可用
- 修复了调试信息格式导致的源码错位问题
- 修复了 Windows 风格 `#include` 路径导致的跳转异常

## 已知限制

- 当前 `launch.json` 仍然依赖较大的显式 `sourceFileMap`
- 工作区位于共享盘 `Z:`，LLVM 工具链在这种路径上仍可能有边缘兼容性问题
- `clangd` 虽然已可用，但这套共享目录方案仍不如本地 NTFS 工作区稳定

## 后续可继续改进的方向

- 为 `sourceFileMap` 增加自动生成脚本，减少后续维护成本
- 补充 `.gitignore`，忽略 `.vscode/compile_commands.json`、日志和生成物
- 增加 `.vscode/extensions.json`，把 `clangd`、`cpptools` 等扩展推荐固化进仓库
- 增加一键刷新 `compile_commands.json` 的 VS Code 任务
- 如果后续 `clangd` 仍偶发异常，可考虑把编辑工作区迁到本地 NTFS 路径，只同步构建产物到共享目录
