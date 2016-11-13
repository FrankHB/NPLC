/*
	© 2012-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.h
\ingroup NBuilder
\brief NPL 解释实现。
\version r2033
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-04-23 15:25:02 +0800
\par 修改时间:
	2016-11-13 04:05 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#ifndef INC_NPL_NBuilder_h_
#define INC_NPL_NBuilder_h_

#include "Interpreter.h"

namespace NPL
{

/// 674
using namespace YSLib;

namespace A1
{

/// 674
void
RegisterLiteralSignal(ContextNode&, const string&, SSignal);


/// 696
//@{
template<typename _func>
void
DoIntegerBinaryArithmetics(_func f, TermNode& term)
{
	Forms::QuoteN(term, 2);

	auto i(term.begin());
	const int e1(Access<int>(Deref(++i)));

	// TODO: Remove 'to_string'?
	term.Value = to_string(f(e1, Access<int>(Deref(++i))));
}

template<typename _func>
void
DoIntegerNAryArithmetics(_func f, int val, TermNode& term)
{
	const auto n(Forms::FetchArgumentN(term));
	auto i(term.begin());
	const auto j(ystdex::make_transform(++i, [](TNIter i){
		return Access<int>(Deref(i));
	}));

	// FIXME: Overflow?
	term.Value = to_string(std::accumulate(j, std::next(j, n), val, f));
}
//@}

} // namespace A1;

} // namespace NPL;

#endif

