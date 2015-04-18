/*
	© 2011-2015 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r3927
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2015-04-18 13:08 +0800
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
#include YFM_MinGW32_YCLib_MinGW32
#include "Interpreter.h"

using namespace NPL;
using namespace YSLib;
using platform_ex::MBCSToMBCS;

ValueNode GlobalRoot;

string
SToMBCS(const String& str, int cp = CP_ACP)
{
	return platform_ex::WCSToMBCS(str.length(),
		reinterpret_cast<const wchar_t*>(str.c_str()), cp);
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

/// 403
const ValueNode&
GetNodeFromPath(const ValueNode& root, list<string> path)
{
	auto p(&root);

	for(const auto& n : path)
		p = &p->at(n);
	return *p;
}

/// 403
const ValueNode&
GetCurrentNode()
{
	return GetNodeFromPath(GlobalRoot, GlobalPath);
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
		cout << MBCSToMBCS(str) << endl;
		cout << "~----: u8 length: " << str.size() << endl;
	}
	cout << rlst.size() << " token(s) parsed." <<endl;
}

/// 330
void
ParseFileImpl(LexicalAnalyzer& lex, const string& path_gbk)
{
	const auto& path(MBCSToMBCS(path_gbk));
	TextFile tf(path.c_str());

	if(!tf)
		throw LoggedEvent("Invalid file: \"" + path_gbk + "\".", Warning);
	{
		ystdex::ifile_iterator i(tf.GetPtr());

		while(!tf.CheckEOF())
		{
			if(YB_UNLIKELY(is_undereferenceable(i)))
				throw LoggedEvent("Bad Source!", Critical);
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
	const auto& path(MBCSToMBCS(path_gbk));
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

} // unnamed namespace;


/// 328
void
LoadFunctions(NPLContext& context)
{
//	LoadFunctions(root);
//	root.Add(ValueNode("parse", string("parse")));

	auto& m(context.Map);

	m.insert({"parse", ParseFile});
	m.insert({"print", PrintFile});
	m.insert({"cd", [](const string& arg){
		if(arg == "..")
		{
			if(GlobalPath.empty())
				throw LoggedEvent("No parent node found.", Warning);
			GlobalPath.pop_back();
		}
		else
		{
			auto& node(GetCurrentNode());

			try
			{
				node.at(arg);
			}
			catch(std::out_of_range&)
			{
				throw LoggedEvent("No node found.", Warning);
			}
			catch(ystdex::bad_any_cast&)
			{
				throw LoggedEvent("Node is empty.", Warning);
			}
			GlobalPath.push_back(arg);
		}
	}});
	m.insert({"add", [](const string& arg){
		if(arg == "..")
			throw LoggedEvent("Invalid node name found.", Warning);
		GetCurrentNode() += {0, arg};
	}});
	m.insert({"set", [](const string& arg){
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
				throw LoggedEvent("Wrong type found.", Warning);
			}
	}});
	m.insert({"get", [](const string&){
		auto& node(GetCurrentNode());

		try
		{
			std::cout << Access<string>(node) << std::endl;
		}
		catch(ystdex::bad_any_cast&)
		{
			throw LoggedEvent("Wrong type found.", Warning);
		}
	}});
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
			EvalS(ystdex::get_mid(arg));
	}});
	m.insert({"evalf", [](const string& path_acp){
		if(TextFile tf{MBCSToMBCS(path_acp)})
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
					e.from(), e.to()), Warning);
			}
		}
		else
			throw LoggedEvent("Invalid file: \"" + path_acp + "\".", Warning);
	}});
	m.insert({"$+", [&](const string& arg){
		context.sem = "$__+" + arg;
	}});
}


/// 304
int
main(int argc, char* argv[])
{
	using namespace std;

	try
	{
		Interpreter intp(LoadFunctions);

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

