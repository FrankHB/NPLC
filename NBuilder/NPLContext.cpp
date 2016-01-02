/*
	© 2012-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.cpp
\ingroup Adaptor
\brief NPL 上下文。
\version r1639
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-01-03 02:35 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#include "NPLContext.h"
#include YFM_NPL_SContext
#include <ystdex/container.hpp>
#include <iostream>
#include YFM_NPL_NPLA1
#include <ystdex/cast.hpp> // for ystdex::pvoid;
#include <ystdex/scope_guard.hpp> // for ystdex::make_guard;

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


void
CleanupEmptyTerms(SemaNode::Container& cont) ynothrow
{
	ystdex::erase_all_if(cont, cont.begin(), cont.end(),
		[](const SemaNode& term) ynothrow{
			return !term;
		});
}

void
RegisterForm(const ContextNode& node, const string& name, FormHandler f)
{
	RegisterContextHandler(node, name,
		[f](const SemaNode& term, const ContextNode&){
		auto& c(term.GetContainerRef());
		const auto s(c.size());

		// TODO: Use more specific exceptions.
		if(s > 1)
			f(std::next(c.begin()), s - 1, term.Value);
		else if(s != 0)
			throw std::invalid_argument("No sufficient arguments.");
		else
			throw std::invalid_argument("Invalid term found.");
	});
}


#define NPL_TRACE 1

void
NPLContext::Reduce(const SemaNode& sema, const ContextNode& ctx,
	Continuation k)
{
	using ystdex::pvoid;
	auto& depth(AccessChild<size_t>(ctx, "__depth"));

	YTraceDe(Informative, "Depth = %zu, context = %p, semantics = %p.", depth,
		pvoid(&ctx), pvoid(&sema));
	++depth;

	const auto gd(ystdex::make_guard([&]() ynothrow{
		--depth;
	}));

	if(!sema.empty())
	{
		auto& cont(sema.GetContainerRef());
		auto n(cont.size());

		if(n == 0)
			// NOTE: Empty list.
			sema.Value = ValueToken::Null;
		else if(n == 1)
		{
			// NOTE: List with single element shall be reduced to its value.
			Deref(cont.begin()).SwapContent(sema);
			Reduce(sema, ctx, k);
		}
		else
		{
			// NOTE: List application: call by value.
			ReduceTerms_CallByValue(sema, ctx, k);
			n = cont.size();
			if(n > 2)
			{
				try
				{
					// NOTE: Matching non-empty syntactic forms like function
					//	calls.
					// TODO: Matching special forms?
					auto i(cont.begin());
					const auto& fn(Deref(i));

					if(const auto p_handler = AccessPtr<ContextHanlder>(fn))
					{
						++i;
						(*p_handler)(sema, ctx);
						k(sema);
					}
					else
						YTraceDe(Warning, "No matching form found.");
				}
				CatchThrow(ystdex::bad_any_cast& e, LoggedEvent(ystdex::sfmt(
					"Mismatched types ('%s', '%s') found.", e.from(), e.to()),
					Warning))
				return;
			}
			if(n == 0)
				YTraceDe(Warning, "Empty reduced form found.");
			else
				YTraceDe(Warning, "%zu term(s) not reduced found.", n);
		//	k(sema);
			return;
		}
	}
	else if(AccessPtr<ValueToken>(sema))
		;
	else if(auto p = AccessPtr<string>(sema))
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
		k(sema);
	}
}

void
NPLContext::ReduceTerms_CallByValue(const SemaNode& sema,
	const ContextNode& ctx, Continuation k)
{
	auto& cont(sema.GetContainerRef());

	// FIXME: Context.
	for(auto& term : cont)
		Reduce(term, ctx, k);
	CleanupEmptyTerms(cont);
}

TokenList&
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	auto sema(SContext::Analyze(Session(unit)));

	Root["__depth"].Value = size_t();
	Reduce(sema, Root, [](const SemaNode&){});
	// TODO: Merge result to 'Root'.

	using namespace std;

	PrintNode(cout, sema, LiteralizeEscapeNodeLiteral);
	cout << endl;

	return token_list;
}

} // namespace NPL;

