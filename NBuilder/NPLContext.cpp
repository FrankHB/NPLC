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
\version r1599
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-02-22 15:53 +0800
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

namespace NPL
{

using A1::ValueToken;

void
ContextHandler::operator()(TermNode& term, ContextNode& ctx) const
{
	if(Special)
	{
		// TODO: Is it worth matching specific builtin special forms here?
		YTraceDe(Debug, "Found special form.");
		DoHandle(term, ctx);
	}
	else
	{
		auto& con(term.GetContainerRef());

		// NOTE: Arguments evaluation: applicative order.
		NPLContext::ReduceArguments(con, ctx);

		const auto n(con.size());

		if(n > 1)
		{
			// NOTE: Matching function calls.
			auto i(con.begin());

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
ContextHandler::DoHandle(TermNode& term, ContextNode& ctx) const
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
RegisterForm(ContextNode& node, const string& name, FormHandler f, bool special)
{
	RegisterContextHandler(node, name,
		ContextHandler([f](TermNode& term, const ContextNode&){
		auto& con(term.GetContainerRef());
		const auto size(con.size());

		YAssert(size != 0, "Invalid term found.");
		// NOTE: This is basically call-by-value, i.e. the arguments would be
		//	copied as the parameters, unless the terms are prevented to be
		//	evaluated immediately by some special tags in reduction.
		f(con.begin(), size - 1, term);
	}, special));
}


ValueObject
NPLContext::FetchValue(const ValueNode& ctx, const string& name)
{
	return ystdex::call_value_or<ValueObject>(
		std::mem_fn(&ValueNode::Value),
		LookupName(ctx, name));
}

observer_ptr<const ValueNode>
NPLContext::LookupName(const ValueNode& ctx, const string& id)
{
	return AccessNodePtr(ctx, id);
}

bool
NPLContext::Reduce(TermNode& term, ContextNode& ctx)
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
				TermNode(std::move(Deref(con.begin()))).SwapContent(term);
				return Reduce(term, ctx);
			}
			else
			{
				AccessChild<EvaluationPass>(ctx, "__$$")(term, ctx);
				// NOTE: List evaluation: call by value.
				// TODO: Form evaluation: macro expansion, etc.
				if(Reduce(*con.begin(), ctx))
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

			// NOTE: If necessary, they should have been handled in preprocess
			//	pass.
			if(id == "," || id == ";")
			{
				term.Clear();
				return true;
			}
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

void
NPLContext::ReduceArguments(TermNode::Container& con, ContextNode& ctx)
{
	if(con.size() > 1)
		// NOTE: The order of evaluation is unspecified by the language
		//	specification. However here it can only be either left-to-right
		//	or right-to-left unless the separators has been predicted.
		std::for_each(std::next(con.begin()), con.end(),
			[&](decltype(*con.end())& sub_term){
			NPLContext::Reduce(sub_term, ctx);
		});
	else
		throw LoggedEvent("Invalid term to handle found.", Err);
}

ValueNode
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	auto term(SContext::Analyze(Session(unit)));

#if NPL_TraceDepth
	Root["__depth"].Value = size_t();
#endif
	Root["__$$"].Value = ListTermPreprocess;
	Preprocess(term);
	Reduce(term, Root);
	// TODO: Merge result to 'Root'?
	return term;
}

} // namespace NPL;

