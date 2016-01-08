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
\version r1204
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2016-01-08 21:42 +0800
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

/// 304
enum class SSignal
{
	Exit,
	ClearScreen,
	About,
	Help,
	License
};


/*!
\brief 值记号：节点中的值的占位符。
\since YSLib build 403
*/
enum class ValueToken
{
	Null
};


/// 592
//@{
using SemaNode = ValueNode;
using ContextNode = ValueNode;
using ContinuationWrapper = ystdex::any;
/// 663
//@{
struct ContextHandler
{
private:
	std::function<void(const SemaNode&, const ContextNode&)> handler;

public:
	bool Special = {};

	template<typename _func>
	ContextHandler(_func f, bool special = {})
		: handler(f), Special(special)
	{}

	void
	operator()(const SemaNode&, const ContextNode&) const;
};

using FormHandler = std::function<void(SemaNode::Container::iterator, size_t,
	ValueObject&)>;
//@{
using XFunction = std::function<void(const SemaNode&, const ContextNode&,
	ContinuationWrapper)>;
using Continuation = std::function<void(const SemaNode&)>;
//@}


/// 663
//@{
void
CleanupEmptyTerms(SemaNode::Container&) ynothrow;

inline PDefH(void, RegisterContextHandler, const ContextNode& node,
	const string& name, ContextHandler f)
	ImplExpr(node[name].Value = f)

void
RegisterForm(const ContextNode&, const string&, FormHandler,
	bool = {});
//@}


/// 306
struct NPLContext : private noncopyable
{
public:
	/// 403
	using Function = std::function<void(const string&)>;
	using FunctionMap = map<string, Function>;

	ValueNode Root;
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

private:
	static void
	HandleIntrinsic(const string& cmd);

public:
	/// 663
	static bool
	Reduce(const SemaNode&, const ContextNode&, Continuation);

	TokenList&
	Perform(const string& unit);
};

} // namespace NPL;

#endif

