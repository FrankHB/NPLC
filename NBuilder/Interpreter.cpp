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
\version r1278
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2020-04-04 19:04 +0800
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
#include YFM_NPL_NPLA1Forms
#include <cstdio> // for std::fprintf;

#define NPLC_Impl_TracePerform true
#define NPLC_Impl_TracePerformDetails false
#define NPLC_Impl_UseDebugMR false
#define NPLC_Impl_UseMonotonic false
#define NPLC_Impl_TestMemoryResource false
#define NPLC_Impl_LogBeforeReduce false
#define NPLC_Impl_FastAsyncReduce true
#if YCL_Linux
//#	define NPLC_Impl_mimalloc true
// XXX: Hard-coded for AUR package 'mimalloc-git'. Set environment variable
//	'LDFLAGS' to '/usr/lib/mimalloc-1.4/mimalloc.o -pthread' to link in with
//	%SHBuild-BuildApp.sh, before the build system is ready to configure the
//	paths. Alternatively, 'LD_PRELOAD=/usr/lib/mimalloc-1.4/libmimalloc.so'
//	can be used without link the library.
#	define NPLC_Impl_ExtInc_mimalloc </usr/lib/mimalloc-1.4/include/mimalloc.h>
#endif

#if NPLC_Impl_mimalloc
#	include NPLC_Impl_ExtInc_mimalloc
#endif

using namespace YSLib;

namespace NPL
{

yconstexpr auto prompt("> ");
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

YB_ATTR_nodiscard YB_PURE string
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

YB_ATTR_nodiscard YB_PURE string
LogValueObject(const ValueObject& vo)
{
	return EscapeLiteral(StringifyValueObject(vo));
}


struct NodeValueLogger
{
	template<class _tNode>
	YB_ATTR_nodiscard YB_PURE string
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
/// 885
static_assert(init_pool_num > 1, "Invalid pool configuration found.");
#if YB_IMPL_GNUCPP >= 30400 || __has_builtin(__builtin_clz)
yconstexpr const auto min_lb_size(sizeof(unsigned) * CHAR_BIT
	- __builtin_clz(unsigned(min_block_size - 1)));
/// 885
yconstexpr const size_t max_fast_block_shift(min_lb_size + init_pool_num - 1);
yconstexpr const size_t
	max_fast_block_size(1U << max_fast_block_shift);
#else
const auto min_lb_size(ceiling_lb(min_block_size));
/// 885
const size_t max_fast_block_shift(min_lb_size + init_pool_num - 1);
const size_t max_fast_block_size(1U << max_fast_block_shift);
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
	return pools.emplace(i, *upstream_resource(), std::min(
		size_t(PTRDIFF_MAX >> lb_size), saved_options.max_blocks_per_chunk),
		size_t(1) << lb_size, lb_size, (1U << lb_size)
		< (1U << init_pool_num) / resource_pool::default_capacity
		? size_t(1 << size_t(init_pool_num - lb_size))
		: resource_pool::default_capacity);
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
	ostringstream oss(string(node.get_allocator()));

	PrintNode(oss, node, NodeValueLogger());
	YTraceDe(lv, "%s", oss.str().c_str());
}

void
LogTermValue(const TermNode& term, Logger::Level lv)
{
	ostringstream oss(string(term.get_allocator()));

	PrintTermNode(oss, term, NodeValueLogger());
	YTraceDe(lv, "%s", oss.str().c_str());
}

/// 845
namespace
{

using namespace pmr;

/// 884
//@{
#if NPLC_Impl_mimalloc
class default_memory_resource : public memory_resource
{
	//! \invariant \c p_heap 。
	::mi_heap_t* p_heap;

public:
	default_memory_resource()
		: p_heap(::mi_heap_get_default())
	{}

	YB_ALLOCATOR void*
	do_allocate(size_t bytes, size_t alignment) override
	{
#	if true
		return ystdex::retry_on_cond([](void* p) -> bool{
			if(p)
				return {};
			if(const auto h = std::get_new_handler())
			{
				h();
				return true;
			}
			throw std::bad_alloc();
		}, [&]() ynothrow{
			return ::mi_heap_malloc_aligned_at(p_heap, bytes, alignment, 0);
		});
#	else
		// NOTE: This is equivalent, but slightly inefficient.
		return ::mi_new_aligned(bytes, alignment);
#	endif
	}

	void
	do_deallocate(void* p, size_t bytes, size_t alignment) ynothrow override
	{
		::mi_free_size_aligned(p, bytes, alignment);
	}

