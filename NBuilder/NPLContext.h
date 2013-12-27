/*
	© 2012-2013 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.h
\ingroup Adaptor
\brief NPL 上下文。
\version r1139
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-08-03 19:55:41 +0800
\par 修改时间:
	2013-12-27 10:31 +0800
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


/// 306
struct NPLContext : private noncopyable
{
public:
	/// 403
	typedef std::function<void(const string&)> Function;
	typedef map<string, Function> FunctionMap;

	ValueNode Root;
	/// 403
	FunctionMap Map;

private:
	TokenList token_list;
	// TODO: use TLCIter instead of TLIter for C++11 comforming implementations;

public:
	/// 328
	string sem;

	NPLContext() = default;

private:
	///329
	NPLContext(const FunctionMap&);
#if 0
	NPLContext(const NPLContext&, TokenList);
#endif

	/// 327
	TLIter
	Call(TLIter b, TLIter e, size_t& cur_off);

	/// 328
	void
	CallS(const string& fn, const string& arg, size_t& cur_off);

public:
	void
	Eval(const string& arg);

private:
	static void
	HandleIntrinsic(const string& cmd);

	/// 327
	/// \pre b and e shall be iterator in or one-past-end of token_list.
	/// \pre b shall be dereferanceable when e is dereferanceable.
	pair<TLIter, size_t>
	Reduce(size_t depth, TLIter b, TLIter e, bool eval = true);

public:
	TokenList&
	Perform(const string& unit);
};

} // namespace NPL;

#endif

