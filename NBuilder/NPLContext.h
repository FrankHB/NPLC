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
\version r1401
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2016-03-06 02:42 +0800
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

/// 675
//@{
struct PassesCombiner
{
	template<typename _tIn>
	bool
	operator()(_tIn first, _tIn last) const
	{
		return ystdex::fast_any_of(first, last, ystdex::indirect<>());
	}
};


template<typename... _tParams>
using GPasses = YSLib::GEvent<bool(_tParams...),
	YSLib::GCombinerInvoker<bool, PassesCombiner>>;

using TermPasses = GPasses<TermNode&>;

using EvaluationPasses = GPasses<TermNode&, ContextNode&>;
//@}


/// 676
bool
DetectReducible(TermNode&, bool);


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

	NPLContext() = default;

	/*!
	\note 可能使参数中容器的迭代器失效。
	\since YSLib build 674
	*/
	static bool
	Reduce(TermNode&, ContextNode&);

	/*!
	\brief 对容器中的第二项开始逐项规约。
	\throw LoggedEvent 容器内的子表达式不大于一项。
	\note 语言规范指定规约顺序不确定。
	\note 可能使参数中容器的迭代器失效。
	\since YSLib build 674
	\sa Reduce
	*/
	static void
	ReduceArguments(TermNode::Container&, ContextNode&);

	/// 665
	TermNode
	Perform(const string&);
};

} // namespace A1;

} // namespace NPL;

#endif