	YB_STATELESS bool
	do_is_equal(const memory_resource& other) const ynothrow override
	{
		return this == &other;
	}
};
#endif
//@}

/// 881
YB_ATTR_nodiscard memory_resource&
GetDefaultResourceRef() ynothrowv
{
#if NPLC_Impl_mimalloc
	static default_memory_resource r;

	return r;
#else
	return Deref(pmr::new_delete_resource());
#endif
}

#if NPLC_Impl_TestMemoryResource
struct test_memory_resource : public memory_resource,
	private ystdex::noncopyable, private ystdex::nonmovable
{
	lref<memory_resource> underlying;
	unordered_map<void*, pair<size_t, size_t>> ump{};

	test_memory_resource()
		: underlying(GetDefaultResourceRef())
	{}

	YB_ALLOCATOR void*
	do_allocate(size_t bytes, size_t alignment) override
	{
		const auto p(underlying.get().allocate(bytes, alignment));

		if(ump.find(p) == ump.cend())
		{
			ump.emplace(p, make_pair(bytes, alignment));
			// XXX: All errors from %std::fprintf are ignored.
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

	YB_STATELESS bool
	do_is_equal(const memory_resource& other) const ynothrow override
	{
		return this == &other;
	}
};
#endif

/// 881
YB_ATTR_nodiscard memory_resource&
GetPoolResourceRef() ynothrowv
{
#if NPLC_Impl_TestMemoryResource
	static test_memory_resource r;

	return r;
#else
	return GetDefaultResourceRef();
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
// XXX: There are several fast asynchrnous reduction assumptions from NPLA1.
//	1. For the leaf values other than value tokens, the next term is not relied
//		on.
//	2. There are no changes on passes and handlers like %ContextNode::Resolve
//		after the initialization.
//	3. No literal handlers rely on the value of the reduced term.
/// 883
//@{
template<typename _func>
ReductionStatus
HandleLiteralOrCall(string_view id, _func f)
{
	if(!IsNPLAExtendedLiteral(id))
		return f();
	A1::Forms::ThrowInvalidSyntaxError(ystdex::sfmt(
		id.front() != '#' ? "Unsupported literal prefix found in literal '%s'."
		: "Invalid literal '%s' found.", id.data()));
}

template<typename _func, typename _func2>
YB_FLATTEN inline ReductionStatus
ReduceFastIdOr(TermNode& term, _func f, _func2 f2)
{
	// XXX: There are several administrative differences to the original
	//	%ContextState::DefaultReduceOnce. For leaf values other than value
	//	tokens, the next term is set up in the original implemenation but
	//	omitted here, cf. assumption 1.
	return [&]() YB_ATTR(always_inline){
		if(const auto p = TermToNamePtr(term))
		{
			string_view id(*p);

			YAssert(IsLeaf(term),
				"Unexpected irregular representation of term found.");
			if(A1::HandleCheckedExtendedLiteral(term, id))
				return f(id);
		}
		return f2();
	}();
}

/// 884
//@{
template<typename _func>
YB_FLATTEN ReductionStatus
ReduceResolved(_func f, const ContextNode& ctx, string_view id)
{
	// XXX: See assumption 2.
	auto pr(ContextNode::DefaultResolve(ctx.GetRecordRef(), id));

	if(pr.first)
		return f(*pr.first, pr.second);
	throw BadIdentifier(id);
}

void
SetupTermEvaluatedTermOrRef(TermNode& term, Environment& env, TermNode& bound,
	TermNode& nd, ResolvedTermReferencePtr p_ref)
{
	if(p_ref)
		[&]() YB_FLATTEN{
			term.SetContent(bound.GetContainer(),
				ValueObject(std::allocator_arg, term.get_allocator(),
				in_place_type<TermReference>,
				p_ref->GetTags() & ~TermTags::Unique, *p_ref));
		}();
	else
		term.Value = ValueObject(std::allocator_arg, term.get_allocator(),
			in_place_type<TermReference>, env.MakeTermTags(nd)
			& ~TermTags::Unique, nd, env.shared_from_this());
}
//@}

ReductionStatus
ReduceFastHNF(TermNode& term, A1::ContextState& cs, TermNode& sub,
	string_view id)
{
	return ReduceResolved([&](TermNode& bound, Environment& env){
		return ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
			[&](TermNode& tm, Environment& e, ResolvedTermReferencePtr p){
				SetupTermEvaluatedTermOrRef(tm, e, bound, nd, p);
			}(sub, env, p_ref);
			// XXX: This is safe, cf. assumption 3.
			EvaluateLiteralHandler(yimpl(sub), cs, nd);
			cs.SetNextTermRef(term);
			return ReduceCombinedReferent(term, cs, nd);
		}, bound);
	}, cs, id);
}

ReductionStatus
ReduceFastBranch(TermNode& term, A1::ContextState& cs)
{
	if(term.size() == 1)
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
		if(IsEmpty(AccessFirstSubterm(term)))
			RemoveHead(term);
		YAssert(IsBranchedList(term), "Invalid node found.");
		cs.SetNextTermRef(term);
		cs.LastStatus = ReductionStatus::Neutral;

		auto& sub(AccessFirstSubterm(term));

		if(bool(sub.Value))
			return [&]() YB_ATTR(always_inline){
				return ReduceFastIdOr(sub, [&](string_view id){
					return HandleLiteralOrCall(id, [&]() YB_FLATTEN{
						return ReduceFastHNF(term, cs, sub, id);
					});
				}, [&]{
					return ReduceNextCombinedBranch(term, cs);
				});
			}();
		RelaySwitched(cs, [&](ContextNode& c){
			return ReduceNextCombinedBranch(term, A1::ContextState::Access(c));
		});
		return RelaySwitched(cs, [&]{
			return ReduceFastBranch(sub, cs);
		});
	}
	return ReductionStatus::Retained;
}
//@}

/// 882
ReductionStatus
ReduceOnceFast(TermNode& term, A1::ContextState& cs)
{
	return bool(term.Value)
		? ReduceFastIdOr(term, [&](string_view id) YB_FLATTEN{
		return HandleLiteralOrCall(id, [&]() YB_FLATTEN{
			return ReduceResolved([&](TermNode& bound, Environment& env){
				ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
					SetupTermEvaluatedTermOrRef(term, env, bound, nd, p_ref);
					A1::EvaluateLiteralHandler(term, cs, nd);
				}, bound);
				return ReductionStatus::Neutral;
			}, cs, id);
		});
	}, []() YB_ATTR_LAMBDA_QUAL(ynothrow, YB_STATELESS){
		return ReductionStatus::Retained;
	}) : ReduceFastBranch(term, cs);
}
#endif

} // unnamed namespace;

