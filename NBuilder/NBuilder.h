/*
	Copyright (C) by Franksoft 2012.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.h
\ingroup NBuilder
\brief NPL 解释实现。
\version r1785;
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 304 。
\par 创建时间:
	2012-04-23 15:25:02 +0800;
\par 修改时间:
	2012-09-02 19:40 +0800;
\par 文本编码:
	UTF-8;
\par 模块名称:
	NBuilder::NBuilder;
*/


#ifndef YSL_INC_NPL_NBUILDER_H_
#define YSL_INC_NPL_NBUILDER_H_

#include "NPLContext.h"
#include <YCLib/NativeAPI.h>
#include <YSLib/Adaptor/ycont.h>
#include <YSLib/Core/ystring.h>

///334
extern NPL::ValueNode GlobalRoot;


YSLib::string
SToMBCS(YSLib::Text::String, int);

YSL_BEGIN_NAMESPACE(NPL)

///334
void
EvalS(const ValueNode&);
///334
template<typename _type>
void
EvalS(const _type& arg)
{
	EvalS(SContext::Analyze(arg));
}

YSL_END_NAMESPACE(NPL)

/// 304
YSL_BEGIN_NAMESPACE(Consoles)

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

	void
	PrintError(const char*);
	void
	PrintError(const YSLib::LoggedEvent&);
};

YSL_END_NAMESPACE(Consoles)

#endif

