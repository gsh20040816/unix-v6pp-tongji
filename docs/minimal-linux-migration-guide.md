# 基于 oos-old 派生系统的最小修改 Linux 迁移手册

## 1. 目的

本文档面向这样一种场景：

1. 你手上有一个基于 `oos-old` 继续开发过的系统。
2. 该系统目前依赖 Windows 下的 MinGW、`.exe` 工具和旧构建脚本。
3. 你希望把它迁移到 Linux 下构建运行。
4. 你希望尽量少改业务逻辑，只改“构建链、启动链、镜像工具链”以及少量与首轮用户态切换有关的代码。

本文档的核心目标不是把系统“现代化重构”，而是用最小补丁把系统重新带到可启动、可进入 shell 的状态。

## 2. 总体原则

迁移时优先遵循以下原则：

1. 不改系统架构，只改平台耦合点。
2. 不改磁盘布局、引导协议、页表布局、进程模型。
3. 不在第一轮迁移中改用户程序格式，继续保留 `PEParser` 和现有 PE 可执行文件。
4. 优先添加兼容层，不优先重写旧代码。
5. 优先修改少量关键文件，不要整目录替换。

这意味着第一轮 Linux 迁移的重点应是：

1. 让内核能在 Linux 下被正确编译、链接、装入。
2. 让镜像能在 Linux 下被正确生成。
3. 让系统能从内核态走到 1 号进程，再走到第一次 `exec("/Shell.exe")`。

## 3. 真正必须改的部分

### 3.1 构建链从 PE/COFF 切到 ELF32

这是最重要的一步。如果仍保留 `pei-i386`，Linux 下的 GCC/binutils 将无法按当前启动链正确生成内核镜像。

必须关注的文件：

1. `src/Link.ld`
2. `src/Makefile.inc`
3. `src/Makefile`
4. `src/boot/Makefile`

最低限度需要完成的修改：

1. 链接脚本的目标格式从 `pei-i386` 改为 `elf32-i386`。
2. 构建参数加入 `-m32`。
3. 显式关闭宿主 Linux 默认会带入的特性：
   - `-fno-pic`
   - `-fno-PIE`
   - `-fno-stack-protector`
   - `-fno-use-cxa-atexit`
   - `-fno-sized-deallocation`
