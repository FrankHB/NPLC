/*
	© 2012-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.h
\ingroup NPL
\brief NPL 上下文。
\version r1442
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2016-04-15 20:05 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#ifndef INC_NPL_NPLContext_h_
#define INC_NPL_NPLContext_h_

#include <NPL/YModules.h>
#include YFM_NPL_SContext
#include YFM_NPL_NPLA1

namespace NPL
{

namespace A1
{

/// 675
using ystdex::optional;

/// 674
//@{
struct FunctionContextHandler
{
public:
	FormContextHandler Handler;

	template<typename _func>
	FunctionContextHandler(_func f)
		: Handler(f)
	{}

	void
	operator()(TermNode&, ContextNode&) const;
};


struct FunctionFormHandler : private FunctionContextHandler
{
public:
	template<typename _func>
	FunctionFormHandler(_func f)
		: FunctionContextHandler(Wrap(f))
	{}

	using FunctionContextHandler::operator();

private:
	static FunctionContextHandler
	Wrap(std::function<void(TNIter, size_t, TermNode&)>);
};


template<typename _func>
void
RegisterFunction(ContextNode& node, const string& name, _func f)
{
	RegisterContextHandler(node, name, FunctionFormHandler(f));
}
//@}


/// 306
struct NPLContext
{
public:
	/// 664
	ContextNode Root;
	/// 675
	TermPasses Preprocess;
	/// 675
	EvaluationPasses ListTermPreprocess;

	NPLContext();

	/// 665
	TermNode
	Perform(const string&);

private:
	/// 675
	static ContextNode
	SetupRoot(EvaluationPasses);
};

} // namespace A1;

} // namespace NPL;

#endif

