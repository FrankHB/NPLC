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
\version r2006
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-04-27 15:56 +0800
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

/// 674
using namespace YSLib;
/// 676
using namespace std::placeholders;

namespace NPL
{

namespace A1
{

void
FunctionContextHandler::operator()(TermNode& term, ContextNode& ctx) const
{
	auto& con(term.GetContainerRef());

	// NOTE: Arguments evaluation: applicative order.
	ReduceArguments(con, ctx);

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
		Handler(term, ctx);
	}
	// TODO: Unreduced form check.
#if 0
	if(n == 0)
		YTraceDe(Warning, "Empty reduced form found.");
	else
		YTraceDe(Warning, "%zu term(s) not reduced found.", n);
#endif
}


FunctionContextHandler
FunctionFormHandler::Wrap(std::function<void(TNIter, size_t, TermNode&)> f)
{
	return [f](TermNode& term, const ContextNode&){
		auto& con(term.GetContainerRef());
		const auto size(con.size());

		YAssert(size != 0, "Invalid term found.");
		// NOTE: This is basically call-by-value, i.e. the arguments would be
		//	copied as the parameters, unless the terms are prevented to be
		//	evaluated immediately by some special tags in reduction.
		f(con.begin(), size - 1, term);
	};
}


NPLContext::NPLContext()
	: Root(SetupRoot(std::bind(std::ref(ListTermPreprocess), _1, _2)))
{}

TermNode
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	auto term(SContext::Analyze(Session(unit)));

	Preprocess(term);
	Reduce(term, Root);
	return term;
}

ContextNode
NPLContext::SetupRoot(EvaluationPasses passes)
{
	ContextNode root;
#if NPL_TraceDepth
	SetupTraceDepth(root);
#endif
	passes += ReduceFirst,
	passes += [](TermNode& term, ContextNode& ctx){
		// TODO: Form evaluation: macro expansion, etc.
		if(!term.empty())
		{
			const auto& fm(Deref(ystdex::as_const(term).begin()));

			if(const auto p_handler = AccessPtr<ContextHandler>(fm))
				(*p_handler)(term, ctx);
			else
			{
				const auto p(AccessPtr<string>(fm));

				// TODO: Capture contextual information in error.
				throw LoggedEvent(ystdex::sfmt("No matching form '%s'"
					" with %zu argument(s) found.", p ? p->c_str()
					: "#<unknown>", term.size()), Err);
			}
		}
		return false;
	};
	AccessListPassesRef(root) = std::move(passes);
	AccessLeafPassesRef(root) = [](TermNode& term, ContextNode& ctx) -> bool{
		if(const auto p = AccessPtr<string>(term))
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
		}
		return {};
	};
	return root;
}

} // namespace A1;

} // namespace NPL;

