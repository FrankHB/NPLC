/*
	© 2013-2014 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.h
\ingroup NBuilder
\brief NPL 解释器。
\version r87
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2014-07-19 09:20 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Interpreter
*/


#ifndef INC_NPL_Interpreter_h_
#define INC_NPL_Interpreter_h_

#include <YSLib/Adaptor/YModules.h>
#include YFM_YSLib_Adaptor_YContainer
#include "NPLContext.h"
#include YFM_MinGW32_YCLib_Consoles
#include <iosfwd>
#include <functional>

namespace NPL
{

/// 403
extern list<string> GlobalPath;

/// 519
using namespace platform::Consoles;

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
	YSLib::LoggedEvent::LevelType err_threshold;
	YSLib::string line;
	NPLContext context;

public:
	/// 403
	Interpreter(std::function<void(NPLContext&)>);

	void
	HandleSignal(SSignal);

	bool
	Process();

	std::istream&
	WaitForLine();
};

} // namespace NPL;

#endif

