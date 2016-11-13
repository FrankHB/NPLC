/*
	© 2012-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.cpp
\ingroup Adaptor
\brief NPL 上下文。
\version r2223
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-11-13 17:52 +0800
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

/// 674
using namespace YSLib;
/// 676
using namespace std::placeholders;

namespace NPL
{

namespace A1
{

void
LoadSequenceSeparators(ContextNode& ctx, EvaluationPasses& passes)
{
	RegisterSequenceContextTransformer(passes, ctx, "$;", string(";")),
	RegisterSequenceContextTransformer(passes, ctx, "$,", string(","));
}

void
LoadDeafultLiteralPasses(ContextNode& ctx)
{
	AccessLiteralPassesRef(ctx)
		= [](TermNode& term, ContextNode&, string_view id) -> bool{
		YAssertNonnull(id.data());
		if(!id.empty())
		{
			const char f(id.front());

			// NOTE: Handling extended literals.
			if((f == '#'|| f == '+' || f == '-') && id.size() > 1)
				;
			else if(std::isdigit(f))
			{
				errno = 0;

				const auto ptr(id.data());
				char* eptr;
				const long ans(std::strtol(ptr, &eptr, 10));

				if(size_t(eptr - ptr) == id.size() && errno != ERANGE)
					term.Value = int(ans);
			}
			else
				return true;
		}
		return {};
	};
}

} // namespace A1;

} // namespace NPL;

