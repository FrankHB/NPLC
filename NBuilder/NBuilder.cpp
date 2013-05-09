/*
	Copyright (C) by Franksoft 2011 - 2012.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r3724;
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301 。
\par 创建时间:
	2011-07-02 07:26:21 +0800;
\par 修改时间:
	2012-09-30 11:24 +0800;
\par 文本编码:
	UTF-8;
\par 模块名称:
	NBuilder::NBuilder;
*/


#include <streambuf>
#include <sstream>
#include <iostream>
#include <fstream>
#include "NBuilder.h"
#include "NPL/Configuration.h"
#include <Helper/Initialization.h>
#include <YCLib/debug.h>

using namespace NPL;
using namespace YSLib;
using platform_ex::UTF8ToMBCS;

ValueNode GlobalRoot;

string
SToMBCS(String s, int cp)
{
	return platform_ex::WCSToMBCS(reinterpret_cast<const wchar_t*>(s.c_str()),
		s.length(), cp);
}

namespace
{

void
Traverse(const ValueNode& node)
{
	using namespace std;

	try
	{
		const auto& s(Access<string>(node));

		cout << s << ' ';
		return;
	}
	catch(ystdex::bad_any_cast&)
	{}
	cout << "( ";
	try
	{
		for(const auto& n : node)
			Traverse(n);
	}
	catch(ystdex::bad_any_cast&)
	{}
	cout << ") ";
}

void
PrintNode(const ValueNode& node)
{
	std::cout << ">Root size: " << node.GetSize() << std::endl;
	Traverse(node);
	std::cout << std::endl;
}

void
TraverseN(const ValueNode& node)
{
	using namespace std;

	cout << '[' << node.GetName() << ']';
	try
	{
		const auto& s(Access<string>(node));

		cout << s << ' ';
		return;
	}
	catch(ystdex::bad_any_cast&)
	{}
	if(node.GetSize() != 0)
	{
		cout << "( ";
		try
		{
			for(const auto& n : node)
				TraverseN(n);
		}
		catch(ystdex::bad_any_cast&)
		{}
		cout << ") ";
	}
}

void
PrintNodeN(const ValueNode& node)
{
	std::cout << ">>>Root size: " << node.GetSize() << std::endl;
	TraverseN(node);
	std::cout << std::endl;
}

} // unnamed namespace;

YSL_BEGIN_NAMESPACE(NPL)

void
EvalS(const ValueNode& root)
{
	PrintNodeN(root);
	try
	{
		PrintNodeN(TransformConfiguration(root));
	}
	catch(ystdex::bad_any_cast&)
	{
		throw LoggedEvent("Bad configuration found.", 0x80);
	}
}

YSL_END_NAMESPACE(NPL)


namespace
{

///334
list<string> g_path;

///403
const ValueNode&
GetNodeFromPath(const ValueNode& root, list<string> path)
{
	auto p(&root);

	for(const auto& n : path)
		p = &p->GetNode(n);
	return *p;
}

///403
const ValueNode&
GetCurrentNode()
{
	return GetNodeFromPath(GlobalRoot, g_path);
}

/// 327
void ParseOutput(LexicalAnalyzer& lex)
{
	const auto& cbuf(lex.GetBuffer());

	using namespace std;

#if 0
	cout << cbuf << endl;
#endif
	cout << "cbuf size:" << cbuf.size() << endl;

	const auto xlst(lex.Literalize());
#if 0
	for(const auto& str : xlst)
		cout << str << endl << "--" << endl;
#endif
	cout << "xlst size:" << cbuf.size() << endl;

	const auto rlst(Tokenize(xlst));

	for(const auto& str : rlst)
	{
	//	cout << str << endl;
	//	cout << String(str).GetMBCS(Text::CharSet::GBK) << endl;
		cout << SToMBCS(str, 936) << endl;
	//	cout << UTF8ToGBK(str) << endl;
		cout << "~----: u8 length: " << str.size() << endl;
	}
	cout << rlst.size() << " token(s) parsed." <<endl;
}

/// 330
void
ParseFileImpl(LexicalAnalyzer& lex, const string& path_gbk)
{
	const string& path(String(path_gbk, CHRLib::CharSet::GBK).GetMBCS());
	TextFile tf(path.c_str());

	if(!tf)
		throw LoggedEvent("Invalid file: \"" + path_gbk + "\".", 0x80);
	{
		ystdex::ifile_iterator i(*tf.GetPtr());

		while(!tf.CheckEOF())
		{
			if(YB_UNLIKELY(is_undereferenceable(i)))
				throw LoggedEvent("Bad Source!", 0x40);
			lex.ParseByte(*i);
			++i;
		}
	}
}

/// 304
void
ParseFile(const string& path_gbk)
{
	LexicalAnalyzer lex;

	ParseFileImpl(lex, path_gbk);
	std::cout << ystdex::sfmt("Parse from file: %s : ", path_gbk.c_str())
		<< std::endl;
	ParseOutput(lex);
}

/// 334
void
PrintFile(const string& path_gbk)
{
	const string& path(String(path_gbk, CHRLib::CharSet::GBK).GetMBCS());
	size_t bom;
	{
		TextFile tf(path);

		bom = tf.GetBOMSize();
	}

	std::ifstream fin(path.c_str());
	string str;

	while(bom--)
		fin.get();
	while(fin)
	{
		std::getline(fin, str);
		std::cout << SToMBCS(str, 936) << '\n';
	}
	std::cout << std::endl;
}

/// 304
void
ParseString(const std::string& str)
{
	LexicalAnalyzer lex;

	for(const auto& c : str)
		lex.ParseByte(c);
	std::cout << ystdex::sfmt("Parse from str: %s : ", str.c_str())
		<< std::endl;
	ParseOutput(lex);
}

/// 307
void
SearchName(ValueNode& root, const string& arg)
{
	using namespace std;

	try
	{
		const auto& name(arg);

		cout << "Searching... " << name << endl;

		auto& node(root.GetNode(name));

		cout << "Found: " << name << " = ";

		auto& s(Access<string>(node));
		cout << s << endl;
	}
	catch(out_of_range&)
	{
		cout << "Not found." << endl;
	}
	catch(ystdex::bad_any_cast&)
	{
		cout << "[Type error]" << endl;
	}
}

} // unnamed namespace;


