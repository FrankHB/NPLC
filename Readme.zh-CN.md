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

　　确保已在其中配置 YSLib 库的位置后，选择 debug 或 release 配置，直接构建。

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

　　运行解释器选项 `-h` 查看帮助消息，包括支持的执行模式、选项和环境变量。

　　运行解释器可执行文件可直接进入 REPL 或加载脚本。使用命令行选项 `-e` ，支持直接求值字符串参数。

**注释** 选项 `-q` 或 `--no-start-file` 在一些 Scheme 语言的实现中通用，尽管模式和具体文件可能不同，如：

* [MIT Scheme](https://www.gnu.org/software/mit-scheme/documentation/stable/mit-scheme-user/Customizing-Scheme.html#Customizing-Scheme)
* [GNU Guile](https://www.gnu.org/software/guile/manual/html_node/Init-File.html)
* [Racket](https://docs.racket-lang.org/reference/running-sa.html#%28mod-path._racket%2Finteractive%29)

　　当前实现同时支持 `doc/NPL.txt` 中的函数和一些未公开的函数。

## 运行环境

　　构建默认依赖 YSLib 动态库。

　　V0.8 起初始化加载 `test.txt`（可为空）。若没有这个文件，初始化出错。

## 测试脚本

　　以下脚本提供 `valgrind` 间接调用解释器命令行示例：

* `td.sh` 调用 debug 模式解释器，默认调用 memcheck 检查泄漏。
* `tr.sh` 调用 release 模式解释器，默认调用 callgrind 收集性能信息。

　　`tr.sh` 输出在前缀为 `callgrind.out` 的文件中。

　　脚本支持环境变量进行配置：

* `CONF` ：配置名，决定解释器路径 `".$CONF/NBuilder.exe"` 。
* `LOGOPT` ：日志文件选项。
* `TOOLOPT` ：工具选项。

　　默认情形的配置和实际命令行参见对应的脚本源代码。

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
		* 增强命令行参数支持：
			* 支持多个 `-e` 选项求值字符串参数。
			* 支持参数指定脚本文件（其中 `-` 表示标准输入）。
			* 支持 `--` 参数后停止解析选项。
			* 支持以 - 为名的脚本视为标准输出。
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
		* 快速规约：优化合并子左值调用和名称查找的实现。
		* 使用 NBuilder 命名空间代替 NPL 并简化上下文类型名。
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
* **V1.5** NBuilder 调整部分实现语义和行为（依赖 YSLib build 962 ）。
	* 解释器实现：
		* 优化内部对象分配。
		* 调整部分字面量的输出格式。
		* 全面支持空字面量。
		* 修复浮点数外部表示。
			* 保证可往返转换（输出可被作为字面量读取）。
				* 保证不依赖区域。
			* 经有限次转换不继续损失精度。
		* 打印节点：添加非正规表示和非真列表支持；移除 `NodeSequence` 支持。
		* 快速规约：优化名称查找和合并子左值调用的实现。
		* 命令行参数支持：
			* 帮助消息明确模式和文本编码。
			* 修复选项 `--` 后无参数调用没有进入交互模式（自从 V1.2 ）。
			* 添加选项 `-q` 和 `--no-init-file` 支持在初始化时跳过加载启动文件。
	* 标准库实现（依赖 YSLib ）：
		* 修复 `$let` 家族空列表左值绑定。
		* 添加全局模块 `std.math` 。
		* 重命名函数 `$and?` 和 `$or?` 为 `$and` 和 `$or` 。
		* 修复 `equal?` 结果正规表示（自从 YSLib b904 ）。
	* 标准库扩展：
		* 添加库函数：
			* `stoi`
			* `write`
			* `logd`
			* `open-input-file`
			* `open-input-string`
			* `parse-stream`
		* 移除库函数：
			* `oss`
			* `ofs`
			* `parse-f`
		* 移除 YSLib 已实现的函数：
			* `put`（使用 `std.io` 中的绑定）
			* 算术操作（使用 `std.math` 中的绑定）
			* `string?`（使用 `std.strings` 中的绑定）
			* `list?`（使用核心库中的绑定）
		* 调整库函数：
			* `display` 输出格式。
			* 算术操作（使用 `std.math` 中的绑定）：
				* 多元算术操作调整为二元操作。
				* 支持一般数值类型而不仅 `int` 类型。
			* 重命名函数 `symbol?` 为 `basic-identifier?` ，使用 YSLib 中的 `symbol?` 代替原同名函数。
			* 转发函数 `$while` 和 `$until` 的参数。
			* 检查函数 `list-push-back!` 的列表参数并避免无条件转移。
		* 重命名库函数 `and?` 和 `or?` 为 `and` 和 `or` 。
		* 函数 `read-line` 使用分配器。
		* 修复库函数实现：
			* `map` 的实现没有支持转发构造列表的第二参数（自从 V0.10 ）。
			* `make-nocopy-fn` 的实现可能具有未定义行为（自从 V0.14 ）。
		* 添加扩展函数的文档。
	* 测试库和用例：
		* 在列表修改测试用例中添加检查并使用 `$expect` 代替 `display/new` 。
		* 修改关于列表修改的测试用例，不再依赖 `cons%` 对第二参数的提升。
		* 添加非真列表的测试用例。
		* 转发函数 `info` 和 `subinfo` 中的参数。
		* 合并部分测试用例集；调整部分测试用例标题顺序。
		* 添加子有序对引用的被引用对象的测试用例。
		* 添加更多 `equal?` 等库函数的测试用例。
		* 添加基于 `std.continuations` 实现的简单异常和测试用例。
		* 添加空动态环境名称的回归测试用例。
	* 添加调用 `valgrind` 的测试脚本。
* **V1.6** NBuilder 调整部分实现语义和行为（依赖 YSLib build 980 ）。
	* 解释器实现：
		* 从名称查找移除标识符检查，重构以支持更高效的派生类型的父环境。
		* 调整部分分配器使用和简化相关实现。
		* 修复未声明的名称（自从 V0.12 ）。
		* 修复抛出的异常中的资源释放可能超过解释器对象的生存期（自从 V0.10 ）。
		* 源代码调整：
			* 调整命名空间使用，简化头文件包含。
			* 裁剪特性：
				* 支持外部配置实现特性宏。
				* 添加配置宏，默认仅在调试模式提供项互操作特性。
				* 仅在启用调试动作时提供函数 $crash 和 trace 。
				* 添加配置宏，默认仅在调试模式提供函数 load-at-root 。
				* 添加配置宏，默认仅在调试模式提供函数 parse-stream 。
		* 调整和增强解释器输出：
			* 移除流缓冲设置，停用输出帮助消息时 C++ 输出流的缓冲。
			* 在帮助消息中默认启用终端颜色支持。
				* 终端颜色支持包括彩色输出和下划线格式。
				* 同时支持设置非空的环境变量 `NO_COLOR` 关闭颜色支持。
			* 调整帮助消息：
				* 使用分行输出选项同义词。
				* 默认启用终端颜色支持。
				* 显示通过框架支持的环境变量。
			* 平台信息添加 C++ 版本。
		* 修复处理非退出信号非预期地追加追踪记录（自从 V0.12 ）。
		* 优化追踪记录访问。
		* 调试模式初始化启用加载源代码时的异常处理。
	* 标准库扩展：
		* 移除 YSLib 已实现的函数：
			* `string-contains?`（使用 `std.strings` 中的绑定）
			* `string=?`（使用 `std.strings` 中的绑定）			
		* 修复库函数实现：
			* 修复 `get-wrapping-count` 的结果类型（自从 V0.10 ）。
			* 修复 `string-length` 的结果类型（自从 V0.7 ）。
			* 修复 `load-at-root` 异步实现（自从 V1.5 ）。
		* 简化函数 `load-at-root` 和 `$crash` 的本机实现。
		* 函数 `wrap1%` 仅在外部依赖可用时启用（当前 Win32 平台动态库配置默认不适用）。
	* 测试库和用例：
		* 添加若干测试用例。
		* 移除子标题中的冗余空格。
		* 重构和简化测试用例实现：
			* 提取公共 `$let` 测试函数。
			* 简化若干测试用例；不依赖 `eval%` 第一参数的左值到右值转换；使用 `eval@` 简化部分实现并转发参数。
			* 移除 `$check` 操作数的冗余括号。
			* 移除块结尾的冗余 `;` 。
			* 除 `$import!` 的测试用例外，使用 `$import&!` 代替 `$import!` 导入符号。
			* 重命名左值判断用例条件。
			* 使用派生合并子构造器代替 `$vau/e` 和 `$vau%` 简化测试用例。
			* `$sequence` 测试用例使用保留引用值的实现。
	* 更新 Code::Blocks 项目文件构建选项：
		* 修复缺失 `YB_DLL` 和 `YF_DLL` 宏（自从 V0.1 前）。
		* 同步编译器和链接器选项。
		* 调整部分选项顺序。

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

　　虽然测试脚本在 V1.5 起正式提供，先前的性能测试数据也可通过脚本调用，如：

```
./tr.sh -e '()pt'
```

## 元数据

　　在 NPLA1 元数据的基础上，支持以下续延名称：

* `guard-load` ：恢复加载前保存的环境。
* `load-external` ：加载外部翻译单元。
* `repl-print` ：REPL 输出。

