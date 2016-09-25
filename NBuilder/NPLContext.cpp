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
\version r2134
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2016-09-25 23:53 +0800
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
LoadSequenceSeparators(ContextNode& ctx, EvaluationPasses& passes)
{
	RegisterSequenceContextTransformer(passes, ctx, "$;", string(";")),
	RegisterSequenceContextTransformer(passes, ctx, "$,", string(","));
}


NPLContext::NPLContext()
{
#if NPL_TraceDepth
	SetupTraceDepth(Root);
#endif
	Setup(Root, std::bind(std::ref(ListTermPreprocess), _1, _2));
}

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

void
NPLContext::Setup(ContextNode& root, EvaluationPasses passes)
{
	// TODO: Simplify by using %ReductionStatus as invocation result directly
	//	in YSLib. Otherwise current versions of G++ would crash here as internal
	//	compiler error: "error reporting routines re-entered".
//	passes += ReduceFirst;
	passes += [](TermNode& term, ContextNode& ctx){
		return ReduceFirst(term, ctx) != ReductionStatus::Success;
	};
	// TODO: Insert more form evaluation passes: macro expansion, etc.
//	passes += EvaluateContextFirst;
	passes += [](TermNode& term, ContextNode& ctx){
		return EvaluateContextFirst(term, ctx) != ReductionStatus::Success;
	};
	AccessListPassesRef(root) = std::move(passes);
	AccessLeafPassesRef(root) = [](TermNode& term, ContextNode& ctx){
		return ystdex::call_value_or([&](string_view id) -> ReductionStatus{
			YAssertNonnull(id.data());
			// NOTE: Only string node of identifier is tested.
			if(!id.empty())
			{
				// NOTE: If necessary, they should have been handled in
				//	preprocess pass.
				if(id == "," || id == ";")
				{
					term.Clear();
					return ReductionStatus::NeedRetry;
				}
				else if(!id.empty())
				{
					// NOTE: Value rewriting.
					// TODO: Implement general literal check.
					if(CheckLiteral(id) == char() && !std::isdigit(id.front()))
						EvaluateIdentifier(term, ctx, id);
				}
				// XXX: Empty token is ignored.
				// XXX: Remained reducible?
			}
			return ReductionStatus::Success;
		// FIXME: Success on node conversion failure?
		}, AccessPtr<string>(term), ReductionStatus::Success)
			!= ReductionStatus::Success;
	};
}

} // namespace A1;

} // namespace NPL;

