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
\version r1910
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-01-10 19:41 +0800
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
ContextHandler::operator()(const TermNode& term, const ContextNode& ctx) const
{
	if(!term.empty())
		handler(term, ctx);
	else
		// TODO: Use more specific exceptions.
		throw std::invalid_argument("Empty term found.");
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

bool
NPLContext::Reduce(const TermNode& term, const ContextNode& ctx,
	Continuation k)
{
	using ystdex::pvoid;
	auto& depth(AccessChild<size_t>(ctx, "__depth"));

	YTraceDe(Informative, "Depth = %zu, context = %p, semantics = %p.", depth,
		pvoid(&ctx), pvoid(&term));
	++depth;

	const auto gd(ystdex::make_guard([&]() ynothrow{
		--depth;
	}));
	auto& con(term.GetContainerRef());

	// NOTE: Rewriting loop until the normal form is got.
	return ystdex::retry_on_cond([&](bool reducible) ynothrow{
		// TODO: Simplify.
		if(reducible)
			k(term);
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
				return Reduce(term, ctx, k);
			}
			else
			{
				// NOTE: List evaluation: call by value.
				// TODO: Form evaluation: macro expansion, etc.
				if(Reduce(*con.cbegin(), ctx, k))
					return true;
				if(con.empty())
					return {};

				auto i(con.cbegin());
				const auto& fm(Deref(i));

				if(const auto p_handler = AccessPtr<ContextHandler>(fm))
				{
					if(p_handler->Special)
					{
						// TODO: Matching specific special forms?
						YTraceDe(Debug, "Found special form.");
						try
						{
							(*p_handler)(term, ctx);
							k(term);
						}
						CatchThrow(ystdex::bad_any_cast& e, LoggedEvent(
							ystdex::sfmt("Mismatched types ('%s', '%s') found.",
							e.from(), e.to()), Warning))
						// FIXME: Recoginze normal forms.
						return {};
					}
					else
					{
						// NOTE: Arguments evaluation: call by value.
						// FIXME: Context.
						std::for_each(std::next(i), con.cend(),
							[&](decltype(*i)& sub_term){
							Reduce(sub_term, ctx, k);
						});
						n = con.size();
						if(n > 1)
						{
							// NOTE: Matching function calls.
							try
							{
								// NOTE: Adjust null list argument application
								//	to function call without arguments.
								// TODO: Improve performance of comparison?
								if(n == 2
									&& Deref(++i).Value == ValueToken::Null)
									con.erase(i);
								(*p_handler)(term, ctx);
								k(term);
							}
							CatchThrow(ystdex::bad_any_cast& e, LoggedEvent(
								ystdex::sfmt("Mismatched types ('%s', '%s')"
								" found.", e.from(), e.to()), Warning))
						}
					}
				}
				else
				{
					const auto p(AccessPtr<string>(fm));

					// TODO: Capture contextual information in error.
					throw LoggedEvent(ystdex::sfmt("No matching form '%s'"
						" with %zu argument(s) found.", p ? p->c_str()
						: "#<unknown>", n), Err);
				}
				if(n == 0)
					YTraceDe(Warning, "Empty reduced form found.");
				else
					YTraceDe(Warning, "%zu term(s) not reduced found.", n);
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
				// TODO: Correct intrinsic handled only as a single term form.
				HandleIntrinsic(id);
				// NOTE: Value rewriting.
				// TODO: Implement general literal check.
				if(!std::isdigit(id.front()))
				{
					if(auto v = FetchValue(ctx, id))
						term.Value = std::move(v);
					else
						throw LoggedEvent(ystdex::sfmt("Undeclared identifier"
							" '%s' found", id.c_str()), Err);
				}
			}
			// XXX: Empty token is ignored.
			// XXX: Continuations?
			k(term);
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

	Root["__depth"].Value = size_t();
	Reduce(term, Root, [](const TermNode&){});
	// TODO: Merge result to 'Root'.

	using namespace std;

	PrintNode(cout, term, LiteralizeEscapeNodeLiteral);
	cout << endl;

	return token_list;
}

} // namespace NPL;