/// 304
namespace Consoles
{

WConsole::WConsole()
	: hConsole(::GetStdHandle(STD_OUTPUT_HANDLE))
{
	YAssert(hConsole && hConsole != INVALID_HANDLE_VALUE,
		"Wrong handle found;");
}
WConsole::~WConsole()
{
	SetColor();
}

void
WConsole::SetSystemColor(std::uint8_t color)
{
	char cmd[9];

	std::sprintf(cmd, "COLOR %02x", int(color));
	std::system(cmd);
}

void
WConsole::PrintError(const char* s)
{
	SetColor(ErrorColor);
	std::cerr << s << std::endl;
}
void
WConsole::PrintError(const LoggedEvent& e)
{
	SetColor(ErrorColor);
	std::cerr << "Error<" << unsigned(e.GetLevel()) << ">: "
		<< e.what() << std::endl;
}

} // namespace Consoles;


#define NPL_NAME "NPL console"
#define NPL_VER "b30xx"
#define NPL_PLATFORM "[MinGW32]"
yconstexpr auto prompt("> ");
yconstexpr auto title(NPL_NAME" " NPL_VER" @ (" __DATE__", " __TIME__") "
	NPL_PLATFORM);


/// 328
void
LoadFunctions(NPLContext& context)
{
//	LoadFunctions(root);
//	root.Add(ValueNode("parse", string("parse")));
	context.Insert("parse", ParseFile);
	context.Insert("print", PrintFile);
	context.Insert("cd", [](const string& arg){
		if(arg == "..")
		{
			if(g_path.empty())
				throw LoggedEvent("No parent node found.", 0x80);
			g_path.pop_back();
		}
		else
		{
			auto& node(GetCurrentNode());

			try
			{
				node.GetNode(arg);
			}
			catch(std::out_of_range&)
			{
				throw LoggedEvent("No node found.", 0x80);
			}
			g_path.push_back(arg);
		}
	});
	context.Insert("add", [](const string& arg){
		if(arg == "..")
			throw LoggedEvent("Invalid node name found.", 0x80);
		GetCurrentNode() += {0, arg};
	});
	context.Insert("set", [](const string& arg){
		auto& node(GetCurrentNode());

		if(!node)
			node.Value = arg;
		else
			try
			{
				Access<string>(node) = arg;
			}
			catch(ystdex::bad_any_cast&)
			{
				throw LoggedEvent("Wrong type found.", 0x80);
			}
	});
	context.Insert("get", [](const string&){
		auto& node(GetCurrentNode());

		try
		{
			std::cout << Access<string>(node) << std::endl;
		}
		catch(ystdex::bad_any_cast&)
		{
			throw LoggedEvent("Wrong type found.", 0x80);
		}
	});
	context.Insert("mangle", [](const string& arg){
		if(CheckLiteral(arg) == '"')
			ParseString(ystdex::get_mid(arg));
		else
			std::cout << "Please use a string literal as argument."
				<< std::endl;
	});
	context.Insert("system", [](const string& arg){
		std::system(arg.c_str());
	});
	context.Insert("echo", [](const string& arg){
		if(CheckLiteral(arg) != 0)
			std::cout << ystdex::get_mid(arg) << std::endl;
	});
	context.Insert("search", [&](const string& arg){
		SearchName(context.Root, arg);
	});
	context.Insert("eval",
		std::bind(&NPLContext::Eval, &context, std::placeholders::_1));
	context.Insert("evals", [](const string& arg){
		if(CheckLiteral(arg) == '\'')
			EvalS(ystdex::get_mid(arg));
	});
	context.Insert("evalf", [](const string& path_gbk){
		if(TextFile tf{String(path_gbk, CHRLib::CharSet::GBK).GetMBCS()})
		{
			Configuration conf;
			File f("out.txt", "w");

			try
			{
				tf >> conf;
				f << conf;
			}
			catch(ystdex::bad_any_cast& e)
			{
				// TODO: Avoid memory allocation.
				throw LoggedEvent(ystdex::sfmt(
					">Bad configuration found: cast failed from [%s] to [%s] .",
					e.from(), e.to()), 0x80);
			}
		}
		else
			throw LoggedEvent("Invalid file: \"" + path_gbk + "\".", 0x80);
	});
	context.Insert("$+", [&](const string& arg){
		context.sem = "$__+" + arg;
	});
}


