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
\version r1293
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2016-02-01 13:50 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#ifndef INC_NPL_NPLContext_h_
#define INC_NPL_NPLContext_h_

#include <NPL/YModules.h>
#include YFM_NPL_SContext

namespace NPL
{

using namespace YSLib;


/// 592
//@{
using TermNode = ValueNode;
using ContextNode = ValueNode;
/// 663
//@{
struct ContextHandler
{
private:
	/// 667
	std::function<void(TermNode&, ContextNode&)> handler;

public:
	bool Special = {};

	template<typename _func>
	ContextHandler(_func f, bool special = {})
		: handler(f), Special(special)
	{}

	/// 667
	void
	operator()(TermNode&, ContextNode&) const;

private:
	/// 667
	void
	DoHandle(TermNode&, ContextNode&) const;
};

/// 667
using FormHandler = std::function<void(TermNode::Container::iterator, size_t,
	TermNode&)>;
/// 664
using LiteralHandler = std::function<bool(const ContextNode&)>;


/// 663
//@{
void
CleanupEmptyTerms(TermNode::Container&) ynothrow;

/// 667
inline PDefH(void, RegisterContextHandler, ContextNode& node,
	const string& name, ContextHandler f)
	ImplExpr(node[name].Value = f)

void
RegisterForm(ContextNode&, const string&, FormHandler, bool = {});
//@}

/// 667
inline PDefH(void, RegisterLiteralHandler, ContextNode& node,
	const string& name, LiteralHandler f)
	ImplExpr(node[name].Value = f)


/// 306
struct NPLContext
{
public:
	/// 664
	ContextNode Root;
	/// 667
	std::function<void(TermNode&)> Preprocess;

	NPLContext() = default;

	/// 592
	static ValueObject
	FetchValue(const ValueNode&, const string&);

	/// 495
	static const ValueNode*
	LookupName(const ValueNode&, const string&);

	/*!
	\note 可能使参数中容器的迭代器失效。
	\since YSLib build 667
	*/
	static bool
	Reduce(TermNode&, ContextNode&);

	/*!
	\brief 对容器中的第二项开始逐项规约。
	\throw LoggedEvent 容器内的子表达式不大于一项。
	\note 语言规范指定规约顺序不确定。
	\note 可能使参数中容器的迭代器失效。
	\since YSLib build 665
	\sa Reduce
	*/
	static void
	ReduceArguments(TermNode::Container&, ContextNode&);

	/// 665
	ValueNode
	Perform(const string&);
};

} // namespace NPL;

#endif

