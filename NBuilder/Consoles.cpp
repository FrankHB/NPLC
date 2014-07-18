/*
	© 2013-2014 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Consoles.cpp
\ingroup NBuilder
\brief 控制台。
\version r73
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 11:01:35 +0800
\par 修改时间:
	2014-07-19 07:52 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#include "Consoles.h"

namespace Consoles
{

WConsole::WConsole()
	: hConsole(::GetStdHandle(STD_OUTPUT_HANDLE))
{
	YAssert(hConsole && hConsole != INVALID_HANDLE_VALUE,
		"Wrong handle found;");
}
WConsole::~WConsole()
{
	SetColor();
}

void
WConsole::SetSystemColor(std::uint8_t color)
{
	char cmd[9];

	std::sprintf(cmd, "COLOR %02x", int(color));
	std::system(cmd);
}

} // namespace Consoles;

