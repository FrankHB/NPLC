/*
	© 2011-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r4206
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2016-01-09 23:41 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#include "NBuilder.h"
#include <streambuf>
#include <sstream>
#include <iostream>
#include <fstream>
#include <Helper/YModules.h>
#include YFM_NPL_Configuration
#include YFM_Helper_Initialization
#include YFM_YCLib_Debug
#include YFM_Win32_YCLib_MinGW32
#include "Interpreter.h"

using namespace NPL;
using namespace YSLib;
using platform_ex::MBCSToMBCS;

ValueNode GlobalRoot;

namespace
{

string
SToMBCS(const String& str, int cp = CP_ACP)
{
	return platform_ex::WCSToMBCS({reinterpret_cast<const wchar_t*>(
		str.c_str()), str.length()}, cp);
}

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

#if 0
void
PrintNode(const ValueNode& node)
{
	std::cout << ">Root size: " << node.GetSize() << std::endl;
	Traverse(node);
	std::cout << std::endl;
}
#endif

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
	if(!node.empty())
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
	std::cout << ">>>Root size: " << node.size() << std::endl;
	TraverseN(node);
	std::cout << std::endl;
}

} // unnamed namespace;

namespace NPL
{

void
EvalS(const ValueNode& root)
{
	PrintNodeN(root);
	try
	{
		PrintNodeN(TransformNPLA1(root));
	}
	catch(ystdex::bad_any_cast&)
	{
		throw LoggedEvent("Bad configuration found.", Warning);
	}
}

} // namespace NPL;


namespace
{

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
		cout << MBCSToMBCS(str) << endl;
		cout << "~----: u8 length: " << str.size() << endl;
	}
	cout << rlst.size() << " token(s) parsed." <<endl;
}

/// 304
void
ParseFile(const string& path_gbk)
{
	std::cout << ystdex::sfmt("Parse from file: %s : ", path_gbk.c_str())
		<< std::endl;

	if(ifstream ifs{MBCSToMBCS(path_gbk)})
	{
		Session sess;
		char c;

		while((c = ifs.get()), ifs)
			Session::DefaultParseByte(sess.Lexer, c);
		ParseOutput(sess.Lexer);
	}
}

/// 334
void
PrintFile(const string& path_gbk)
{
	const auto& path(MBCSToMBCS(path_gbk));
	size_t bom;
	if(ifstream ifs{path})
	{
		ifs.seekg(0, std::ios_base::end);
		bom = Text::DetectBOM(ifs, ifs.tellg()).second;
	}
	else
		throw LoggedEvent("Failed opening file.");

	std::ifstream fin(path.c_str());
	string str;

	while(bom--)
		fin.get();
	while(fin)
	{
		std::getline(fin, str);
		std::cout << SToMBCS(str) << '\n';
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

		auto& node(root.at(name));

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

/// 328
void
LoadFunctions(NPLContext& context)
{
//	LoadFunctions(root);
//	root.Add(ValueNode("parse", string("parse")));

	auto& m(context.Map);

	m.insert({"parse", ParseFile});
	m.insert({"print", PrintFile});
	m.insert({"mangle", [](const string& arg){
		if(CheckLiteral(arg) == '"')
			ParseString(ystdex::get_mid(arg));
		else
			std::cout << "Please use a string literal as argument."
				<< std::endl;
	}});
	m.insert({"system", [](const string& arg){
		std::system(arg.c_str());
	}});
	m.insert({"echo", [](const string& arg){
		if(CheckLiteral(arg) != 0)
			std::cout << ystdex::get_mid(arg) << std::endl;
	}});
	m.insert({"search", [&](const string& arg){
		SearchName(context.Root, arg);
	}});
	m.insert({"eval",
		std::bind(&NPLContext::Eval, &context, std::placeholders::_1)});
	m.insert({"evals", [](const string& arg){
		if(CheckLiteral(arg) == '\'')
			EvalS(string_view(ystdex::get_mid(arg)));
	}});
	m.insert({"evalf", [](const string& path_acp){
		if(ifstream ifs{MBCSToMBCS(path_acp)})
		{
			Configuration conf;
			ofstream f("out.txt");

			try
			{
				ifs >> conf;
				f << conf;
			}
			catch(ystdex::bad_any_cast& e)
			{
				// TODO: Avoid memory allocation.
				throw LoggedEvent(ystdex::sfmt(
					">Bad configuration found: cast failed from [%s] to [%s] .",
					e.from(), e.to()), Warning);
			}
		}
		else
			throw LoggedEvent("Invalid file: \"" + path_acp + "\".", Warning);
	}});
	m.insert({"$+", [&](const string& arg){
		context.sem = "$__+" + arg;
	}});

	auto& root(context.Root);

	RegisterContextHandler(root, "$quote",
		ContextHandler([](const SemaNode& sema, const ContextNode&){
		YAssert(!sema.empty(), "Invalid term found.");
	}, true));
	RegisterContextHandler(root, "$quote1",
		ContextHandler([](const SemaNode& sema, const ContextNode&){
		YAssert(!sema.empty(), "Invalid term found.");

		const auto n(sema.size() - 1);

		if(n != 1)
			throw LoggedEvent(ystdex::sfmt("Syntax error in '$quote1': expected"
				" 1 argument, received %zu.", n), Err);
	}, true));
	RegisterContextHandler(root, "$define",
		ContextHandler([](const SemaNode& term, const ContextNode& ctx){
		auto& c(term.GetContainerRef());

		YAssert(!c.empty(), "Invalid term found.");

	//	const auto n(c.size() - 1);
		auto i(c.begin());

		if(const auto p_id = AccessPtr<string>(Deref(++i)))
		{
			const auto& id(*p_id);

			YTraceDe(Debug, "Found identifier '%s'.", id.c_str());
			try
			{
			//	TODO: Implementation.
			}
			// TODO: Proper error handling.
			CatchThrow(..., LoggedEvent("Bad definition found.", Warning))
		}
		else
			throw LoggedEvent("List substitution is not supported yet.", Err);
	}, true));
	RegisterContextHandler(root, "$lambda",
		ContextHandler([](const SemaNode& term, const ContextNode&){
		auto& c(term.GetContainerRef());
		auto s(c.size());

		YAssert(s != 0, "Invalid term found.");
		--s;
		YTraceDe(Debug, "Found lambda abstraction with %zu argument(s).", s);
		if(s != 0)
		{
		//	TODO: Implementation.
		}
		else
			// TODO: Use proper exception type for syntax error.
			throw LoggedEvent(ystdex::sfmt("Syntax error in lambda abstraction,"
				" no arguments are found."), Err);
	}, true));
	RegisterForm(root, "+", [](SemaNode::Container::iterator i, size_t n,
		const SemaNode& sema){
		try
		{
			int sum(0);

			// FIXME: Overflow?
			while(n-- != 0)
				sum += std::stoi(Access<string>(Deref(++i)));
			sema.Value = to_string(sum);
		}
		CatchThrow(std::invalid_argument& e, LoggedEvent(e.what(), Warning))
	});
	RegisterForm(root, "add2", [](SemaNode::Container::iterator i, size_t n,
		const SemaNode& sema){
		if(n == 2)
			try
			{
				const auto e1(std::stoi(Access<string>(Deref(++i))));
				const auto e2(std::stoi(Access<string>(Deref(++i))));

				// FIXME: Overflow?
				sema.Value = to_string(e1 + e2);
			}
			CatchThrow(std::invalid_argument& e, LoggedEvent(e.what(), Warning))
	});
}

} // unnamed namespace;


/// 304
int
main(int argc, char* argv[])
{
	using namespace std;

	yunused(argc), yunused(argv);
	return FilterExceptions([]{
		try
		{
			Interpreter intp(LoadFunctions);

			while(intp.WaitForLine() && intp.Process())
				;
		}
		catch(ystdex::bad_any_cast& e)
		{
			cerr << "Wrong type in any_cast from [" << typeid(e.from()).name()
				<< "] to [" << typeid(e.from()).name() << "]: " << e.what()
				<< endl;
		}
	}, "::main") ? EXIT_FAILURE : EXIT_SUCCESS;
}

