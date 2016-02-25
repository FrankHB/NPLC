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
\version r1367
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2016-02-25 11:20 +0800
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

/// 674
using YSLib::ValueObject;
/// 674
using YSLib::observer_ptr;

namespace A1
{

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

/// 674
//@{
template<typename... _tParams>
using GPass = YSLib::GEvent<void(_tParams...)>;

using TermPass = GPass<TermNode&>;

using EvaluationPass = GPass<TermNode&, ContextNode&>;
//@}


/// 306
struct NPLContext
{
public:
	/// 664
	ContextNode Root;
	/// 673
	TermPass Preprocess;
	/// 673
	EvaluationPass ListTermPreprocess;

	NPLContext() = default;

	/// 674
	static ValueObject
	FetchValue(const ContextNode&, const string&);

	/// 674
	static observer_ptr<const ValueNode>
	LookupName(const ContextNode&, const string&);

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

