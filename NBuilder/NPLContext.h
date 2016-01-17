/*
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
\version r1221
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2016-01-17 20:55 +0800
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
	/// 403
	using Function = std::function<void(const string&)>;
	using FunctionMap = map<string, Function>;

	/// 664
	ContextNode Root;
	/// 403
	FunctionMap Map;

private:
	TokenList token_list;

public:
	/// 328
	string sem;

	NPLContext() = default;

private:
	///329
	NPLContext(const FunctionMap&);

public:
	void
	Eval(const string& arg);

	/// 592
	static ValueObject
	FetchValue(const ValueNode&, const string&);

	/// 495
	static const ValueNode*
	LookupName(const ValueNode&, const string&);

	/// 663
	//! \note 可能使参数中容器的迭代器失效。
	static bool
	Reduce(const TermNode&, const ContextNode&);

	TokenList&
	Perform(const string& unit);
};

} // namespace NPL;

#endif

