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
\version r2114
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-01-17 20:42 +0800
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
#include <ystdex/functional.hpp> // for ystdex::retry_on_cond;

#define NPL_TracePerform 1

namespace NPL
{

void
ContextHandler::operator()(const TermNode& term, const ContextNode& ctx) const
{
	if(Special)
	{
		// TODO: Matching specific special forms?
		YTraceDe(Debug, "Found special form.");
		DoHandle(term, ctx);
	}
	else
	{
		auto& con(term.GetContainerRef());

		if(con.size() < 2)
			throw LoggedEvent("Invalid term to handle found.", Err);
		// NOTE: Arguments evaluation: call by value.
		// FIXME: Context.
		std::for_each(std::next(con.cbegin()), con.cend(),
			[&](decltype(*con.cend())& sub_term){
			NPLContext::Reduce(sub_term, ctx);
		});

		const auto n(con.size());

		if(n > 1)
		{
			// NOTE: Matching function calls.
			auto i(con.cbegin());

			// NOTE: Adjust null list argument application
			//	to function call without arguments.
			// TODO: Improve performance of comparison?
			if(n == 2 && Deref(++i).Value == ValueToken::Null)
				con.erase(i);
			DoHandle(term, ctx);
		}
		// TODO: Unreduced form check.
#if 0
		if(n == 0)
			YTraceDe(Warning, "Empty reduced form found.");
		else
			YTraceDe(Warning, "%zu term(s) not reduced found.", n);
#endif
	}
}

void
ContextHandler::DoHandle(const TermNode& term, const ContextNode& ctx) const
{
	try
	{
		if(!term.empty())
			handler(term, ctx);
		else
			// TODO: Use more specific exceptions.
			throw std::invalid_argument("Empty term found.");
	}
	CatchThrow(ystdex::bad_any_cast& e, LoggedEvent(
		ystdex::sfmt("Mismatched types ('%s', '%s') found.",
		e.from(), e.to()), Warning))
	// TODO: Use nest exceptions?
	CatchThrow(std::exception& e, LoggedEvent(e.what(), Err))
}


void
CleanupEmptyTerms(TermNode::Container& con) ynothrow
{
	ystdex::erase_all_if(con, [](const TermNode& term) ynothrow{
		return !term;
	});
}

void
RegisterForm(const ContextNode& node, const string& name, FormHandler f,
	bool special)
{
	RegisterContextHandler(node, name,
		ContextHandler([f](const TermNode& term, const ContextNode&){
		auto& con(term.GetContainerRef());
		const auto size(con.size());

		YAssert(size != 0, "Invalid term found.");
		f(con.begin(), size - 1, term);
	}, special));
}


#define NPL_TRACE 1

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

bool
NPLContext::Reduce(const TermNode& term, const ContextNode& ctx)
{
	using ystdex::pvoid;
#if NPL_TraceDepth
	auto& depth(AccessChild<size_t>(ctx, "__depth"));

	YTraceDe(Informative, "Depth = %zu, context = %p, semantics = %p.", depth,
		pvoid(&ctx), pvoid(&term));
	++depth;

	const auto gd(ystdex::make_guard([&]() ynothrow{
		--depth;
	}));
#endif
	auto& con(term.GetContainerRef());

	// NOTE: Rewriting loop until the normal form is got.
	return ystdex::retry_on_cond([&](bool reducible) ynothrow{
		// TODO: Simplify.
		// XXX: Continuations?
	//	if(reducible)
		//	k(term);
		CleanupEmptyTerms(con);
		// NOTE: Stop on got a normal form.
		return reducible && !con.empty();
	}, [&]() -> bool{
		if(!con.empty())
		{
			auto n(con.size());

			YAssert(n != 0, "Invalid node found.");
			if(n == 1)
			{
				// NOTE: List with single element shall be reduced to its value.
				Deref(con.begin()).SwapContent(term);
				return Reduce(term, ctx);
			}
			else
			{
				// NOTE: List evaluation: call by value.
				// TODO: Form evaluation: macro expansion, etc.
				if(Reduce(*con.cbegin(), ctx))
					return true;
				if(con.empty())
					return {};

				const auto& fm(Deref(con.cbegin()));

				if(const auto p_handler = AccessPtr<ContextHandler>(fm))
					(*p_handler)(term, ctx);
				else
				{
					const auto p(AccessPtr<string>(fm));

					// TODO: Capture contextual information in error.
					throw LoggedEvent(ystdex::sfmt("No matching form '%s'"
						" with %zu argument(s) found.", p ? p->c_str()
						: "#<unknown>", n), Err);
				}
				return {};
			}
		}
		else if(!term.Value)
			// NOTE: Empty list.
			term.Value = ValueToken::Null;
		else if(AccessPtr<ValueToken>(term))
			// TODO: Handle special value token.
			;
		else if(const auto p = AccessPtr<string>(term))
		{
			// NOTE: String node of identifier.
			auto& id(*p);

			// FIXME: Correct handling of separators.
			if(id == "," || id == ";")
				term.Clear();
			else if(!id.empty())
			{
				// NOTE: Value rewriting.
				// TODO: Implement general literal check.
				if(CheckLiteral(id) == char() && !std::isdigit(id.front()))
				{
					if(auto v = FetchValue(ctx, id))
					{
						term.Value = std::move(v);
						if(const auto p_handler
							= AccessPtr<LiteralHandler>(term))
							return (*p_handler)(ctx);
					}
					else
						throw LoggedEvent(ystdex::sfmt("Undeclared identifier"
							" '%s' found", id.c_str()), Err);
				}
			}
			// XXX: Empty token is ignored.
			// XXX: Remained reducible?
			return {};
		}
		// NOTE: Exited loop has produced normal form by default.
		return {};
	});
}

TokenList&
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	auto term(SContext::Analyze(Session(unit)));

#if NPL_TraceDepth
	Root["__depth"].Value = size_t();
#endif
	Reduce(term, Root);
#if NPL_TracePerform

	// TODO: Merge result to 'Root'.
	std::ostringstream oss;

	PrintNode(oss, term, LiteralizeEscapeNodeLiteral);
	YTraceDe(Debug, "%s", oss.str().c_str());
#endif
	return token_list;
}

} // namespace NPL;

