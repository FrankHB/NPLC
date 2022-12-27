/*
	© 2013-2022 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.cpp
\ingroup NBuilder
\brief NPL 解释器。
\version r3744
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2022-12-27 08:24 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Interpreter
*/


#include "Interpreter.h" // for IsAtom, bad_any_cast, IsPair, type_id,
//	type_info, QueryTypeName, IsTyped, YSLib::sfmt, YSLib::ostringstream,
//	YAssertNonnull, LookupName, IsCombiningTerm, IsLeaf,
//	A1::QuerySourceInformation, IsList, RemoveHead, IsEmpty, AccessFirstSubterm,
//	trivial_swap, IsSingleElementList, LiftOtherValue, namespace YSLib,
//	TraceException, A1::TraceBacktrace, YSLib::IO::StreamGet;
#include <ystdex/functional.hpp> // for ystdex::bind1, std::bind, std::ref,
//	std::placeholders::_1;
#include <Helper/YModules.h>
#include YFM_YCLib_YCommon // for ystdex::quote;
#include <ystdex/expanded_function.hpp> // for ystdex::retry_on_cond;
#include <ystdex/string.hpp> // for ystdex::begins_with;
#include <cstring> // for std::strcmp, std::strstr;
#include YFM_NPL_NPLAMath // for FPToString;
#include <cstdio> // for std::fprintf, stderr;
#include <ystdex/scope_guard.hpp> // for ystdex::make_guard;
#include <iostream> // for std::cin, std::cout;

#if YCL_Linux
//#	define NPLC_Impl_mimalloc true
// XXX: Hard-coded for AUR package 'mimalloc-git'. Set environment variable
//	'SHBuild_LIBS' to '/usr/lib/mimalloc-1.6/mimalloc.o -pthread' to link in
//	with %SHBuild-BuildPkg.sh, before the build system is ready to configure the
//	paths. Alternatively, 'LD_PRELOAD=/usr/lib/mimalloc-1.6/libmimalloc.so'
//	can be used without link the library.
#	define NPLC_Impl_ExtInc_mimalloc </usr/lib/mimalloc-1.6/include/mimalloc.h>
#endif
#define NPLC_Impl_FastAsyncReduce true
#define NPLC_Impl_LogBeforeReduce false
#define NPLC_Impl_TestMemoryResource false
#define NPLC_Impl_TracePerform true
#define NPLC_Impl_TracePerformDetails false
#define NPLC_Impl_UseBacktrace true
#define NPLC_Impl_UseDebugMR false
#define NPLC_Impl_UseMonotonic false
#define NPLC_Impl_UseCheckedExtendedLiteral false
#define NPLC_Impl_UsePool true
#define NPLC_Impl_UseSourceInfo true

#if NPLC_Impl_mimalloc
#	include NPLC_Impl_ExtInc_mimalloc
#endif

