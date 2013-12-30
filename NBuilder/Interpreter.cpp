/*
	© 2013 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.cpp
\ingroup NBuilder
\brief NPL 解释器。
\version r161
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2013-12-27 10:39 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Interpreter
*/


#include "Interpreter.h"
#include <Helper/YModules.h>
#include <iostream>
#include YFM_YCLib_YCommon
#include YFM_Helper_Initialization

using namespace NPL;
using YSLib::LoggedEvent;

namespace NPL
{

/// 403
list<string> GlobalPath;


#define NPL_NAME "NPL console"
#define NPL_VER "b30xx"
#define NPL_PLATFORM "[MinGW32]"
yconstexpr auto prompt("> ");
yconstexpr auto title(NPL_NAME" " NPL_VER" @ (" __DATE__", " __TIME__") "
	NPL_PLATFORM);


Interpreter::Interpreter(std::function<void(NPLContext&)> loader)
	: wc(), err_threshold(RecordLevel(0x10)), line(), context()
{
	using namespace std;
	using namespace Consoles;

	wc.SetColor(TitleColor);
	cout << title << endl << "Initializing...";
	YSLib::InitializeInstalled();
	loader(context);
	cout << "NPLC initialization OK!" << endl << endl;
	wc.SetColor(InfoColor);
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
	using namespace Consoles;

	if(line.empty())
		return true;
	wc.SetColor(SideEffectColor);
	try
	{
		auto& unreduced(context.Perform(line));

		if(!unreduced.empty())
		{
			using namespace std;
			using namespace Consoles;

		//	wc.PrintError("Bad command.");
		//	wc.SetColor(InfoColor);
		//	cout << "Unrecognized reduced token list:" << endl;
			wc.SetColor(ReducedColor);
			for(const auto& token : unreduced)
			{
			//	cout << token << endl;
				cout << token << ' ';
			}
			cout << endl;
		}
	}
	catch(SSignal e)
	{
		if(e == SSignal::Exit)
			return false;
		wc.SetColor(SignalColor);
		HandleSignal(e);
	}
	catch(LoggedEvent& e)
	{
		const auto l(e.GetLevel());

		if(l < err_threshold)
			throw;
		wc.PrintError(e);
	}
	return true;
}

std::istream&
Interpreter::WaitForLine()
{
	using namespace std;
	using namespace Consoles;

	wc.SetColor(PromptColor);
	for(const auto& n : GlobalPath)
		cout << n << ' ';
	cout << prompt;
	wc.SetColor();
	return getline(cin, line);
}

} // namespace NPL;

