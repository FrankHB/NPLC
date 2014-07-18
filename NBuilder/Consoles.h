/*
	© 2013-2014 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Consoles.h
\ingroup NBuilder
\brief 控制台。
\version r95
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 11:01:12 +0800
\par 修改时间:
	2014-07-19 07:51 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Consoles
*/


#ifndef INC_NPL_Consoles_h_
#define INC_NPL_Consoles_h_

#include <YCLib/YModules.h>
#include <YSLib/Core/YModules.h>
#include YFM_YCLib_Video
#include <windows.h>

/// 304
namespace Consoles
{

using namespace platform::Consoles;

/// 327
yconstexpr Color DefaultColor(Gray), TitleColor(Cyan),
	InfoColor(White), ErrorColor(Red), PromptColor(DarkGreen),
	SignalColor(DarkRed), SideEffectColor(Yellow), ReducedColor(Magenta);


/// 304
class WConsole
{
private:
	::HANDLE hConsole;

public:
	WConsole();
	~WConsole();

	inline void
	SetColor(std::uint8_t color = DefaultColor)
	{
		::SetConsoleTextAttribute(hConsole, color);
	}

	static inline void
	SetSystemColor()
	{
		std::system("COLOR");
	}
	static void
	SetSystemColor(std::uint8_t);

	static inline void
	Clear()
	{
		std::system("CLS");
	}
};

} // namespace Consoles;

#endif

