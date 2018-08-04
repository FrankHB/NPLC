﻿/*
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
\version 415
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2018-08-04 20:12 +0800
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

using namespace YSLib;

#define NPL_TracePerform 1
#define NPL_TracePerformDetails 0

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

Interpreter::Interpreter(Application& app,
	std::function<void(REPLContext&)> loader)
	: terminal(), err_threshold(RecordLevel(0x10)), line(),
#if NPL_TracePerformDetails
	context(true)
#else
	context()
#endif
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

		p_ground = ctx.SwitchEnvironmentUnchecked(
			make_shared<Environment>(ValueObject(ctx.WeakenRecord())));
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

