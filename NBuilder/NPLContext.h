﻿/*
	© 2012-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.h
\ingroup Adaptor
\brief NPL 上下文。
\version r1254
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2016-01-22 14:18 +0800
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

/*!
\brief 值记号：节点中的值的占位符。
\since YSLib build 403
*/
enum class ValueToken
{
	Null,
	//! \since YSLib build 664
	GroupingAnchor,
	//! \since YSLib build 664
	OrderedAnchor
};


/// 592
//@{
using TermNode = ValueNode;
using ContextNode = ValueNode;
using ContinuationWrapper = ystdex::any;
/// 663
//@{
struct ContextHandler
{
private:
	std::function<void(const TermNode&, const ContextNode&)> handler;

public:
	bool Special = {};

	template<typename _func>
	ContextHandler(_func f, bool special = {})
		: handler(f), Special(special)
	{}

	void
	operator()(const TermNode&, const ContextNode&) const;

private:
	void
	DoHandle(const TermNode&, const ContextNode&) const;
};

using FormHandler = std::function<void(TermNode::Container::iterator, size_t,
	const TermNode&)>;
/// 664
using LiteralHandler = std::function<bool(const ContextNode&)>;
//@{
using XFunction = std::function<void(const TermNode&, const ContextNode&,
	ContinuationWrapper)>;
//@}


/// 663
//@{
void
CleanupEmptyTerms(TermNode::Container&) ynothrow;

inline PDefH(void, RegisterContextHandler, const ContextNode& node,
	const string& name, ContextHandler f)
	ImplExpr(node[name].Value = f)

void
RegisterForm(const ContextNode&, const string&, FormHandler,
	bool = {});
//@}

/// 664
inline PDefH(void, RegisterLiteralHandler, const ContextNode& node,
	const string& name, LiteralHandler f)
	ImplExpr(node[name].Value = f)


/// 306
struct NPLContext : private noncopyable
{
public:
	/// 664
	ContextNode Root;

	NPLContext() = default;

	/// 592
	static ValueObject
	FetchValue(const ValueNode&, const string&);

	/// 495
	static const ValueNode*
	LookupName(const ValueNode&, const string&);

	/*!
	\note 可能使参数中容器的迭代器失效。
	\since YSLib build 663
	*/
	static bool
	Reduce(const TermNode&, const ContextNode&);

	/*!
	\brief 对容器中的第二项开始逐项规约。
	\throw LoggedEvent 容器内的子表达式不大于一项。
	\note 语言规范指定规约顺序不确定。
	\note 可能使参数中容器的迭代器失效。
	\since YSLib build 665
	\sa Reduce
	*/
	static void
	ReduceArguments(TermNode::Container&, const ContextNode&);

	/// 665
	ValueNode
	Perform(const string&);
};

} // namespace NPL;

#endif