Interpreter::Interpreter()
	: terminal(), err_threshold(RecordLevel(0x10)),
	pool_rsrc(&GetPoolResourceRef()), line(), Context(NPLC_Impl_PoolName)
{
	using namespace std;
	using namespace platform_ex;

#if NPLC_Impl_TracePerformDetails
	SetupTraceDepth(Context.Root);
#endif
	// TODO: Avoid reassignment of default passes?
#if NPLC_Impl_FastAsyncReduce
	if(Context.IsAsynchronous())
		// XXX: Only safe and meaningful for asynchrnous implementations.
		Context.Root.ReduceOnce.Handler = [&](TermNode& term, ContextNode& ctx){
			return ReduceOnceFast(term, A1::ContextState::Access(ctx));
		};
#elif NPLC_Impl_LogBeforeReduce
	using namespace placeholders;
	A1::EvaluationPasses
		passes(std::bind(std::ref(Context.ListTermPreprocess), _1, _2));

	passes += [](TermNode& term){
		YTraceDe(Notice, "Before ReduceCombined:");
		LogTermValue(term, Notice);
		return ReductionStatus::Retained;
	};
	passes += ReduceHeadEmptyList;
	passes += A1::ReduceFirst;
	passes += A1::ReduceCombined;
	Context.Root.EvaluateList = std::move(passes);
#endif
}

void
Interpreter::EnableExtendedLiterals()
{
	Context.Root.EvaluateLiteral += A1::FetchExtendedLiteralPass();
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
		UpdateTextColor(SideEffectColor);
		try
		{
			line = DecodeArg(line);

			auto res(Context.Perform(line));

#if NPLC_Impl_TracePerform
		//	UpdateTextColor(InfoColor);
		//	cout << "Unrecognized reduced token list:" << endl;
			UpdateTextColor(ReducedColor);
			LogTermValue(res);
#endif
		}
		catch(SSignal e)
		{
			if(e == SSignal::Exit)
				return {};
			UpdateTextColor(SignalColor);
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
Interpreter::ProcessLine(string unit)
{
	line = std::move(unit);
	return Process();
}

bool
Interpreter::SaveGround()
{
	if(!p_ground)
	{
		auto& ctx(Context.Root);

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
	UpdateTextColor(PromptColor);
	os << prompt;
	UpdateTextColor(DefaultColor);
	return std::getline(is, line);
}

} // namespace NPL;

