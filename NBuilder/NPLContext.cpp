/*
	© 2012-2014 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.cpp
\ingroup Adaptor
\brief NPL 上下文。
\version r1493
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2014-04-25 18:57 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#include "NPLContext.h"
#include YFM_NPL_SContext
#include <ystdex/container.hpp>
#include <iostream>

namespace NPL
{

NPLContext::NPLContext(const FunctionMap& m)
	: Root(), Map(m), token_list(), sem()
{}

void
NPLContext::Eval(const string& arg)
{
	if(CheckLiteral(arg) == '\'')
		NPLContext(Map).Perform(ystdex::get_mid(arg));
}

ValueObject
NPLContext::FetchValue(const ValueNode& ctx, const string& name)
{
	if(auto p_id = LookupName(ctx, name))
		return p_id->Value;
	return ValueObject();
}

const ValueNode*
NPLContext::LookupName(const ValueNode& ctx, const string& id)
{
	TryRet(&ctx.at(id))
	CatchIgnore(std::out_of_range&)
	CatchIgnore(ystdex::bad_any_cast&)
	return {};
}

void
NPLContext::HandleIntrinsic(const string& cmd)
{
	if(cmd == "exit")
		throw SSignal(SSignal::Exit);
	if(cmd == "cls")
		throw SSignal(SSignal::ClearScreen);
	if(cmd == "about")
		throw SSignal(SSignal::About);
	if(cmd == "help")
		throw SSignal(SSignal::Help);
	if(cmd == "license")
		throw SSignal(SSignal::License);
}

#define NPL_TRACE 1

void
NPLContext::Reduce(size_t depth, const SemaNode& sema, const ContextNode& ctx,
	Continuation cont)
{
	std::clog << "Depth = " << depth << ", Context = " << &ctx
		<< ", Semantics = " << &sema << std::endl;
	++depth;
	if(auto p = sema.GetContainerPtr())
	{
		const auto n(p->size());

		if(n == 0)
			// NOTE: Empty list.
			sema.Value = ValueToken::Null;
		else if(n == 1)
		{
			// NOTE: List with single element shall be reduced to its value.
			sema.Value = std::move(p->begin()->Value);
			sema.ClearContainer();
			Reduce(depth, sema, ctx, cont);
		}
		else
		{
			// NOTE: List application: call by value.
			// FIXME: Context.
			for(auto& term : *p)
//PackNodes(sema.GetName(), ValueNode(0, "$parent", &ctx)
				Reduce(depth, term, ctx, cont);
			ystdex::erase_all_if(*p, p->begin(), p->end(),
				[](const SemaNode& term){
					return !term;
			});
			// NOTE: Match function calls.
			try
			{
				Deref(p).begin()->Value.Access<FunctionHandler>()(sema, ctx);
				cont(sema);
			}
			CatchExpr(std::out_of_range&, YTraceDe(Warning,
				"No matching functions found."))
			CatchThrow(ystdex::bad_any_cast& e,
				LoggedEvent(ystdex::sfmt("Mismatched types <'%s', '%s'> found.",
				e.from(), e.to()), Warning))
			return;
		}
	}
	else if(sema.Value.AccessPtr<ValueToken>())
		;
	else if(auto p = sema.Value.AccessPtr<string>())
	{
		auto& id(*p);

		if(id == "," || id == ";")
			sema.Clear();
		else
		{
			HandleIntrinsic(id);
			// NOTE: Value rewriting.
			if(auto v = FetchValue(ctx, id))
				sema.Value = std::move(v);
		//	else
		//		throw LoggedEvent(ystdex::sfmt(
		//			"Wrong identifier '%s' found", id.c_str()), Warning);
		}
		cont(sema);
	}
}

namespace
{

inline std::ostream&
WritePrefix(std::ostream& f, size_t n = 1, char c = '\t')
{
	while(n--)
		f << c;
	return f;
}

bool
PrintNodeString(std::ostream& f, const ValueNode& node)
{
	try
	{
		const auto& s(Access<string>(node));

		f << '"' << EscapeLiteral(s) << '"' << '\n';
		return true;
	}
	CatchIgnore(ystdex::bad_any_cast&)
	return {};
}

std::ostream&
WriteNodeC(std::ostream& f, const ValueNode& node, size_t depth)
{
	WritePrefix(f, depth);
	f << EscapeLiteral(node.GetName());
	if(node)
	{
		f << ' ';
		if(PrintNodeString(f, node))
			return f;
		f << '\n';
		try
		{
			for(const auto& n : node)
			{
				WritePrefix(f, depth);
				if(IsPrefixedIndex(n.GetName()))
					PrintNodeString(f, n);
				else
				{
					f << '(' << '\n';
					TryExpr(WriteNodeC(f, n, depth + 1))
					CatchIgnore(std::out_of_range&)
					WritePrefix(f, depth);
					f << ')' << '\n';
				}
			}
		}
		CatchExpr(ystdex::bad_any_cast&,
			f << '[' << node.Value.GetType().name() << ']')
	}
	return f;
}

} // unnamed namespace;

TokenList&
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	auto sema(SContext::Analyze(Session(unit)));

	Reduce(0, sema, Root, [](const SemaNode&){});
	// TODO: Merge result to 'Root'.

	using namespace std;

	WriteNodeC(cout, sema, 0);
	cout << endl;

	return token_list;
}

} // namespace NPL;

