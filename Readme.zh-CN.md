# 概要

　　NPLC 是 NPL 命令行特性的试验项目。本文档是 NPLC 项目自述文件。

　　关于 NPL ，参见 [YSLib](https://bitbucket.org/FrankHB/yslib) 的 [Wiki 页面][和 YSLib 中的项目文档 `doc/NPL.txt` 。

# NBuilder

　　NBuilder 实现了 NPLA1 的扩展，包含解释器的优化实现和不提供文档的试验性特性（全局函数）。

　　NBuilder 基于 YSLib 中 YFramework 库的 NPL 模块实现。

　　除特定版本经发布注记明确以外的 NBuilder NPLA1 库 API 不保证稳定；当前不提供文档。除非另行指定，所有 NPLA1 库函数可能会被未来版本移至 YSLib 实现。

# 构建

　　NBuilder 支持多种构建方式。

## 使用 Code::Blocks 构建

　　确保已在其中配置 YSLib 库的位置后，选择 `debug` 或 `release` 配置，直接构建。

　　生成的可执行文件在配置名的目录下。

## 使用 YSLib [Sysroot](https://bitbucket.org/FrankHB/yslib/wiki/Sysroot.zh-CN.md)

```
bash
. SHBuild-BuildApp.sh
SHBuild_BuildApp . -xj,3
```

　　以上命令调用 `bash` 以避免覆盖当前 shell 。

　　若需指定配置，使用 `. SHBuild-BuildApp.sh -cdebug` 等代替以上第二行。不指定配置名称，默认同 `-crelease` 。

　　使用 debug 配置由配置名称推断，除非指定的名称包含 `debug` ，默认为 release 配置。

　　其它具体调用详见 Sysroot 文档。

　　生成的可执行文件在 `SHBuild-BuildApp.sh` 指定的配置名对应的带有前缀 `.` 的目录下（默认为 `.release` ）。

## 使用 YSLib [SHBuild](https://bitbucket.org/FrankHB/yslib/wiki/Tools/SHBuild.zh-CN.md) 工具

　　SHBuild 是 Sysroot 脚本调用的构建工具。具体使用方式详见工具对应的文档。

# 运行

　　运行解释器可执行文件直接进入 REPL ；或使用命令行选项 `-e` ，支持直接求值字符串参数。

　　当前实现支持 `doc/NPL.txt` 中的函数和一些未公开的函数。

## 运行环境

　　构建默认依赖 YSLib 动态库。

　　V0.8 起初始化加载 `test.txt`（可为空）。若没有这个文件，初始化出错。

# 发布注记

　　当前所有历史版本都是试验性测试版本，为 PreAlpha 开发阶段。

* **V0.1** 添加 NBuilder 解释器框架（依赖 YSLib V0.4 ）。
	* 添加命令行界面。
	* 添加控制台彩色输出。
* **V0.2** NBuilder 添加基本规约（依赖 YSLib build 495 ）。
	* 添加规约函数。
	* 修改日志记录的实现。
* **V0.3** NBuilder 简化命令行界面实现（依赖 YSLib build 520 ）。
	* 合并控制台 API 到 YSLib 。
* **V0.4** NBuilder 修复规约，简化规约和主函数实现（使用 YSLib build 662 ）。
	* 修复空列表访问。
	* 简化错误处理。
* **V0.5** NBuilder 添加 NPLA1 规约语义支持（使用 YSLib build 665 ）。
	* 添加值记号、简单的语义规约规则和基本语义节点输出。
	* 添加 NPLA1 函数。
		* 添加名称查找、变量绑定、λ 抽象和赋值形式。
		* 添加列表操作、算术操作和若干其它全局函数。
* **V0.6** NBuilder 解释实现更新（依赖 YSLib build 695 ）。
	* 明确支持的求值顺序。
	* 添加语法预处理。
		* 添加分隔符变换。
* **V0.7** NBuilder 解释实现更新（依赖 YSLib build 765 ）。
	* 添加扩展字面量支持。
	* 合并规约函数实现到 YSLib 。
	* 修复解释器命令行界面重定向。
	* 移除 Win32 依赖。
	* 添加和简化部分 NPLA1 函数。
* **V0.8** NBuilder 解释实现更新（依赖 YSLib build 804 ）。
	* 增强日志输出。
	* 扩展 NPLA1 初始环境。
		* 添加一等环境支持。
		* 添加部分 [Kernel](https://web.cs.wpi.edu/~jshutt/kernel.html) 兼容函数，实现 [vau 演算](http://www.wpi.edu/Pubs/ETD/Available/etd-090110-124904/unrestricted/jshutt.pdf)。
		* 添加外部模块加载支持。
		* 处理扩展字面量。
	* 添加调试功能。
* **V0.9** NBuilder 解释实现更新（依赖 YSLib build 839 ）。
	* 处理主函数异常。
	* 调整 NPLA1 初始环境：
		* 添加若干 NPLA1 函数。
		* 移除在 YSLib 中实现的 NPLA1 函数。
		* 调整部分其它 NPLA1 函数。
		* 初始化 NPLA1 函数使用 std 模块封装。
		* 使用 YSLib 简化模块加载实现。
	* 添加初始化计时输出。
* **V0.10** NBuilder 调整 NPLA1 实现（依赖 YSLib build 861 ）。
	* 使用资源池和分配器优化实现性能。
	* 增强调试功能。
	* 调整 NPLA1 初始环境：
		* 添加临时对象销毁顺序测试（默认不启用）。
		* 添加、移除和重命名若干 NPLA1 函数。
		* 使用 YSLib 流类型代替 C++ 标准库流。
* **V0.11** NBuilder 调整 NPLA1 实现（依赖 YSLib build 878 ）。
	* 调整 NPLA1 初始环境：
		* 移除和 std.system 重复的根环境中的定义。
		* 简化导入模块。
		* 添加和重命名若干 NPLA1 函数。
		* 转移派生库实现到外部模块。
	* 优化池资源。
	* 默认关闭交互式调试。
	* 添加派生库扩展和基本测试。
	* 移除项检查。
* **V1.0** NBuilder 优化 NPLA1 实现（依赖 YSLib build 887 ）。
	* 添加依赖异步规约的快速规约。
	* 简化 REPL 上下文构造。
	* 简化扩展字面量处理。
	* 更新资源池。
		* Linux 平台可选支持 mimalloc 。
		* 修复资源池的实现。
		* 优化分配参数。
	* 添加 `-e` 选项支持直接求值命令行字符串参数。
* **V1.1** NBuilder 改进诊断消息（依赖 YSLib build 896 ）。
	* 改进诊断消息。
		* 编码部分类型名称代替宿主类型名称。
		* 默认保存源信息，并在未声明的标识符的错误消息中使用。
		* 支持错误时的续延追踪。
	* 加载翻译单元保证在尾上下文求值、取消嵌套加载异常并支持续延追踪；对应函数返回翻译单元表达式中的求值结果代替 `#inert` 。
	* 调整 REPL 输出。
		* 使用紧凑格式在 REPL 显示列表。
		* 调整部分数据类型的输出格式。
	* 标准库扩展：
		* 移除 YSLib 已实现的函数：
			* `load`（使用 `std.io` 中的绑定）
* **V1.2** NBuilder 调整部分实现语义和行为（依赖 YSLib build 907 ）。
	* 标准库扩展：
		* 移除 YSLib 已实现的函数：
			* `assv`
			* `equal?`
			* `load`（使用 `std.io` 中的绑定）
			* `puts`（使用 `std.io` 中的绑定）
		* 添加标准库扩展函数 `list-push-back!` 。
		* 修复 `$remote-eval%` 结果冗余提升。
		* 拆分扩展标准库实现为模块并在初始化后冻结。
	* 添加和修复部分测试用例。 
	* 修复初始化时的环境资源泄漏（自 V1.1 ）。
	* 修复部分诊断输出。
	* 修复 Win32 环境下编码处理错误依赖当前代码页。
* **1.3** NBuilder 调整部分实现语义和行为（依赖 YSLib build 925 ）。
	* 解释器实现：
		* 调整环境值的输出格式。
		* 标准错误输出支持单独的终端。
		* 用户界面输出除提示外使用标准错误输出。
		* 明确 REPL 输出的平台字符串。
		* 优化字面量处理的实现。
	* 标准库实现（依赖 YSLib ）：
		* 部分核心库函数和 `std.promises` 等使用本机实现。
	* 标准库扩展：
		* 添加库函数：
			* `string-contains?`
			* `iota`（移动自测试脚本）
			* `iota-range`（移动自测试脚本）
			* `make-nocopy`
			* `$while`
			* `$until`
		* 移除 YSLib 已实现的函数：
			* `as-const`
			* `$remote-eval`
			* `$remote-eval%`
	* 拆分初始化和测试脚本。
	* 添加和修复部分测试用例。 
	* 增强命令行参数支持：
		* 支持多个 `-e` 选项求值字符串参数。
		* 支持参数指定脚本文件（其中 `-` 表示标准输入）。
		* 支持 `--` 参数后停止解析选项。
		* 支持以 - 为名的脚本视为标准输出。
* **V1.4** NBuilder 调整部分实现语义和行为（依赖 YSLib build 945 ）。
	* 解释器实现：
		* 优化内部对象分配。
		* 调整部分字面量的输出格式。
		* 全面支持空字面量。
		* 修复浮点数外部表示。
			* 保证可往返转换（输出可被作为字面量读取）。
				* 保证不依赖区域。
			* 经有限次转换不继续损失精度。
		* 修复一个项的嵌套列表中的非列表值的求值（自从 V0.11 ）。
		* 优化快速规约实现。
	* 标准库实现（依赖 YSLib ）：
		* 修复 `$let` 家族空列表左值绑定。
		* 添加全局模块 `std.math` 。
		* 重命名函数 `$and?` 和 `$or?` 为 `$and` 和 `$or` 。
	* 标准库扩展：
		* 添加库函数：
			* `stoi`
			* `write`
			* `logd`
			* `make-nocopy-fn`
		* 移除 YSLib 已实现的函数：
			* `put`（使用 `std.io` 中的绑定）
			* 算术操作（使用 `std.math` 中的绑定）
			* 函数 get-module（使用 `std.io` 中的绑定）
			* 函数 `assq`
		* 调整库函数功能：
			* `display` 输出格式。
			* 算术操作（使用 `std.math` 中的绑定）：
				* 多元算术操作调整为二元操作。
				* 支持一般数值类型而不仅 `int` 类型。
		* 重命名函数 `and?` 和 `or?` 为 `and` 和 `or` 。
		* 修复函数 `load-at-root` 的异步规约实现使用的守卫并调整续延名称。
		* 修复函数 `memq?` 和 `memv?` 的实现（自从 V1.2 ）。
	* 添加和优化部分测试用例。

# 实现注记

　　NBuilder 优化解释器的快速规约依赖 YSLib NPLA1 实现的异步规约，且不支持其中的可扩展规约定制特性。

## 性能

　　因为语言特性的要求（能捕获一等续延，尽管当前不提供公开 API ）和使用 AST 解释器实现，性能较大多数一般的语言解释器实现低。

　　以下只给出基准测试用例的比较，使用支持类似特性且同为异步调用风格的 AST 解释器的 Kernel 实现 [klisp](https://klisp.org) 0.3（只支持 32 位）在 MinGW32 下的 release 配置。

　　兼容 POSIX shell 的命令行调用如下：

```
NBuilder -e '$defl! fib (&n) ($if (<=? n 1) n (+ (fib (- n 1)) (fib (- n 2)))); iput (fib 30)'
```

　　对应的 klisp 调用如下：

```
klisp -e '(display ($sequence ($define! fib ($lambda (n) ($if (<=? n 1) n (+ (fib (- n 1)) (fib (- n 2)))))) (fib 30)))'
```

使用 `time` 运行，估算 NBuilder/klisp 运行时间比例如下（误差约 10% ）：

* **V0.9** 约 5.50
* **V0.10** 约 3.50
* **V0.11** 约 1.80
* **V1.0** 约 0.95

　　（使用较小参数的基准测试内容另见 `test.txt` 中的 `pt` 函数。）

## 元数据

　　在 NPLA1 元数据的基础上，支持以下续延名称：

* `guard-load` ：恢复加载前保存的环境。
* `load-external` ：加载外部翻译单元。
* `repl-print` ：REPL 输出。