/// 304
class Interpreter
{
private:
	Consoles::WConsole wc;
	LoggedEvent::LevelType err_threshold;
	string line;
	NPLContext context;

public:
	Interpreter()
		: wc(), err_threshold(0x10), line(), context()
	{
		using namespace std;
		using namespace Consoles;

		wc.SetColor(TitleColor);
		cout << title << endl << "Initializing...";
		YSLib::InitializeInstalled();
		LoadFunctions(context);
		cout << "NPLC initialization OK!" << endl << endl;
		wc.SetColor(InfoColor);
		cout << "Type \"exit\" to exit,"
			" \"cls\" to clear screen, \"help\", \"about\", or \"license\""
			" for more information." << endl << endl;
	}

	void
	HandleSignal(SSignal e)
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
	Process()
	{
		using namespace Consoles;

		if(line.empty())
			return true;
		wc.SetColor(SideEffectColor);
		try
		{
			auto& unreduced(context.Translate(line));

			if(!unreduced.empty())
			{
				using namespace std;
				using namespace Consoles;

			//	wc.PrintError("Bad command.");
			//	wc.SetColor(InfoColor);
			//	cout << "Unrecognized reduced token list:" << endl;
				wc.SetColor(ReducedColor);
				for_each(unreduced.cbegin(), unreduced.cend(),
					[](const string& token){
				//	cout << token << endl;
					cout << token << ' ';
				});
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
			auto l(e.GetLevel());

			if(l < err_threshold)
				throw;
			wc.PrintError(e);
		}
		return true;
	}

	std::istream&
	WaitForLine()
	{
		using namespace std;
		using namespace Consoles;

		wc.SetColor(PromptColor);
		for(const auto& n : g_path)
			cout << n << ' ';
		cout << prompt;
		wc.SetColor();
		return getline(cin, line);
	}
};


/// 304
int
main(int argc, char* argv[])
{
	using namespace std;

	platform::YDebugSetStatus(true);
	try
	{
		Interpreter intp;

		while(intp.WaitForLine() && intp.Process())
			;
	}
	catch(LoggedEvent& e) // TODO: Logging.
	{
		cerr << "An logged error occured: " << endl << e.what() << endl;
		return EXIT_FAILURE;
	}
	catch(std::invalid_argument& e)
	{
		cerr << "Invalid argument found: " << endl << e.what() << endl;
		return EXIT_FAILURE;
	}
	catch(ystdex::bad_any_cast& e)
	{
		cerr << "Wrong type in any_cast from [" << typeid(e.from()).name()
			<< "] to [" << typeid(e.from()).name() << "]: " << e.what() << endl;
	}
	catch(std::exception& e)
	{
		cerr << "An error occured: " << endl << e.what() << endl;
		return EXIT_FAILURE;
	}
	catch(...)
	{
		cerr << "Unhandled exception found." << endl;
		return EXIT_FAILURE;
	}
	cout << "Exited." << endl;
}

