/*
	Copyright by FrankHB 2013.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.h
\ingroup NBuilder
\brief NPL 解释器。
\version r68
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2013-05-09 17:54 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Interpreter
*/


#ifndef INC_NPL_Interpreter_h_
#define INC_NPL_Interpreter_h_

#include <YSLib/Adaptor/ycont.h>
#include "NPLContext.h"
#include "Consoles.h"
#include <iosfwd>
#include <functional>

YSL_BEGIN_NAMESPACE(NPL)

/// 403
extern list<string> GlobalPath;


/// 304
class Interpreter
{
private:
	Consoles::WConsole wc;
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

YSL_END_NAMESPACE(NPL)

#endif