4. 链接器使用 `ld -m elf_i386`。
5. Windows 路径分隔符 `\` 改为 `/`。
6. Windows 专用命令 `copy`、`del`、`dir /B` 改为 Linux 可用方案。

最小化建议：

1. 如果你的派生系统已经有很多脚本依赖 `kernel.exe` 这个文件名，那么可以保留输出文件名不变，只改其内部目标格式为 ELF。不要为了“命名规范”额外制造更多改动。
2. 如果你的派生系统里 `sector2.bin` 其实是 NASM 生成的 ELF 对象，也可以继续保留这个名字，不必为了名字再连带修改更多构建规则。

### 3.2 启动运行时符号要按 ELF 规则适配

`oos-old` 的启动代码明显带有 COFF/MinGW 时代的符号约定，Linux/ELF 下必须修。

必须关注的文件：

1. `src/boot/sector2.s`
2. `src/boot/support.c`
3. `src/Link.ld`

需要处理的点：

1. `sector2.s` 中对启动符号的引用要从旧的 COFF 风格改到 ELF 兼容形式。
2. `support.c` 不能只依赖旧式 `.ctors/.dtors`，最好同时支持 `.init_array`。
3. 链接脚本中需要导出 `__init_array_start` 和 `__init_array_end`。

建议采取的最小方案：

1. 保留 `_main()` 和 `_atexit()` 这套接口，不去引入新的运行时框架。
2. 在 `support.c` 中优先遍历 `.init_array`，同时保留旧的 `.ctors` 兼容逻辑。
3. 不要在第一轮迁移中尝试接入宿主系统标准运行时。

这样做的原因很简单：你的系统原本就是裸机内核，不应该因为迁移到 Linux 构建而被迫吸收 Linux 用户态运行时语义。

### 3.3 bootloader 读入内核的扇区数要重新匹配

即使代码逻辑不变，切到 Linux/ELF 后，`kernel.bin` 的大小通常也会变化。如果引导扇区还按旧大小读取，系统会在跳到高地址入口后异常。

必须关注的文件：

1. `src/boot/boot.s`

需要做的事：

1. 重新确认 `kernel.bin` 实际大小对应的扇区数。
2. 更新 `KERNEL_SIZE`。

最小化建议：

1. 第一轮可先手工维护 `KERNEL_SIZE`，确认系统恢复启动后，再考虑自动化。
2. 如果你的派生系统后续会频繁增长内核体积，再考虑在 Makefile 中自动把扇区数传给 NASM。

### 3.4 首次切到用户态时的缺页问题要单独修

这部分不是“Linux 构建必改”，但对能否真正进入 shell 非常关键。`oos-new` 的迁移历史表明，系统在“能进内核”之后，真正的卡点出现在第一次 `iret` 进入用户态和第一次 `exec()` 之间。

必须关注的文件：

1. `src/include/Regs.h`
2. `src/proc/MemoryDescriptor.cpp`
3. `src/kernel/main.cpp`

最小必要修复通常包括：

1. 用户态初始栈顶不要直接放在 `0x800000`，应放在最后一页内部，例如 `0x7FF000`。
2. 在第一次 `iret` 到用户态之前，先给用户空间做一层临时恒等映射，保证 trampoline 和首个 `exec()` 过程可执行。
3. 如果启动阶段汇编仍写死跳转到 `$_next`，而 ELF 符号名变成了 `next`，优先在 C/C++ 侧加一个 `_next` 别名，而不是去改更多汇编调用点。

兼容性优先级建议如下：

1. 优先加 `_next` 别名。
2. 优先补临时用户页表映射。
3. 优先修初始用户栈地址。
4. 最后再考虑更系统的页表整理。

### 3.5 修掉已经存在但以前未暴露的问题

Linux 迁移过程中，一些原本就存在的 bug 会被更早暴露出来。应优先修这种“旧 bug 被新环境放大”的问题，而不是误判为 Linux 平台本身的问题。

在 `oos-old` 与 `oos-new` 的对比中，最典型的一处是：

1. `src/proc/MemoryDescriptor.cpp` 中 `m_PageBaseAd1dress` 拼写错误。

这类错误在旧版本上未必一定触发，但在迁移后会直接影响页表映射逻辑，建议在迁移时顺手修正。

## 4. 需要 Linux 化，但应尽量少动的部分

### 4.1 镜像生成工具链

旧系统的镜像写入工具明显依赖 Windows：

1. `filescanner.exe`
2. `fsedit.exe`
3. 若干批处理和 Windows 路径语义

如果这部分不处理，即使内核编出来了，也无法稳定得到可启动镜像。

应关注的文件：

1. `src/Makefile`
2. `tools/v6pp-fs-edit-2022/src/FileScanner/src/main.cpp`
3. `tools/v6pp-fs-edit-2022/src/FsEditor/src/...`
4. 相关 makefile

最小迁移策略：

1. 保留原有镜像格式和指令协议。
2. 只把工具本身改到能在 Linux 下重新编译和运行。
3. 不要重写一套新的镜像工具。

特别注意：

1. `FileScanner` 最好不要再输出绝对路径。
2. 如果工程路径中含有中文，绝对路径会进入工具的路径解析逻辑。
3. 现有 `FsEditor` 的路径读取逻辑对非 ASCII 字符并不友好。

因此，最小改法是：

1. `filescanner` 输出相对路径。
2. `fsedit` 继续处理原有协议。

这样能避免因为工程位于中文路径下而使镜像打包失败。

### 4.2 Bochs 配置

Linux 下 Bochs 的 BIOS/VGA ROM 安装路径通常和 Windows 环境不同。

应关注的文件：

1. `targets/UNIXV6++/bochsrc.bxrc`

最小改法：

1. 只改 BIOS/VGA ROM 路径。
2. 只注释掉在 Linux 下容易出问题的键盘映射项。
3. 不要顺手调整 ATA 几何、内存大小、磁盘布局等与系统行为相关的配置，除非确实需要。

## 5. 明确不要动的部分

为了保证最小修改，第一轮迁移不建议动以下内容：

1. `PEParser` 的文件格式逻辑。
2. 用户程序的文件格式。
3. 进程调度器整体设计。
4. 文件系统布局。
5. ATA/DMA 主逻辑。
6. 内核高地址映射策略。
7. 用户态程序库接口。

换句话说，第一轮迁移的目标应该是：

1. 继续使用旧的用户程序格式。
2. 继续使用旧的 shell 装载路径。
3. 继续使用旧的磁盘镜像布局。
4. 继续使用旧的内核地址空间设计。

## 6. 推荐迁移顺序

建议严格按下面顺序做。不要一开始就把所有文件一起改掉。

### 阶段 1：只让内核在 Linux 下编译通过

先改：

1. `src/Link.ld`
2. `src/Makefile.inc`
3. `src/boot/Makefile`
4. `src/Makefile`
5. `src/boot/sector2.s`
6. `src/boot/support.c`

阶段目标：

1. 能生成 `kernel.bin`
2. 能生成 `boot.bin`
3. 链接产物不依赖 Windows 工具链

### 阶段 2：只让内核被 bootloader 正确装入

再改：

1. `src/boot/boot.s`

阶段目标：

1. bootloader 能把完整内核装入 1MB
2. 能跳到高地址入口
3. 能进入 `main0()`

### 阶段 3：只让镜像在 Linux 下构造成功

再改：

1. `src/Makefile` 的 deploy 流程
2. `tools/v6pp-fs-edit-2022/...`
3. `targets/UNIXV6++/bochsrc.bxrc`

阶段目标：

1. 能在 Linux 下生成 `c.img`
2. Bochs 能正常加载镜像

### 阶段 4：只解决第一次用户态切换

再改：

1. `src/include/Regs.h`
2. `src/proc/MemoryDescriptor.cpp`
3. `src/kernel/main.cpp`

阶段目标：

1. 不在第一次 `iret` 时触发 PF14
2. 能进入 `ExecShell()`
3. 能继续走到 `exec("/Shell.exe")`

### 阶段 5：只修派生系统自身新增逻辑暴露出的兼容问题

最后才处理你自己的系统与 `oos-old` 相比新增的那部分修改。

建议做法：

1. 先把公共迁移层做好。
2. 再看你自己的新增功能在哪个阶段挂掉。
3. 只修与该阶段直接相关的兼容问题。

## 7. 建议的最小文件清单

如果你的派生系统与 `oos-old` 差异已经很大，建议优先只对下面这些文件做人工迁移。

### 一级必改

1. `src/Link.ld`
2. `src/Makefile.inc`
3. `src/Makefile`
4. `src/boot/Makefile`
5. `src/boot/sector2.s`
6. `src/boot/support.c`
7. `src/boot/boot.s`

### 二级常见必改

1. `src/include/Regs.h`
2. `src/proc/MemoryDescriptor.cpp`
3. `src/kernel/main.cpp`

### 三级按需修改

1. `tools/v6pp-fs-edit-2022/src/FileScanner/src/main.cpp`
2. `tools/v6pp-fs-edit-2022/src/FsEditor/src/...`
3. `targets/UNIXV6++/bochsrc.bxrc`

## 8. 验证清单

每完成一个阶段，都建议做一次最小验证。

### 验证 1：编译链验证

检查点：

1. `make clean`
2. `make build`
3. 生成 `boot.bin`
4. 生成 `kernel.bin`
5. 无 Windows 命令依赖

### 验证 2：启动链验证

检查点：

1. Bochs 能启动
2. 跳转到 `greatstart`
3. 进入 `main0()`
4. 能执行到 `next()`

### 验证 3：用户态切换验证

检查点：

1. 0 号进程创建 1 号进程
2. 第一次 `MoveToUserStack()` 不发生 PF14
3. `ExecShell()` 被调用
4. 能走到 `PEParser`

### 验证 4：镜像内容验证

检查点：

1. `c.img` 中包含 bootloader
2. `c.img` 中包含 kernel
3. `programs` 被正确写入镜像
4. shell 程序可以被找到

## 9. 常见故障与定位顺序

### 症状 1：编译就失败

优先检查：

1. 是否还残留 Windows 路径分隔符
2. 是否还残留 `copy`、`del`、`dir /B`
3. 是否缺少 `-m32`
4. 是否没有关闭 PIE/PIC

### 症状 2：Bochs 启动后立即挂死

优先检查：

1. `KERNEL_SIZE` 是否仍是旧值
2. `kernel.bin` 是否被完整写入镜像
3. 是否确实跳到了 `0xC0100000`
4. `sector2.s` 的入口符号是否匹配

### 症状 3：进入内核后，切到用户态触发 PF14

优先检查：

1. `MoveToUserStack()` 的用户栈是否落在已映射页面中
2. 是否给第一次用户态入口做了临时映射
3. `MemoryDescriptor` 中页表项是否正确填写
4. 是否有旧拼写错误仍未修掉

### 症状 4：镜像生成成功，但 shell 找不到

优先检查：

1. `filescanner` 是否输出了 Linux 下不可解析的绝对路径
2. 中文路径是否进入了工具协议
3. `programs` 目录是否被正确写入镜像
4. shell 文件名和内核中的路径是否一致

## 10. 对派生系统的具体实施建议

如果你的系统是“基于 `oos-old` 又做了不少本地修改”的版本，建议按下面方式实施，以避免把你自己的改动冲掉。

1. 不要整文件直接用 `oos-new` 覆盖。
2. 先把差异分成三类：
   - 平台差异
   - 启动链差异
   - 你自己新增的功能差异
3. 先只迁平台差异和启动链差异。
4. 在能进 shell 之前，不要碰你自己新增功能的实现细节。
5. 对符号名、入口名、页表细节，优先加兼容别名和小补丁，而不是大重构。

尤其建议采用下面两条经验：

1. 能加别名就不要改一串汇编调用点。
2. 能在 Makefile 和工具链层解决的问题，就不要深入改内核逻辑。

## 11. 第一轮迁移的合理目标

对于一个 `oos-old` 派生系统，第一轮迁移的合理完成标准应该是：

1. Linux 下可以重新构建 `boot.bin` 和 `kernel.bin`
2. Linux 下可以重新生成 `c.img`
3. Bochs 下可以从镜像启动
4. 系统可以完成从 bootloader 到内核，再到 1 号进程的切换
5. 系统可以执行第一次 `exec("/Shell.exe")`
6. 至少可以进入 shell 或到达 `PEParser`

这已经足够证明迁移主干是成功的。之后再处理：

1. 更多用户程序的 Linux 重建
2. 工具链进一步清理
3. 你自己的派生功能恢复

## 12. 结论

对 `oos-old` 派生系统做 Linux 迁移时，真正应优先处理的不是“大量内核代码重写”，而是以下六件事：

1. ELF32 构建链
2. 启动符号与构造器初始化
3. bootloader 读入长度
4. Linux 下镜像工具链
5. 首次用户态切换的页表与栈
6. 少量旧 bug 清理

只要把这六类问题按顺序解决，通常就能在不大动系统主体逻辑的前提下，把一个基于 `oos-old` 的系统重新带回 Linux 可启动状态。
