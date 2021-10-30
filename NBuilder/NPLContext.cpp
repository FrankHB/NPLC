﻿/*
	© 2012-2017, 2020-2021 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.cpp
\ingroup Adaptor
\brief NPL 上下文。
\version r2421
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2021-10-30 17:40 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#include "NPLContext.h"
#include YFM_NPL_SContext
#include <ystdex/container.hpp>
#include <iostream>
#include YFM_NPL_NPLA1

//! \since YSLib build 674
using namespace YSLib;
//! \since YSLib build 676
using namespace std::placeholders;

namespace NPL
{

namespace A1
{

bool
HandleCheckedExtendedLiteral(TermNode& term, string_view id)
{
	YAssertNonnull(id.data());

	const char f(id.front());

	// NOTE: Handling extended literals.
	if(!id.empty() && IsNPLAExtendedLiteralNonDigitPrefix(f) && id.size() > 1)
	{
		// TODO: Support numeric literal evaluation passes.
		if(id == "+inf.0")
			term.Value = std::numeric_limits<double>::infinity();
		else if(id == "-inf.0")
			term.Value = -std::numeric_limits<double>::infinity();
		else if(id == "+inf.f")
			term.Value = std::numeric_limits<float>::infinity();
		else if(id == "-inf.f")
			term.Value = -std::numeric_limits<float>::infinity();
		else if(id == "+inf.t")
			term.Value = std::numeric_limits<long double>::infinity();
		else if(id == "-inf.t")
			term.Value = -std::numeric_limits<long double>::infinity();
		else if(id == "+nan.0")
			term.Value = std::numeric_limits<double>::quiet_NaN();
		else if(id == "-nan.0")
			term.Value = -std::numeric_limits<double>::quiet_NaN();
		else if(id == "+nan.f")
			term.Value = std::numeric_limits<float>::quiet_NaN();
		else if(id == "-nan.f")
			term.Value = -std::numeric_limits<float>::quiet_NaN();
		else if(id == "+nan.t")
			term.Value = std::numeric_limits<long double>::quiet_NaN();
		else if(id == "-nan.t")
			term.Value = -std::numeric_limits<long double>::quiet_NaN();
		else
			return true;
		return {};
	}
	return true;
}

} // namespace A1;

} // namespace NPL;