namespace NBuilder
{

//! \since YSLib build 304
yconstexpr auto prompt("> ");

//! \since YSLib build 519
namespace
{

//! \since YSLib build 948
//@{
template<typename _func>
void
PrintTermNodeCore(std::ostream& os, const TermNode& term, _func f,
	size_t depth = 0, size_t idx = 0)
{
	if(depth != 0 && idx != 0)
		os << ' ';

	const auto non_list(!IsList(term));

	if(non_list && IsAtom(term))
		f(term);
	else
		PrintContainedNodes([&](char b){
			os << b;
		}, [&]{
			size_t i(0);

			for(auto& tm : term)
			{
				if(!IsSticky(tm.Tags))
					PrintTermNodeCore(os, tm, f, depth + 1, i);
				else
					break;
				++i;
			}
			if(non_list)
			{
				YAssert(i != 0, "Invalid state found.");
				os << " . ";
				f(term);
			}
		});
}

template<typename _func>
void
PrintTermNodeFiltered(std::ostream& os, const TermNode& term, _func f,
	size_t depth = 0, size_t idx = 0)
{
	PrintTermNodeCore(os, term, [&](const TermNode& tm){
		TryExpr(f(tm))
		CatchIgnore(bad_any_cast&)
	}, depth, idx);
}

template<typename _func>
void
PrintTermNode(std::ostream& os, const TermNode& term, _func f, bool pfx = {},
	size_t depth = 0, size_t idx = 0)
{
	PrintTermNodeFiltered(os, term, [&](const TermNode& tm){
		try
		{
			// XXX: As %ResolveTerm, without restriction of being an atom.
			if(const auto p = TryAccessLeaf<const TermReference>(tm))
			{
				auto& nd(p->get());

				if(pfx && p)
				{
					const auto tags(p->GetTags());
					const auto i(tm.size() - CountPrefix(tm));

					os << "*";
					if(i != 0)
						os << "{" << i << "}";
					os << "[" << (bool(tags & TermTags::Unique) ? 'U' : ' ')
						<< (bool(tags & TermTags::Nonmodifying) ? 'N' : ' ')
						<< (bool(tags & TermTags::Temporary) ? 'T' : ' ')
						<< ']';
				}
				if(IsPair(nd) || nd.Value.type() != type_id<TermReference>())
					PrintTermNode(os, nd, f, pfx, depth, idx);
				else
					PrintTermNodeFiltered(os, nd, std::bind(f, std::ref(os),
						std::placeholders::_1), depth, idx);
			}
			else
				f(os, tm);
		}
		CatchIgnore(bad_any_cast&)
	}, depth, idx);
}
//@}

//! \since YSLib build 889
//@{
YB_ATTR_nodiscard YB_ATTR_returns_nonnull YB_PURE const char*
DecodeTypeName(const type_info& ti)
{
	// NOTE: Some well-known types for unreadable objects are simplified.
	using namespace A1;
	const auto tname(QueryTypeName(ti));

	if(tname.data())
		return tname.data();
	if(IsTyped<ContextHandler>(ti))
		return "ContextHandler";
	if(IsTyped<LiteralHandler>(ti))
		return "LiteralHandler";
	if(IsTyped<ContextHandler::FuncType*>(ti))
		return "native function pointer";
	if(IsTyped<ystdex::expanded_caller<ContextHandler::FuncType,
		ReductionStatus(*)(NPL::TermNode&)>>(ti))
		return "expanded native function pointer";

	const auto name(ti.name());

	// XXX: The following cases are dependent on mangled names.
	// TODO: Use libcxxabi interface conditionally?
	if(std::strcmp(name, "N3NPL2A112_GLOBAL__N_110VauHandlerE") == 0)
		return "vau (without dynamic environment)";
	if(std::strcmp(name, "N3NPL2A112_GLOBAL__N_117DynamicVauHandlerE") == 0)
		return "vau";
	if(std::strstr(name, "GroundedDerived"))
		return "ground derived native handler";
	if(std::strstr(name, "_GLOBAL__"))
		return "native handler";
	return name;
}

YB_ATTR_nodiscard YB_PURE string
StringifyEnvironment(const shared_ptr<Environment>& p_env, bool weak)
{
	return YSLib::sfmt<string>("#[%senvironment: %p]", weak ? "weak " : "",
		ystdex::pvoid(p_env.get()));
}

template<class _tHandler>
YB_ATTR_nodiscard YB_PURE string
StringifyContextHandler(const _tHandler& h)
{
	if(const auto p = h.template target<A1::FormContextHandler>())
		switch(p->GetWrappingCount())
		{
		case 0:
			return "operative[" + StringifyContextHandler(p->Handler) + "]";
		case 1:
			return YSLib::sfmt<string>("applicative[%s]",
				StringifyContextHandler(p->Handler).c_str());
		default:
			return YSLib::sfmt<string>("applicative[%s, wrapping = %zu]",
				StringifyContextHandler(p->Handler).c_str(),
				p->GetWrappingCount());
		}
	if(const auto p = h.template target<A1::WrappedContextHandler<
		YSLib::GHEvent<void(NPL::TermNode&, NPL::ContextNode&)>>>())
		return "wrapped " + StringifyContextHandler(p->Handler);
	return string("ContextHandler: ") + DecodeTypeName(h.target_type());
}
//@}

//! \since YSLib build 929
YB_ATTR_nodiscard YB_PURE string
StringifyValueObjectDefault(const ValueObject& vo)
{
	const auto& ti(vo.type());

	// XXX: %NPL::TryAccessValue is not used. This ignores the possible
	//	exception thrown from the value holder.
	if(const auto p = vo.AccessPtr<bool>())
		return *p ? "#t" : "#f";
	if(const auto p = vo.AccessPtr<shared_ptr<Environment>>())
		return StringifyEnvironment(*p, {});
	if(const auto p = vo.AccessPtr<EnvironmentReference>())
		return StringifyEnvironment(p->GetPtr().lock(), true);
	if(const auto p = vo.AccessPtr<A1::ContextHandler>())
		return "#[" + StringifyContextHandler(*p) + "]";
	if(!IsTyped<void>(ti))
		return ystdex::quote(string(DecodeTypeName(ti)), "#[", ']');
	throw bad_any_cast();
}

//! \since YSLib build 932
//@{
// NOTE: The precisions of the format for flonums are as Racket BC. See
//	https://github.com/racket/racket/blob/d10b9e083ce3c2068f960ce924af88f34111db26/racket/src/bc/src/numstr.c#L1846-L1873.

YB_ATTR_nodiscard YB_PURE string
StringifyBasicObject(const ValueObject& vo)
{
	using YSLib::sfmt;

	// XXX: Ditto.
	if(const auto p = vo.AccessPtr<A1::ValueToken>())
		return sfmt<string>("%s", to_string(*p).c_str());
	if(const auto p = vo.AccessPtr<int>())
		return sfmt<string>("%d", *p);
	if(const auto p = vo.AccessPtr<unsigned>())
		return sfmt<string>("%u", *p);
	if(const auto p = vo.AccessPtr<long long>())
		return sfmt<string>("%lld", *p);
	if(const auto p = vo.AccessPtr<unsigned long long>())
		return sfmt<string>("%llu", *p);
	if(const auto p = vo.AccessPtr<double>())
		return FPToString(*p);
	if(const auto p = vo.AccessPtr<long>())
		return sfmt<string>("%ld", *p);
	if(const auto p = vo.AccessPtr<unsigned long>())
		return sfmt<string>("%lu", *p);
	if(const auto p = vo.AccessPtr<short>())
		return sfmt<string>("%hd", *p);
	if(const auto p = vo.AccessPtr<unsigned short>())
		return sfmt<string>("%hu", *p);
	if(const auto p = vo.AccessPtr<signed char>())
		return sfmt<string>("%d", int(*p));
	if(const auto p = vo.AccessPtr<unsigned char>())
		return sfmt<string>("%u", unsigned(*p));
	if(const auto p = vo.AccessPtr<float>())
		return FPToString(*p);
	if(const auto p = vo.AccessPtr<long double>())
		return FPToString(*p);
	return StringifyValueObjectDefault(vo);
}

YB_ATTR_nodiscard YB_PURE string
StringifyBasicObjectWithTypeTag(const ValueObject& vo)
{
	using YSLib::sfmt;

	// XXX: Ditto.
	if(const auto p = vo.AccessPtr<A1::ValueToken>())
		return sfmt<string>("[ValueToken] %s", to_string(*p).c_str());
	if(const auto p = vo.AccessPtr<int>())
		return sfmt<string>("[int] %d", *p);
	if(const auto p = vo.AccessPtr<unsigned>())
		return sfmt<string>("[uint] %u", *p);
	if(const auto p = vo.AccessPtr<long long>())
		return sfmt<string>("[longlong] %lld", *p);
	if(const auto p = vo.AccessPtr<unsigned long long>())
		return sfmt<string>("[ulonglong] %llu", *p);
	if(const auto p = vo.AccessPtr<double>())
		return "[double] " + FPToString(*p);
	if(const auto p = vo.AccessPtr<long>())
		return sfmt<string>("[long] %ld", *p);
	if(const auto p = vo.AccessPtr<unsigned long>())
		return sfmt<string>("[ulong] %lu", *p);
	if(const auto p = vo.AccessPtr<short>())
		return sfmt<string>("[short] %hd", *p);
	if(const auto p = vo.AccessPtr<unsigned short>())
		return sfmt<string>("[ushort] %hu", *p);
	if(const auto p = vo.AccessPtr<signed char>())
		return sfmt<string>("[char] %d", int(*p));
	if(const auto p = vo.AccessPtr<unsigned char>())
		return sfmt<string>("[uchar] %u", unsigned(*p));
	if(const auto p = vo.AccessPtr<float>())
		return "[float] " + FPToString(*p);
	if(const auto p = vo.AccessPtr<long double>())
		return "[longdouble] " + FPToString(*p);
	return StringifyValueObjectDefault(vo);
}

YB_ATTR_nodiscard YB_PURE string
StringifyValueObjectEscape(const ValueObject& vo,
	string(&fallback)(const ValueObject&))
{
	// XXX: Ditto.
	if(const auto p = vo.AccessPtr<string>())
		return ystdex::quote(EscapeLiteral(*p));
	if(const auto p = vo.AccessPtr<TokenValue>())
		return EscapeLiteral(*p);
	return fallback(vo);
}
//@}

//! \since YSLib build 852
YB_ATTR_nodiscard YB_PURE string
StringifyValueObject(const ValueObject& vo)
{
	return StringifyValueObjectEscape(vo, StringifyBasicObjectWithTypeTag);
}

//! \since YSLib build 928
YB_ATTR_nodiscard YB_PURE string
StringifyValueObjectForDisplay(const ValueObject& vo)
{
	// XXX: Ditto.
	if(const auto p = vo.AccessPtr<string>())
		return string(*p, p->get_allocator());
	if(const auto p = vo.AccessPtr<TokenValue>())
		return string(*p, p->get_allocator());
	return StringifyBasicObject(vo);
}

//! \since YSLib build 928
YB_ATTR_nodiscard YB_PURE string
StringifyValueObjectForWrite(const ValueObject& vo)
{
	return StringifyValueObjectEscape(vo, StringifyBasicObject);
}

/*!
\ingroup functors
\since YSLib build 852
*/
struct NodeValueLogger
{
	//! \since YSLib build 928
	void
	operator()(std::ostream& os, const TermNode& nd) const
	{
		os << StringifyValueObject(nd.Value);
	}
};


//! \since YSLib build 867
//@{
using ystdex::ceiling_lb;
yconstexpr const auto min_block_size(resource_pool::adjust_for_block(1, 1));
// XXX: This is tuned for %ystdex::resource_pool::allocate.
yconstexpr const size_t init_pool_num(yimpl(19));
//! \since YSLib build 885
static_assert(init_pool_num > 1, "Invalid pool configuration found.");
#if YB_IMPL_GNUCPP >= 30400 || __has_builtin(__builtin_clz)
yconstexpr const auto min_lb_size(sizeof(unsigned) * CHAR_BIT
	- __builtin_clz(unsigned(min_block_size - 1)));
//! \since YSLib build 885
yconstexpr const size_t max_fast_block_shift(min_lb_size + init_pool_num - 1);
yconstexpr const size_t
	max_fast_block_size(1U << max_fast_block_shift);
#else
const auto min_lb_size(ceiling_lb(min_block_size));
//! \since YSLib build 885
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
	const auto ms(resource_pool::adjust_for_block(bytes, alignment));
	const auto lb_size(ceiling_lb(ms));

