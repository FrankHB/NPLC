/*
	© 2013-2018 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.cpp
\ingroup NBuilder
\brief NPL 解释器。
\version 511
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2018-11-26 04:59 +0800
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

#define NPL_TracePerform true
#define NPL_TracePerformDetails false
#define NPL_Impl_UseDebugMR false
#define NPL_Impl_UseMonotonic false
#define NPL_Impl_TestMemoryResource false

namespace NPL
{

#define NPL_NAME "NPL console"
#define NPL_VER "b30xx"
#define NPL_PLATFORM "[MinGW32]"
yconstexpr auto prompt("> ");
yconstexpr auto title(NPL_NAME" " NPL_VER" @ (" __DATE__", " __TIME__") "
	NPL_PLATFORM);

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

/// 803
void
PrintTermNode(std::ostream& os, const ValueNode& node, NodeToString node_to_str,
	IndentGenerator igen = DefaultGenerateIndent, size_t depth = 0)
{
	PrintIndent(os, igen, depth);
	os << EscapeLiteral(node.GetName()) << ' ';

	const auto print_node_str(
		[&](const ValueNode& nd) -> pair<lref<const ValueNode>, bool>{
		const TermNode& term(nd);
		const auto& tm(ReferenceTerm(term));
		const ValueNode& vnode(tm);

		if(&tm != &term)
			os << '*';
		return {vnode, PrintNodeString(os, vnode, node_to_str)};
	});
	const auto pr(print_node_str(node));

	if(!pr.second)
	{
		const auto& vnode(pr.first.get());

		os << '\n';
		if(vnode)
			TraverseNodeChildAndPrint(os, vnode, [&]{
				PrintIndent(os, igen, depth);
			}, print_node_str, [&](const ValueNode& nd){
				return PrintTermNode(os, nd, node_to_str, igen, depth + 1);
			});
	}
}

} // unnamed namespace;


void
LogTree(const ValueNode& node, Logger::Level lv)
{
	std::ostringstream oss;

	PrintTermNode(oss, node, [](const ValueNode& nd){
		return EscapeLiteral([&]() -> string{
			if(nd.Value != A1::ValueToken::Null)
			{
				if(const auto p = AccessPtr<string>(nd))
					return *p;
				if(const auto p = AccessPtr<TokenValue>(nd))
					return sfmt("[TokenValue] %s", p->c_str());
				if(const auto p = AccessPtr<A1::ValueToken>(nd))
					return sfmt("[ValueToken] %s", to_string(*p).c_str());
				if(const auto p = AccessPtr<bool>(nd))
					return *p ? "[bool] #t" : "[bool] #f";
				if(const auto p = AccessPtr<int>(nd))
					return sfmt("[int] %d", *p);
				if(const auto p = AccessPtr<unsigned>(nd))
					return sfmt("[uint] %u", *p);
				if(const auto p = AccessPtr<double>(nd))
					return sfmt("[double] %lf", *p);

				const auto& v(nd.Value);
				const auto& t(v.type());

				if(t != ystdex::type_id<void>())
					return ystdex::quote(string(t.name()), '[', ']');
			}
			throw ystdex::bad_any_cast();
		}());
	});
	YTraceDe(lv, "%s", oss.str().c_str());
}

void
LogTermValue(const TermNode& term, Logger::Level lv)
{
	LogTree(term, lv);
}

/// 845
namespace
{

using namespace pmr;

#if NPL_Impl_TestMemoryResource
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
#if NPL_Impl_UseDebugMR
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
#if NPL_Impl_UseDebugMR
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
#if NPL_Impl_TestMemoryResource
	static test_memory_resource r;

	return r;
#else
	return Deref(YSLib::pmr::new_delete_resource());
#endif
}

#if NPL_Impl_UseMonotonic
memory_resource&
GetMonotonicPoolRef()
{
	static YSLib::pmr::monotonic_buffer_resource r;

	return r;
}

#	define NPL_Impl_PoolName GetMonotonicPoolRef()
#else
#	define NPL_Impl_PoolName pool_rsrc
#endif

} // unnamed namespace;

Interpreter::Interpreter(Application& app,
	std::function<void(REPLContext&)> loader)
	: terminal(), err_threshold(RecordLevel(0x10)),
	pool_rsrc(&GetPoolResourceRef()), line(),
	context(NPL_TracePerformDetails, NPL_Impl_PoolName)
{
	using namespace std;
	using namespace platform_ex;

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

#if NPL_TracePerform
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

