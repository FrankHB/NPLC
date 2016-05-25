/*
	© 2013-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.h
\ingroup NBuilder
\brief NPL 解释器。
\version r137
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2016-05-26 02:36 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Interpreter
*/


#ifndef INC_NPL_Interpreter_h_
#define INC_NPL_Interpreter_h_

#include <Helper/YModules.h>
#include "NPLContext.h"
#include YFM_Win32_YCLib_Consoles
#include YFM_YSLib_Core_YConsole
#include YFM_Helper_Environment
#include <iosfwd>
#include <functional>

namespace NPL
{

/// 592
using namespace YSLib::Consoles;
/// 674
using YSLib::Logger;
/// 674
using A1::NPLContext;

/// 304
enum class SSignal
{
	Exit,
	ClearScreen,
	About,
	Help,
	License
};


/// 673
void
LogTree(const ValueNode&, Logger::Level = YSLib::Debug);


/*!
\build 控制台默认颜色。
\since YSLib build 327
*/
yconstexpr Color DefaultColor(Gray), TitleColor(Cyan),
	InfoColor(White), ErrorColor(Red), PromptColor(DarkGreen),
	SignalColor(DarkRed), SideEffectColor(Yellow), ReducedColor(Magenta);


/// 304
class Interpreter
{
private:
	/// 520
	platform_ex::WConsole wc;
	/// 674
	YSLib::RecordLevel err_threshold;
	/// 689
	YSLib::unique_ptr<YSLib::Environment> p_env;
	/// 674
	string line;
	NPLContext context;

public:
	/// 694
	Interpreter(YSLib::Application&, std::function<void(NPLContext&)>);

	void
	HandleSignal(SSignal);

	bool
	Process();

	std::istream&
	WaitForLine();
	/// 696
	std::istream&
	WaitForLine(std::istream&, std::ostream&);
};

} // namespace NPL;

#endif