	if(ms <= max_fast_block_size)
	{
		yverify(lb_size >= min_lb_size);
		return (pools.begin()
			+ pools_t::difference_type(lb_size - min_lb_size))->allocate();
	}
	if(bytes <= saved_options.largest_required_pool_block)
	{
		auto pr(find_pool(lb_size));

		if(!([&] YB_LAMBDA_ANNOTATE((), , pure){
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
	const auto ms(resource_pool::adjust_for_block(bytes, alignment));
	const auto lb_size(ceiling_lb(ms));

	if(ms <= max_fast_block_size)
	{
		yverify(lb_size >= min_lb_size);
		return (pools.begin() + pools_t::difference_type(
			lb_size - min_lb_size))->deallocate(p);
	}
	if(bytes <= saved_options.largest_required_pool_block)
	{
		auto pr(find_pool(lb_size));

		yverify([&] YB_LAMBDA_ANNOTATE((), , pure){
			return pr.first != pools.end();
		}() && pr.first->get_extra_data() == pr.second);
		pr.first->deallocate(p);
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
		pools_t::difference_type(pools.size()), lb_size, []
		YB_LAMBDA_ANNOTATE((const resource_pool& a, size_t lb), ynothrow, pure){
		return a.get_extra_data() < lb;
	}), lb_size};
}


void
DisplayTermValue(std::ostream& os, const TermNode& term)
{
	YSLib::stringstream oss(string(term.get_allocator()));

	PrintTermNode(oss, term, [](std::ostream& os0, const TermNode& nd){
		os0 << StringifyValueObjectForDisplay(nd.Value);
	});
	YSLib::IO::StreamPut(os, oss.str().c_str());
}

void
LogTree(const ValueNode& node, Logger::Level lv)
{
	YSLib::ostringstream oss(string(node.get_allocator()));

	PrintNode(oss, node, [] YB_LAMBDA_ANNOTATE((const ValueNode& nd), , pure){
		return StringifyValueObject(nd.Value);
	});
	yunused(lv);
	YTraceDe(lv, "%s", oss.str().c_str());
}

void
LogTermValue(const TermNode& term, Logger::Level lv)
{
	YSLib::ostringstream oss(string(term.get_allocator()));

	PrintTermNode(oss, term, NodeValueLogger(), true);
	yunused(lv);
	YTraceDe(lv, "%s", oss.str().c_str());
}

void
WriteTermValue(std::ostream& os, const TermNode& term)
{
	YSLib::ostringstream oss(string(term.get_allocator()));

	PrintTermNode(oss, term, [](std::ostream& os0, const TermNode& nd){
		os0 << StringifyValueObjectForWrite(nd.Value);
	});
	YSLib::IO::StreamPut(os, oss.str().c_str());
}

//! \since YSLib build 845
namespace
{

using namespace pmr;

//! \since YSLib build 884
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

//! \since YSLib build 881
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

//! \since YSLib build 905
YB_ATTR_nodiscard memory_resource&
GetUpstreamResourceRef() ynothrowv
{
#if NPLC_Impl_TestMemoryResource
	static test_memory_resource r;

	return r;
#else
	return GetDefaultResourceRef();
#endif
}

#if NPLC_Impl_UsePool
#	define NPLC_Impl_MemoryResourceName pool_rsrc
#elif NPLC_Impl_UseMonotonic
memory_resource&
GetMonotonicPoolRef()
{
	static pmr::monotonic_buffer_resource r;

	return r;
}

#	define NPLC_Impl_MemoryResourceName GetMonotonicPoolRef()
#else
// XXX: Skip %pool_rsrc by using default upstream resource.
#	define NPLC_Impl_MemoryResourceName GetUpstreamResourceRef()
#endif

#if NPLC_Impl_FastAsyncReduce
//! \since YSLib build 945
//@{
//! \since YSLib build 963
YB_ATTR_nodiscard shared_ptr<Environment>
RedirectToShared(shared_ptr<Environment> p_env)
{
#if NPL_NPLA_CheckParentEnvironment
	if(p_env)
#else
	YAssertNonnull(p_env);
#endif
		return p_env;
#if NPL_NPLA_CheckParentEnvironment
	throw InvalidReference(ystdex::sfmt("Invalid reference found for name,"
		" probably due to invalid context access by a dangling reference."));
#endif
}

using Redirector
	= ystdex::unchecked_function<observer_ptr<const ValueObject>()>;

YB_ATTR_nodiscard observer_ptr<const ValueObject>
RedirectEnvironmentList(Environment::allocator_type a, Redirector& cont,
	EnvironmentList::const_iterator first, EnvironmentList::const_iterator last)
{
	if(first != last)
	{
		cont = ystdex::make_obj_using_allocator<Redirector>(a, trivial_swap,
			std::bind(
			[=, &cont](EnvironmentList::const_iterator i, Redirector& c){
			cont = std::move(c);
			return RedirectEnvironmentList(a, cont, i, last);
		}, std::next(first), std::move(cont)));
		return NPL::make_observer(&*first);
	}
	return {};
}
//@}


// XXX: This is essentially same to %ContextNode::DefaultResolve.
//! \since YSLib build 962
//@{
//! \since YSLib build 963
YB_ATTR_nodiscard bool
ResolveRedirect(shared_ptr<Environment>& p_env, Redirector& cont)
{
	observer_ptr<const ValueObject> p_next(&p_env->Parent);
	shared_ptr<Environment> p_redirected{};

	do
	{
		auto& parent(*p_next);
		const auto& ti(parent.type());

		p_next = {};
		if(IsTyped<EnvironmentReference>(ti))
		{
			p_redirected = RedirectToShared(
				parent.GetObject<EnvironmentReference>().Lock());
			p_env.swap(p_redirected);
		}
		else if(IsTyped<shared_ptr<Environment>>(ti))
		{
			p_redirected
				= RedirectToShared(parent.GetObject<shared_ptr<Environment>>());
			p_env.swap(p_redirected);
		}
		else
		{
			if(IsTyped<EnvironmentList>(ti))
			{
				const auto& envs(parent.GetObject<EnvironmentList>());

				p_next = RedirectEnvironmentList(NPL::ToBindingsAllocator(
					*p_env), cont, envs.cbegin(), envs.cend());
			}
			while(!p_next && bool(cont))
				p_next = ystdex::exchange(cont, Redirector())();
			YAssert(p_next.get() != &parent, "Cyclic parent found.");
		}
	}while(p_next);
	return bool(p_redirected);
}

YB_ATTR_nodiscard NameResolution::first_type
ResolveDefault(shared_ptr<Environment>& p_env, string_view id)
{
	YAssertNonnull(p_env);

	Redirector cont;

	return ystdex::retry_on_cond(
#if YB_IMPL_GNUCPP >= 120000
		[&](NameResolution::first_type p)
#else
		[&] YB_LAMBDA_ANNOTATE((NameResolution::first_type p), , flatten)
#endif
	{
		return !(p || !ResolveRedirect(p_env, cont));
	}, [&, id]{
		return LookupName(p_env->GetMapUncheckedRef(), id);
	});
}
//@}

// XXX: There are several fast asynchrnous reduction assumptions from NPLA1.
//	1. For the leaf values other than value tokens, the next term is not relied
//		on.
//	2. There are no changes on passes and handlers like %ContextNode::Resolve
//		and %ContextState::ReduceOnce::Handler after the initialization.
//	3. No literal handlers rely on the value of the reduced term or access the
//		context asnynchrnously.
//	4. The 1st subterm of the combining term is not relied on in the context
//		handler calls.
//! \since YSLib build 945
//@{
YB_FLATTEN void
EvaluateFastNonListCore(TermNode& term, const shared_ptr<Environment>& p_env,
	TermNode& bound, ResolvedTermReferencePtr p_ref)
{
	const auto a(term.get_allocator());

	// XXX: Some uses of allocator are different to YSLib for performance.
	if(p_ref)
		term.SetContent(bound.GetContainer(), ValueObject(std::allocator_arg, a,
			in_place_type<TermReference>, p_ref->GetTags() & ~TermTags::Unique,
			*p_ref));
	else
		term.SetValue(a, in_place_type<TermReference>,
			NPL::Deref(p_env).MakeTermTags(bound) & ~TermTags::Unique, bound,
			EnvironmentReference(p_env, NPL::Deref(p_env).GetAnchorPtr()));
}

template<typename _func, typename _func2, typename _fReducePair>
// XXX: This is less efficient at least on x86_64-pc-linux G++ 12.1.
#if YB_IMPL_GNUCPP < 120000
YB_FLATTEN
#endif
inline ReductionStatus
ReduceFastTmpl(TermNode& term, ContextState& cs, _func f, _func2 f2,
	_fReducePair reduce_pair)
{
	if(IsCombiningTerm(term))
		return reduce_pair(term, cs);
	// XXX: There are several administrative differences to the original
	//	%ContextState::DefaultReduceOnce. For leaf values other than value
	//	tokens, the next term is set up in the original implemenation but
	//	omitted here, cf. assumption 1.
	if(const auto p = TermToNamePtr(term))
	{
		string_view id(*p);

		YAssert(IsLeaf(term),
			"Unexpected irregular representation of term found.");
		// XXX: Assume the call does not rely on %term and it does not change
		//	the stored name on throwing.
		// XXX: See assumption 2. This is also known not throwing
		//	%BadIdentifier.
		auto p_env(cs.GetRecordPtr());

		if(const auto p_term = ResolveDefault(p_env, id))
			return f(*p_term, p_env);
#	if NPLC_Impl_UseSourceInfo
		{
			BadIdentifier e(id);

			// XXX: Inlining expansion of the call to
			//	%A1::QuerySourceInformation here is likely less efficient.
			if(const auto p_si = A1::QuerySourceInformation(term.Value))
				e.Source = *p_si;
			throw e;
		}
#	else
		throw BadIdentifier(id);
#	endif
	}
#if true
	// XXX: This is faster.
	return f2();
#else
	return !IsTyped<A1::ValueToken>(term) ? f2() : ReductionStatus::Retained;
#endif
}

template<typename _fReducePair>
YB_FLATTEN inline ReductionStatus
ReduceFastSimple(TermNode& term, ContextState& cs, _fReducePair reduce_pair)
{
	return ReduceFastTmpl(term, cs,
		[&](TermNode& bound, const shared_ptr<Environment>& p_env){
		return ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref)
		// XXX: Ditto.
#if YB_IMPL_GNUCPP < 120000
			YB_FLATTEN
#endif
		{
			EvaluateFastNonListCore(term, p_env, bound, p_ref);
			// XXX: This is safe, cf. assumption 3.
			A1::EvaluateLiteralHandler(term, cs, nd);
			return ReductionStatus::Neutral;
		}, bound);
	}, [] YB_LAMBDA_ANNOTATE((), ynothrow, const){
		return ReductionStatus::Retained;
	}, reduce_pair);
}
//@}

//! \since YSLib build 883
//@{
ReductionStatus
ReduceFastBranch(TermNode&, ContextState&);

ReductionStatus
ReduceFastBranchNotNested(TermNode& term, ContextState& cs)
{
	YAssert(IsCombiningTerm(term), "Invalid term found.");
	YAssert(!(IsList(term) && term.size() == 1), "Invalid term found.");
	return [&] YB_LAMBDA_ANNOTATE((), , flatten){
		YAssert(term.size() > (IsList(term) ? 1 : 0), "Invalid term found.");
		// XXX: These passes are known safe to synchronize.
		if(IsEmpty(AccessFirstSubterm(term)))
			RemoveHead(term);
		YAssert(IsBranch(term), "Invalid term found.");
		cs.LastStatus = ReductionStatus::Neutral;

		auto& sub(AccessFirstSubterm(term));

		return ReduceFastTmpl(sub, cs,
			[&](TermNode& bound, const shared_ptr<Environment>&){
			return ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr){
				// XXX: Missing setting of the 1st subterm is safe, cf.
				//	assumption 4.
				// XXX: This is safe, cf. assumption 3.
				A1::EvaluateLiteralHandler(sub, cs, nd);
				return
					ReduceCombinedReferentWithOperator(term, cs, nd, sub.Value);
			}, bound);
		}, [&]{
			return ReduceCombinedBranch(term, cs);
		}, [&](TermNode&, ContextNode& ctx){
			// XXX: %trivial_swap is not used here to avoid worse inlining.
			RelaySwitched(ctx, A1::NameTypedReducerHandler([&](ContextNode& c)
#if YB_IMPL_GNUCPP >= 120000
				YB_ATTR(noinline)
#endif
			{
				return A1::ReduceCombinedBranch(term, c);
			}, "eval-combine-operands"));
			return RelaySwitched(ctx, trivial_swap, [&](ContextNode& c)
#if YB_IMPL_GNUCPP >= 120000
				YB_ATTR(noinline)
#endif
			{
				return ReduceFastBranch(sub, ContextState::Access(c));
			});
		});
	}();
}

ReductionStatus
ReduceFastBranch(TermNode& term, ContextState& cs)
{
	YAssert(IsCombiningTerm(term), "Invalid term found.");
	if(IsSingleElementList(term))
	{
		auto term_ref(ystdex::ref(term));

		ystdex::retry_on_cond([&] YB_LAMBDA_ANNOTATE((), , pure){
			return IsSingleElementList(term_ref);
		}, [&]{
			term_ref = AccessFirstSubterm(term_ref);
		});
		LiftOtherValue(term, term_ref);
		return ReduceFastSimple(term, cs, ReduceFastBranchNotNested);
	}
	return ReduceFastBranchNotNested(term, cs);
}
//@}
#endif

//! \since YSLib build 955
template<typename _func>
inline void
RewriteBy(ContextState& cs, _func f)
{
	cs.Rewrite(NPL::ToReducer(cs.get_allocator(), trivial_swap, std::move(f)));
}

} // unnamed namespace;

