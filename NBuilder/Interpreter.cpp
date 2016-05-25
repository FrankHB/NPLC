/*
	© 2013-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.cpp
\ingroup NBuilder
\brief NPL 解释器。
\version r271
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2016-05-26 02:36 +0800
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

/// 691
YB_NONNULL(3) void
PrintError(WConsole& wc, const LoggedEvent& e, const char* name = "Error")
{
	wc.UpdateForeColor(ErrorColor);
	std::cerr << name << "[" << typeid(e).name() << "]" << "<"
		<< unsigned(e.GetLevel()) << ">: " << e.what() << std::endl;
}

} // unnamed namespace;


void
LogTree(const ValueNode& node, Logger::Level lv)
{
	std::ostringstream oss;

	PrintNode(oss, node, [](const ValueNode& node) -> string{
		return EscapeLiteral([&]() -> string{
			if(const auto p = AccessPtr<string>(node))
				return *p;

			const auto& t(node.Value.GetType());

			if(t != typeid(void))
				return ystdex::quote(string(t.name()), '[', ']');
			throw ystdex::bad_any_cast();
		}());
	});
	YTraceDe(lv, "%s", oss.str().c_str());
}


Interpreter::Interpreter(Application& app,
	std::function<void(NPLContext&)> loader)
	: wc(), err_threshold(RecordLevel(0x10)), line(), context()
{
	using namespace std;
	using namespace platform_ex;

	wc.UpdateForeColor(TitleColor);
	cout << title << endl << "Initializing...";
	p_env.reset(new Environment(app));
	loader(context);
	cout << "NPLC initialization OK!" << endl << endl;
	wc.UpdateForeColor(InfoColor);
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
		wc.Clear();
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

	if(line.empty())
		return true;
	wc.UpdateForeColor(SideEffectColor);
	try
	{
		const auto res(context.Perform(line));

#if NPL_TracePerform
	//	wc.UpdateForeColor(InfoColor);
	//	cout << "Unrecognized reduced token list:" << endl;
		wc.UpdateForeColor(ReducedColor);
		LogTree(res);
#endif
	}
	catch(SSignal e)
	{
		if(e == SSignal::Exit)
			return false;
		wc.UpdateForeColor(SignalColor);
		HandleSignal(e);
	}
	CatchExpr(NPLException& e, PrintError(wc, e, "NPLException"))
	catch(LoggedEvent& e)
	{
		const auto l(e.GetLevel());

		if(l < err_threshold)
			throw;
		PrintError(wc, e);
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
	wc.UpdateForeColor(PromptColor);
	os << prompt;
	wc.UpdateForeColor(DefaultColor);
	return std::getline(is, line);
}

} // namespace NPL;

