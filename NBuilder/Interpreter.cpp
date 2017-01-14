/*
	© 2013-2017 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.cpp
\ingroup NBuilder
\brief NPL 解释器。
\version r337
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2017-01-14 23:47 +0800
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

} // unnamed namespace;


void
LogTree(const ValueNode& node, Logger::Level lv)
{
	std::ostringstream oss;

	PrintNode(oss, node, [](const ValueNode& nd){
		return EscapeLiteral([&]() -> string{
			if(const auto p = AccessPtr<string>(nd))
				return *p;
			if(const auto p = AccessPtr<bool>(nd))
				return *p ? "[bool] #t" : "[bool] #f";

			const auto& v(nd.Value);
			const auto& t(v.GetType());

			if(t != ystdex::type_id<void>())
				return ystdex::quote(string(t.name()), '[', ']');
			throw ystdex::bad_any_cast();
		}());
	});
	YTraceDe(lv, "%s", oss.str().c_str());
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
	p_env.reset(new Environment(app));
	loader(context);
	cout << "NPLC initialization OK!" << endl << endl;
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

			const auto res(context.Perform(line));

#if NPL_TracePerform
		//	terminal.UpdateForeColor(InfoColor);
		//	cout << "Unrecognized reduced token list:" << endl;
			terminal.UpdateForeColor(ReducedColor);
			LogTree(res);
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

