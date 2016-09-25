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
\version r1987
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-04-23 15:25:02 +0800
\par 修改时间:
	2016-09-25 23:53 +0800
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
//@{
bool
ExtractModifier(TermNode::Container&, const ValueObject& = string("!"));
inline PDefH(bool, ExtractModifier, const TermNode& term,
	const ValueObject& mod = string("!"))
	ImplRet(ExtractModifier(term.GetContainer(), mod))

void
ReduceTail(TermNode&, ContextNode&, TNIter);

void
RemoveHeadAndReduceAll(TermNode&, ContextNode&);

void
RegisterLiteralSignal(ContextNode&, const string&, SSignal);
//@}


/// 696
//@{
inline size_t
CheckTermSize(TermNode& term)
{
	const auto n(term.size());

	YAssert(n != 0, "Invalid term found.");
	return n - 1;
}

template<typename _func>
void
DoIntegerBinaryArithmetics(_func f, TermNode& term)
{
	const auto n(CheckTermSize(term));

	if(n == 2)
	{
		auto i(term.begin());
		const auto e1(std::stoi(Access<string>(Deref(++i))));

		// TODO: Remove 'to_string'?
		term.Value
			= to_string(f(e1, std::stoi(Access<string>(Deref(++i)))));
	}
	else
		throw ArityMismatch(2, n);
}

template<typename _func>
void
DoIntegerNAryArithmetics(_func f, int val, TermNode& term)
{
	const auto n(CheckTermSize(term));
	auto i(term.begin());
	const auto j(ystdex::make_transform(++i, [](TNIter i){
		return std::stoi(Access<string>(Deref(i)));
	}));

	// FIXME: Overflow?
	term.Value = to_string(std::accumulate(j, std::next(j, n), val, f));
}

template<typename _func>
void
DoUnary(_func f, TermNode& term)
{
	const auto n(CheckTermSize(term));

	if(n == 1)
		// TODO: Assignment of void term.
		f(Access<string>(Deref(std::next(term.begin()))));
	else
		throw ArityMismatch(1, n);
	term.ClearContainer();
}
//@}

} // namespace A1;

} // namespace NPL;

#endif

