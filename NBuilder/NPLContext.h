/*
	© 2012-2014 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.h
\ingroup Adaptor
\brief NPL 上下文。
\version r1159
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2013-12-27 10:31 +0800
	2014-04-25 18:56 +0800
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
using FunctionHandler
	= std::function<void(const SemaNode&, const ContextNode&)>;
using XFunction = std::function<void(const SemaNode&, const ContextNode&,
	ContinuationWrapper)>;
using Continuation = std::function<void(const SemaNode&)>;


template<typename _func>
inline void
SetContextHandler(const ContextNode& node, const string& name, _func f)
{
	node[name].Value = FunctionHandler(f);
}

template<typename _func>
inline void
SetContextHandler_A(const ContextNode& node, const string& name, _func f)
{
	SetContextHandler(node, name, [f](const SemaNode& term, const ContextNode&){
		auto& c(term.GetContainerRef());
		const auto n(c.size());
		auto i(c.begin());

		if(n != 0 && ++i != c.end())
			f(i, c.size() - 1, term.Value);
		else
			throw LoggedEvent("No sufficient arguments.", Alert);
	});
}
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
	// TODO: use TLCIter instead of TLIter for C++11 comforming implementations.
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
	/// 592
	static void
	Reduce(size_t, const SemaNode&, const ContextNode&, Continuation);

	TokenList&
	Perform(const string& unit);
};

} // namespace NPL;

#endif