Interpreter::Interpreter()
	: terminal(), terminal_err(stderr), pool_rsrc(&GetUpstreamResourceRef()),
	line(), Global([this](const A1::GParsedValue<ByteParser>& str){
		TermNode term(Global.Allocator);
		const auto id(YSLib::make_string_view(str));

#if NPLC_Impl_UseCheckedExtendedLiteral
		if(A1::HandleCheckedExtendedLiteral(term, id))
#endif
			A1::ParseLeaf(term, id);
		return term;
	}, [this](const A1::GParsedValue<SourcedByteParser>& val,
		const ContextState& cs){
		TermNode term(Global.Allocator);
		const auto id(YSLib::make_string_view(val.second));

#if NPLC_Impl_UseCheckedExtendedLiteral
		if(A1::HandleCheckedExtendedLiteral(term, id))
#endif
			A1::ParseLeafWithSourceInformation(term, id, cs.CurrentSource,
				val.first);
		return term;
	}, NPLC_Impl_MemoryResourceName)
{
	using namespace std;

#if NPLC_Impl_UseSourceInfo
	Global.UseSourceLocation = true;
#endif
#if NPLC_Impl_TracePerformDetails
	SetupTraceDepth(Main);
#endif
	// TODO: Avoid reassignment of default passes?
#if NPLC_Impl_FastAsyncReduce
	if(Global.IsAsynchronous())
		// XXX: Only safe and meaningful for asynchrnous implementations.
		Main.ReduceOnce.Handler = []
			YB_LAMBDA_ANNOTATE((TermNode& term, ContextNode& ctx), , flatten){
			return ReduceFastSimple(term, ContextState::Access(ctx),
				ReduceFastBranch);
		};
#elif NPLC_Impl_LogBeforeReduce
	using namespace placeholders;
	A1::EvaluationPasses passes([](TermNode& term){
		YTraceDe(Notice, "Before ReduceCombined:");
		LogTermValue(term, Notice);
		return ReductionStatus::Retained;
	});

	passes += ReduceHeadEmptyList;
	passes += A1::ReduceFirst;
	passes += A1::ReduceCombined;
	Global.EvaluateList = std::move(passes);
#endif
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

void
Interpreter::HandleREPLException(std::exception_ptr p_exc, ContextNode& ctx)
{
	YAssertNonnull(p_exc);
	TryExpr(std::rethrow_exception(std::move(p_exc)))
	catch(SSignal e)
	{
		if(e == SSignal::Exit)
			// XXX: Assume the current context is %Main.
			Main.UnwindCurrent();
		else
		{
			UpdateTextColor(SignalColor);
			HandleSignal(e);
		}
	}
	catch(std::exception& e)
	{
		using namespace YSLib;
		const auto gd(ystdex::make_guard([&]() ynothrowv{
			Backtrace.clear();
		}));
		auto& trace(ctx.Trace);

		UpdateTextColor(ErrorColor, true);
		TraceException(e, trace);
		trace.TraceFormat(Notice, "Location: %s.", Main.CurrentSource
			? Main.CurrentSource->c_str() : "<unknown>");
#if NPLC_Impl_UseBacktrace
		A1::TraceBacktrace(Backtrace, trace);
#endif
	}
}

ReductionStatus
Interpreter::ExecuteOnce(ContextNode& ctx)
{
	Global.Preprocess(Term);
	UpdateTextColor(SideEffectColor),
	UpdateTextColor(SideEffectColor, true);
	return A1::ReduceOnce(Term, ctx);
}

ReductionStatus
Interpreter::ExecuteString(string_view unit, ContextNode& ctx)
{
	PrepareExecution(ctx);
	Term = Global.ReadFrom(unit, Main);
	return ExecuteOnce(ctx);
}

void
Interpreter::PrepareExecution(ContextNode& ctx)
{
	ctx.SaveExceptionHandler();
	// TODO: Blocked. Use C++14 lambda initializers to simplify the
	//	implementation.
	ctx.HandleException = ystdex::bind1([&](std::exception_ptr p,
		const ContextNode::ReducerSequence::const_iterator& i){
		ctx.TailAction = nullptr;
		ctx.Shift(Backtrace, i);
		HandleREPLException(std::move(p), ctx);
	}, ctx.GetCurrent().cbegin());
	RelaySwitched(ctx, trivial_swap, A1::NameTypedReducerHandler([&]{
	//	UpdateTextColor(InfoColor, true);
	//	clog << "Unrecognized reduced token list:" << endl;
		UpdateTextColor(ReducedColor, true);
		LogTermValue(Term);
		return ReductionStatus::Neutral;
	}, "repl-print"));
}

void
Interpreter::Run()
{
	// XXX: Use %std::bind here can be a slightly more efficient.
	RewriteBy(Main,
		std::bind(&Interpreter::RunLoop, this, std::placeholders::_1));
}

YB_FLATTEN void
Interpreter::RunScript(string filename)
{
	if(filename == "-")
	{
		Main.ShareCurrentSource("*STDIN*");
		RewriteBy(Main, [&](ContextNode& ctx){
			PrepareExecution(ctx);
			Term = Global.ReadFrom(std::cin, Main);
			return ExecuteOnce(ctx);
		});
	}
	else if(!filename.empty())
	{
		Main.ShareCurrentSource(filename);
		RewriteBy(Main, [&](ContextNode& ctx){
			PrepareExecution(ctx);
			// NOTE: As %A1::ReduceToLoadExternal.
			Term = Global.Load(Main, std::move(filename));
			return ExecuteOnce(ctx);
		});
	}
}

void
Interpreter::RunLine(string_view unit)
{
	if(!unit.empty())
	{
		Main.ShareCurrentSource("*STDIN*");
		// XXX: Use %std::bind here can be a slightly less efficient.
		RewriteBy(Main, [&](ContextNode& ctx){
			return ExecuteString(unit, ctx);
		});
	}
}

ReductionStatus
Interpreter::RunLoop(ContextNode& ctx)
{
	// TODO: Set error continuation to filter exceptions.
	if(WaitForLine())
	{
		Main.ShareCurrentSource("*STDIN*");
		RelaySwitched(ctx, trivial_swap,
			std::bind(&Interpreter::RunLoop, this, std::placeholders::_1));
		return
			!line.empty() ? ExecuteString(line, ctx) : ReductionStatus::Partial;
	}
	return ReductionStatus::Retained;
	// TODO: Add root continuation?
}

bool
Interpreter::SaveGround()
{
	if(!p_ground)
	{
		p_ground = NPL::SwitchToFreshEnvironment(Main,
			ValueObject(Main.WeakenRecord()));
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
	UpdateTextColor(DefaultColor),
	UpdateTextColor(DefaultColor, true);
	YSLib::IO::StreamGet(is, line);
	return is;
}

} // namespace NBuilder;

