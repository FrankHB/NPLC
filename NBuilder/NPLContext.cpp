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
\version r1785
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-01-08 22:04 +0800
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
ContextHandler::operator()(const SemaNode& sema, const ContextNode& ctx) const
{
	if(!sema.empty())
		handler(sema, ctx);
	else
		// TODO: Use more specific exceptions.
		throw std::invalid_argument("Empty term found.");
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
RegisterForm(const ContextNode& node, const string& name, FormHandler f,
	bool special)
{
	RegisterContextHandler(node, name,
		ContextHandler([f](const SemaNode& term, const ContextNode&){
		auto& c(term.GetContainerRef());
		const auto s(c.size());

		YAssert(s != 0, "Invalid term found.");
		f(c.begin(), s - 1, term);
	}, special));
}


#define NPL_TRACE 1

bool
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

		YAssert(n != 0, "Invalid node found.");
		if(n == 1)
		{
			// NOTE: List with single element shall be reduced to its value.
			Deref(cont.begin()).SwapContent(sema);
			return Reduce(sema, ctx, k);
		}
		else
		{
			ystdex::retry_on_cond([&](bool reducible) ynothrow{
				// TODO: Simplify.
				if(reducible)
					k(sema);
				CleanupEmptyTerms(cont);

				// NOTE: Stop on got a normal form.
				return reducible && !cont.empty();
			}, [&]() -> bool{
				// NOTE: List evaluation: call by value.
				// TODO: Form evaluation: macro expansion, etc.
				Reduce(*cont.cbegin(), ctx, k);
				CleanupEmptyTerms(cont);
				if(cont.empty())
					return {};

				auto i(cont.cbegin());

				const auto& fm(Deref(i));

				if(const auto p_handler = AccessPtr<ContextHandler>(fm))
				{
					if(p_handler->Special)
					{
						// TODO: Matching specific special forms?
						YTraceDe(Debug, "Found special form.");
						try
						{
							(*p_handler)(sema, ctx);
							k(sema);
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
						std::for_each(std::next(i), cont.cend(),
							[&](decltype(*i)& term){
							Reduce(term, ctx, k);
						});
						n = cont.size();
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
									cont.erase(i);
								(*p_handler)(sema, ctx);
								k(sema);
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
			});
			return {};
		}
	}
	else if(!sema.Value)
		// NOTE: Empty list.
		sema.Value = ValueToken::Null;
	else if(AccessPtr<ValueToken>(sema))
		;
	else if(auto p = AccessPtr<string>(sema))
	{
		auto& id(*p);

		if(id == "," || id == ";")
			sema.Clear();
		else if(!id.empty())
		{
			HandleIntrinsic(id);
			// NOTE: Value rewriting.
			// TODO: Implement general literal check.
			if(!std::isdigit(id.front()))
			{
				if(auto v = FetchValue(ctx, id))
					sema.Value = std::move(v);
				else
					throw LoggedEvent(ystdex::sfmt(
						"Undeclared identifier '%s' found", id.c_str()), Err);
			}
		}
		k(sema);
		// XXX: Remained reducible?
		return true;
	}
	return {};
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

