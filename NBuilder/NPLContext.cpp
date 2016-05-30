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
\version r2051
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-05-30 10:35 +0800
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

NPLContext::NPLContext()
	: Root(SetupRoot(std::bind(std::ref(ListTermPreprocess), _1, _2)))
{}

TermNode
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found.", Alert);

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
	passes += ReduceFirst;
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
				throw ListReductionFailure(ystdex::sfmt("No matching form '%s'"
					" with %zu argument(s) found.", p ? p->c_str()
					: "#<unknown>", term.size()));
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
						throw UndeclaredIdentifier(ystdex::sfmt(
							"Undeclared identifier '%s' found", id.c_str()));
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

