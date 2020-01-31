/*
	© 2013-2020 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.cpp
\ingroup NBuilder
\brief NPL 解释器。
\version 895
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2020-01-31 19:00 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Interpreter
*/


#include "Interpreter.h"
#include <Helper/YModules.h>
#include <iostream>
#include YFM_YCLib_YCommon
#include YFM_Helper_Environment
#include YFM_YSLib_Service_TextFile
#include <cstdio> // for std::fprintf;

using namespace YSLib;

#define NPLC_Impl_TracePerform true
#define NPLC_Impl_TracePerformDetails false
#define NPLC_Impl_UseDebugMR false
#define NPLC_Impl_UseMonotonic false
#define NPLC_Impl_TestMemoryResource false
#define NPLC_Impl_LogBeforeReduce false
#define NPLC_Impl_FastAsyncReduce true

namespace NPL
{

#define NPLC_NAME "NPL console"
#define NPLC_VER "b30xx"
#define NPLC_PLATFORM "[MinGW32]"
yconstexpr auto prompt("> ");
yconstexpr auto title(NPLC_NAME" " NPLC_VER" @ (" __DATE__", " __TIME__") "
	NPLC_PLATFORM);

/// 519
namespace
{

/// 520
using namespace platform_ex;

/// 755
YB_NONNULL(3) void
PrintError(Terminal& terminal, const LoggedEvent& e, const char* name = "Error")
{
	terminal.UpdateForeColor(ErrorColor);
	YF_TraceRaw(e.GetLevel(), "%s[%s]<%u>: %s", name, typeid(e).name(),
		unsigned(e.GetLevel()), e.what());
//	ExtractAndTrace(e, e.GetLevel());
}

/// 852
inline PDefH(const TermNode&, MapToTermNode, const ValueNode& nd) ynothrowv
	ImplRet(Deref(static_cast<const TermNode*>(static_cast<const void*>(&nd))))

/// 852
//@{
void
PrintTermNode(std::ostream& os, const TermNode& term,
	TermNodeToString term_to_str, IndentGenerator igen = DefaultGenerateIndent,
	size_t depth = 0)
{
	PrintIndent(os, igen, depth);

	const auto print_node_str([&](const TermNode& subterm){
		return ResolveTerm([&](const TermNode& tm,
			ResolvedTermReferencePtr p_ref) -> pair<lref<const TermNode>, bool>{
			if(p_ref)
			{
				const auto tags(p_ref->GetTags());

				os << "*[" << (bool(tags & TermTags::Unique) ? 'U' : ' ')
					<< (bool(tags & TermTags::Nonmodifying) ? 'N' : ' ')
					<< (bool(tags & TermTags::Temporary) ? 'T' : ' ') << ']';
			}
			try
			{
				os << term_to_str(tm) << '\n';
				return {tm, true};
			}
			CatchIgnore(bad_any_cast&)
			return {tm, false};
		}, subterm);
	});
	const auto pr(print_node_str(term));

	if(!pr.second)
	{
		const auto& vterm(pr.first.get());

		os << '\n';
		if(vterm)
			TraverseNodeChildAndPrint(os, vterm, [&]{
				PrintIndent(os, igen, depth);
			}, print_node_str, [&](const TermNode& nd){
				return PrintTermNode(os, nd, term_to_str, igen, depth + 1);
			});
	}
}

string
StringifyValueObject(const ValueObject& vo)
{
	if(vo != A1::ValueToken::Null)
	{
		if(const auto p = vo.AccessPtr<string>())
			return *p;
		if(const auto p = vo.AccessPtr<TokenValue>())
			return sfmt<string>("[TokenValue] %s", p->c_str());
		if(const auto p = vo.AccessPtr<A1::ValueToken>())
			return sfmt<string>("[ValueToken] %s", to_string(*p).c_str());
		if(const auto p = vo.AccessPtr<bool>())
			return *p ? "[bool] #t" : "[bool] #f";
		if(const auto p = vo.AccessPtr<int>())
			return sfmt<string>("[int] %d", *p);
		if(const auto p = vo.AccessPtr<unsigned>())
			return sfmt<string>("[uint] %u", *p);
		if(const auto p = vo.AccessPtr<double>())
			return sfmt<string>("[double] %lf", *p);

		const auto& t(vo.type());

		if(t != ystdex::type_id<void>())
			return ystdex::quote(string(t.name()), '[', ']');
	}
	throw ystdex::bad_any_cast();
}

string
LogValueObject(const ValueObject& vo)
{
	return EscapeLiteral(StringifyValueObject(vo));
}


struct NodeValueLogger
{
	template<class _tNode>
	string
	operator()(const _tNode& node) const
	{
		return LogValueObject(node.Value);
	}
};
//@}

/// 867
//@{
using ystdex::ceiling_lb;
yconstexpr const auto min_block_size(resource_pool::adjust_for_block(1, 1));
yconstexpr const size_t init_pool_num(yimpl(12));
#if YB_IMPL_GNUCPP >= 30400 || __has_builtin(__builtin_clz)
yconstexpr const auto min_lb_size(sizeof(unsigned) * CHAR_BIT
	- __builtin_clz(unsigned(min_block_size - 1)));
yconstexpr const size_t
	max_fast_block_size(1U << (min_lb_size + init_pool_num));
#else
const auto min_lb_size(ceiling_lb(min_block_size));
const size_t max_fast_block_size(1U << (min_lb_size + init_pool_num));
#endif
//@}

} // unnamed namespace;

shared_pool_resource::shared_pool_resource(
	const pool_options& opts, memory_resource* p_up) ynothrow
	: saved_options(opts), oversized((yconstraint(p_up), *p_up)), pools(p_up)
{
	adjust_pool_options(saved_options);
	pools.reserve(init_pool_num);
	for(size_t i(0); i < init_pool_num; ++i)
		emplace(pools.cend(), min_lb_size + i);
}

void
shared_pool_resource::release() yimpl(ynothrow)
{
	oversized.release();
	yassume(pools.size() >= init_pool_num);
	pools.erase(pools.begin() + init_pool_num, pools.end());
	for(size_t i(0); i < init_pool_num; ++i)
		pools[i].clear();
	pools.shrink_to_fit();
}

void*
shared_pool_resource::do_allocate(size_t bytes, size_t alignment)
{
	if(bytes <= saved_options.largest_required_pool_block)
	{
		const auto ms(resource_pool::adjust_for_block(bytes, alignment));
		const auto lb_size(ceiling_lb(ms));

		if(ms <= max_fast_block_size)
		{
			yassume(lb_size >= min_lb_size);
			return (pools.begin()
				+ pools_t::difference_type(lb_size - min_lb_size))->allocate();
		}

		auto pr(find_pool(lb_size));

		if(!([&]() YB_PURE{
			return pr.first != pools.end();
		}() && pr.first->get_extra_data() == pr.second))
			pr.first = emplace(pr.first, pr.second);
		return pr.first->allocate();
	}
	return oversized.allocate(bytes, alignment);
}

void
shared_pool_resource::do_deallocate(void* p, size_t bytes, size_t alignment)
	yimpl(ynothrowv)
{
	if(bytes <= saved_options.largest_required_pool_block)
	{
		const auto ms(resource_pool::adjust_for_block(bytes, alignment));
		const auto lb_size(ceiling_lb(ms));

		if(ms <= max_fast_block_size)
		{
			yassume(lb_size >= min_lb_size);
			return (pools.begin() + pools_t::difference_type(
				lb_size - min_lb_size))->deallocate(p);
		}

		auto pr(find_pool(lb_size));

		yverify([&]() YB_PURE{
			return pr.first != pools.end();
		}() && pr.first->get_extra_data() == pr.second);
		return pr.first->deallocate(p);
	}
	else
		oversized.deallocate(p, bytes, alignment);
}

bool
shared_pool_resource::do_is_equal(const memory_resource& other) const ynothrow
{
	return this == &other;
}

shared_pool_resource::pools_t::iterator
shared_pool_resource::emplace(pools_t::const_iterator i, size_t lb_size)
{
	return pools.emplace(i, *upstream_resource(),
		std::min(size_t(PTRDIFF_MAX >> lb_size),
		saved_options.max_blocks_per_chunk), size_t(1) << lb_size, lb_size);
}

std::pair<shared_pool_resource::pools_t::iterator, size_t>
shared_pool_resource::find_pool(size_t lb_size) ynothrow
{
	return {ystdex::lower_bound_n(pools.begin(),
		pools_t::difference_type(pools.size()),
		lb_size, [](const resource_pool& a, size_t lb)
		YB_ATTR_LAMBDA_QUAL(ynothrow, YB_PURE){
		return a.get_extra_data() < lb;
	}), lb_size};
}


void
LogTree(const ValueNode& node, Logger::Level lv)
{
	std::ostringstream oss;

	PrintNode(oss, node, NodeValueLogger());
	YTraceDe(lv, "%s", oss.str().c_str());
}

void
LogTermValue(const TermNode& term, Logger::Level lv)
{
	std::ostringstream oss;

	PrintTermNode(oss, term, NodeValueLogger());
	YTraceDe(lv, "%s", oss.str().c_str());
}

/// 845
namespace
{

using namespace pmr;

#if NPLC_Impl_TestMemoryResource
struct test_memory_resource : public memory_resource,
	private ystdex::noncopyable, private ystdex::nonmovable
{
	lref<memory_resource> underlying;
	unordered_map<void*, pair<size_t, size_t>> ump{};

	test_memory_resource()
		: underlying(*new_delete_resource())
	{}

	YB_ALLOCATOR void*
	do_allocate(size_t bytes, size_t alignment) override
	{
		const auto p(underlying.get().allocate(bytes, alignment));

		if(ump.find(p) == ump.cend())
		{
			ump.emplace(p, make_pair(bytes, alignment));
#if NPLC_Impl_UseDebugMR
			std::fprintf(stderr, "Info: Allocated '%p' with bytes"
				" '%zu' and alignment '%zu'.\n", p, bytes, alignment);
#endif
		}
		else
			std::fprintf(stderr,
				"Error: Duplicate alloction for '%p' found.\n", p);
		return p;
	}

	void
	do_deallocate(void* p, size_t bytes, size_t alignment) ynothrow override
	{
		const auto i(ump.find(p));

		if(i != ump.cend())
		{
			const auto rbytes(i->second.first);
			const auto ralign(i->second.second);

			if(rbytes == bytes && ralign == alignment)
			{
				underlying.get().deallocate(p, bytes, alignment);
#if NPLC_Impl_UseDebugMR
				std::fprintf(stderr, "Info: Deallocated known '%p' with bytes"
					" '%zu' and alignment '%zu'.\n", p, bytes, alignment);
#endif
			}
			else
				std::fprintf(stderr, "Error: Found known '%p' with bytes '%zu'"
					" and alignment '%zu' in deallocation, distinct to"
					" recorded bytes '%zu' and alignment '%zu'.\n", p, bytes,
					alignment, rbytes, ralign);
			ump.erase(i);
		}
		else
			std::fprintf(stderr, "Error: Found unknown '%p' with bytes '%zu'"
				" and alignment '%zu' in deallocation.\n", p, bytes, alignment);
	}

	bool
	do_is_equal(const memory_resource& other) const ynothrow override
	{
		return this == &other;
	}
};
#endif

memory_resource&
GetPoolResourceRef()
{
#if NPLC_Impl_TestMemoryResource
	static test_memory_resource r;

	return r;
#else
	return Deref(pmr::new_delete_resource());
#endif
}

#if NPLC_Impl_UseMonotonic
memory_resource&
GetMonotonicPoolRef()
{
	static pmr::monotonic_buffer_resource r;

	return r;
}

#	define NPLC_Impl_PoolName GetMonotonicPoolRef()
#else
#	define NPLC_Impl_PoolName pool_rsrc
#endif

#if NPLC_Impl_FastAsyncReduce
/// 881
ReductionStatus
ReduceOnceFast(TermNode& term, A1::ContextState& cs,
	const A1::EvaluationPasses& preprocess)
{
	if(bool(term.Value))
	{
		// XXX: There are several administrative differences to the original
		//	%ContextState::DefaultReduceOnce. For leaf values other than value
		//	tokens, the next term is set up in the original implemenation but
		//	omitted here, as it is not relied on in this implementation.
		if(const auto p = TermToNamePtr(term))
		{
			using namespace A1::Forms;
			string_view id(*p);

			YAssertNonnull(id.data());
			if(!id.empty()
				&& CheckReducible(HandleExtendedLiteral(term, cs, id)))
			{
				if(!IsNPLAExtendedLiteral(id))
					return EvaluateIdentifier(term, cs, id);
				ThrowInvalidSyntaxError(ystdex::sfmt(id.front() != '#'
					? "Unsupported literal prefix found in literal '%s'."
					: "Invalid literal '%s' found.", id.data()));
			}
		}
	}
	else if(term.size() == 1)
	{
		auto term_ref(ystdex::ref(term));

		ystdex::retry_on_cond([&]{
			return term_ref.get().size() == 1;
		}, [&]{
			term_ref = AccessFirstSubterm(term_ref);
		});
		return A1::ReduceAgainLifted(term, cs, term_ref);
	}
	else if(IsBranch(term))
	{
		YAssert(term.size() > 1, "Invalid node found.");
		// XXX: These passes are known safe to synchronize.
		preprocess(term, cs);
		ReduceHeadEmptyList(term);
		YAssert(IsBranchedList(term), "Invalid node found.");
		cs.SetNextTermRef(term);
		cs.LastStatus = ReductionStatus::Neutral;
		RelaySwitched(cs, [&](ContextNode& c){
			A1::ContextState::Access(c).SetNextTermRef(term);
			// TODO: Expose internal implementation of
			//	%A1::ReduceCombined.
			return A1::ReduceCombined(term, c);
		});
		return RelaySwitched(cs, [&]{
			return ReduceOnceFast(AccessFirstSubterm(term), cs, preprocess);
		});
	}
	return ReductionStatus::Retained;
}
#endif

} // unnamed namespace;

Interpreter::Interpreter(Application& app,
	std::function<void(REPLContext&)> loader)
	: terminal(), err_threshold(RecordLevel(0x10)),
	pool_rsrc(&GetPoolResourceRef()), line(), context(NPLC_Impl_PoolName)
{
	using namespace std;
	using namespace platform_ex;

#if NPLC_Impl_TracePerformDetails
	SetupTraceDepth(context.Root);
#endif
	// TODO: Avoid reassignment of default passes?
#if NPLC_Impl_FastAsyncReduce
	if(context.IsAsynchronous())
		// XXX: Only safe and meaningful for asynchrnous implementations.
		context.Root.ReduceOnce.Handler = [&](TermNode& term, ContextNode& ctx){
			return ReduceOnceFast(term, A1::ContextState::Access(ctx),
				context.ListTermPreprocess);
		};
#elif NPLC_Impl_LogBeforeReduce
	using namespace placeholders;
	A1::EvaluationPasses
		passes(std::bind(std::ref(context.ListTermPreprocess), _1, _2));

	passes += [](TermNode& term){
		YTraceDe(Notice, "Before ReduceCombined:");
		LogTermValue(term, Notice);
		return ReductionStatus::Retained;
	};
	passes += ReduceHeadEmptyList;
	passes += A1::ReduceFirst;
	passes += A1::ReduceCombined;
	context.Root.EvaluateList = std::move(passes);
#endif
	terminal.UpdateForeColor(TitleColor);
	cout << title << endl << "Initializing...";
	p_env.reset(new YSLib::Environment(app));
	loader(context);
	terminal.UpdateForeColor(InfoColor);
	cout << "Type \"exit\" to exit,"
		" \"cls\" to clear screen, \"help\", \"about\", or \"license\""
		" for more information." << endl << endl;
}

void
Interpreter::HandleSignal(SSignal e)
{
	using namespace std;

	static yconstexpr auto not_impl("Sorry, not implemented: ");

	switch(e)
	{
	case SSignal::ClearScreen:
		terminal.Clear();
		break;
	case SSignal::About:
		cout << not_impl << "About" << endl;
		break;
	case SSignal::Help:
		cout << not_impl << "Help" << endl;
		break;
	case SSignal::License:
		cout << "See license file of the YSLib project." << endl;
		break;
	default:
		YAssert(false, "Wrong command!");
	}
}

bool
Interpreter::Process()
{
	using namespace platform_ex;

	if(!line.empty())
	{
		terminal.UpdateForeColor(SideEffectColor);
		try
		{
			line = DecodeArg(line);

			auto res(context.Perform(line));

#if NPLC_Impl_TracePerform
		//	terminal.UpdateForeColor(InfoColor);
		//	cout << "Unrecognized reduced token list:" << endl;
			terminal.UpdateForeColor(ReducedColor);
			LogTermValue(res);
#endif
		}
		catch(SSignal e)
		{
			if(e == SSignal::Exit)
				return {};
			terminal.UpdateForeColor(SignalColor);
			HandleSignal(e);
		}
		CatchExpr(NPLException& e, PrintError(terminal, e, "NPLException"))
		catch(LoggedEvent& e)
		{
			if(e.GetLevel() < err_threshold)
				throw;
			PrintError(terminal, e);
		}
	}
	return true;
}

bool
Interpreter::SaveGround()
{
	if(!p_ground)
	{
		auto& ctx(context.Root);

		p_ground = NPL::SwitchToFreshEnvironment(ctx,
			ValueObject(ctx.WeakenRecord()));
		return true;
	}
	return {};
}

std::istream&
Interpreter::WaitForLine()
{
	return WaitForLine(std::cin, std::cout);
}
std::istream&
Interpreter::WaitForLine(std::istream& is, std::ostream& os)
{
	terminal.UpdateForeColor(PromptColor);
	os << prompt;
	terminal.UpdateForeColor(DefaultColor);
	return std::getline(is, line);
}

} // namespace NPL;

