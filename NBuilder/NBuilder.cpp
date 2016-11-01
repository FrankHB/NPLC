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
\version r5350
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2016-11-01 23:58 +0800
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
#include YFM_YSLib_Service_TextFile
#include YFM_Win32_YCLib_NLS

namespace NPL
{

namespace A1
{

void
RegisterLiteralSignal(ContextNode& node, const string& name, SSignal sig)
{
	RegisterLiteralHandler(node, name,
		[=](const ContextNode&) YB_ATTR(noreturn) -> bool{
		throw sig;
	});
}

} // namespace A1;

} // namespace NPL;

using namespace NPL;
using namespace A1;
using namespace YSLib;
using namespace platform_ex;

namespace
{

/// 327
void ParseOutput(LexicalAnalyzer& lex)
{
	const auto& cbuf(lex.GetBuffer());
	const auto xlst(lex.Literalize());
	using namespace std;
	const auto rlst(Tokenize(xlst));

	cout << "cbuf size:" << cbuf.size() << endl
		<< "xlst size:" << cbuf.size() << endl;
	for(const auto& str : rlst)
	{
	//	cout << str << endl;
	//	cout << String(str).GetMBCS(Text::CharSet::GBK) << endl;
		cout << MBCSToMBCS(str) << endl
			<< "* u8 length: " << str.size() << endl;
	}
	cout << rlst.size() << " token(s) parsed." <<endl;
}


/// 328
void
LoadFunctions(NPLContext& context)
{
	using namespace std::placeholders;
	using namespace Forms;
	auto& root(context.Root);

	LoadSequenceSeparators(root, context.ListTermPreprocess);
	RegisterLiteralSignal(root, "exit", SSignal::Exit);
	RegisterLiteralSignal(root, "cls", SSignal::ClearScreen);
	RegisterLiteralSignal(root, "about", SSignal::About);
	RegisterLiteralSignal(root, "help", SSignal::Help);
	RegisterLiteralSignal(root, "license", SSignal::License);
	RegisterFormContextHandler(root, "$quote", Quote, IsBranch);
	RegisterFormContextHandler(root, "$quote1",
		ystdex::bind1(QuoteN, 1), IsBranch);
	RegisterFormContextHandler(root, "$define",
		std::bind(DefineOrSet, _1, _2, true), IsBranch);
	RegisterFormContextHandler(root, "$set",
		std::bind(DefineOrSet, _1, _2, false), IsBranch);
	RegisterFormContextHandler(root, "$lambda", Lambda, IsBranch);
	// NOTE: Examples.
	// FIXME: Overflow?
	RegisterFunction(root, "+", std::bind(DoIntegerNAryArithmetics<
		ystdex::plus<>>, ystdex::plus<>(), 0, _1), IsBranch);
	// FIXME: Overflow?
	RegisterFunction(root, "add2", std::bind(DoIntegerBinaryArithmetics<
		ystdex::plus<>>, ystdex::plus<>(), _1), IsBranch);
	// FIXME: Underflow?
	RegisterFunction(root, "-", std::bind(DoIntegerBinaryArithmetics<
		ystdex::minus<>>, ystdex::minus<>(), _1), IsBranch);
	// FIXME: Overflow?
	RegisterFunction(root, "*", std::bind(DoIntegerNAryArithmetics<
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1), IsBranch);
	// FIXME: Overflow?
	RegisterFunction(root, "multiply2", std::bind(DoIntegerBinaryArithmetics<
		ystdex::multiplies<>>, ystdex::multiplies<>(), _1), IsBranch);
	RegisterFunction(root, "/", [](TermNode& term){
		DoIntegerBinaryArithmetics([](int e1, int e2){
			if(e2 != 0)
				return e1 / e2;
			throw std::domain_error("Runtime error: divided by zero.");
		}, term);
	}, IsBranch);
	RegisterFunction(root, "%", [](TermNode& term){
		DoIntegerBinaryArithmetics([](int e1, int e2){
			if(e2 != 0)
				return e1 % e2;
			throw std::domain_error("Runtime error: divided by zero.");
		}, term);
	}, IsBranch);
	RegisterUnaryFunction<const string>(root, "system", [](const string& arg){
		std::system(arg.c_str());
	});
	RegisterUnaryFunction<const string>(root, "echo", [](const string& arg){
		if(CheckLiteral(arg) != char())
			std::cout << ystdex::get_mid(arg) << std::endl;
	});
	RegisterUnaryFunction<const string>(root, "parse",
		[](const string& path_gbk){
		{
			using namespace std;
			cout << ystdex::sfmt("Parse from file: %s : ", path_gbk.c_str())
				<< endl;
		}
		if(ifstream ifs{MBCSToMBCS(path_gbk)})
		{
			Session sess;
			char c;

			while((c = ifs.get()), ifs)
				Session::DefaultParseByte(sess.Lexer, c);
			ParseOutput(sess.Lexer);
		}
	});
	RegisterUnaryFunction<const string>(root, "print",
		[](const string& path_gbk){
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
			std::cout << WCSToMBCS({reinterpret_cast<const wchar_t*>(
				str.c_str()), str.length()}, CP_ACP) << '\n';
		}
		std::cout << std::endl;
	});
	RegisterUnaryFunction<const string>(root, "mangle", [](const string& arg){
		if(CheckLiteral(arg) == '"')
		{
			const auto& str(ystdex::get_mid(arg));
			LexicalAnalyzer lex;

			for(const auto& c : str)
				lex.ParseByte(c);
			std::cout << ystdex::sfmt("Parse from str: %s : ", str.c_str())
				<< std::endl;
			ParseOutput(lex);
		}
		else
			std::cout << "Please use a string literal as argument."
				<< std::endl;
	});
	RegisterUnaryFunction<const string>(root, "search", [&](const string& arg){
		using namespace std;

		try
		{
			const auto& name(arg);
			auto& node(AccessNode(root, name));
			const auto& s(Access<string>(node));

			cout << "Found: " << name << " = " << s << endl;
		}
		CatchExpr(out_of_range&, cout << "Not found." << endl)
	});
	RegisterUnaryFunction<const string>(root, "eval", [&](const string& arg){
		if(CheckLiteral(arg) == '\'')
			NPLContext(context).Perform(ystdex::get_mid(arg));
	});
	RegisterUnaryFunction<const string>(root, "evals", [](const string& arg){
		if(CheckLiteral(arg) == '\'')
		{
			using namespace std;
			const TermNode&
				root(SContext::Analyze(string_view(ystdex::get_mid(arg))));
			const auto PrintNodeN([](const ValueNode& node){
				cout << ">>>Root size: " << node.size() << endl;
				PrintNode(cout, node);
				cout << endl;
			});

			PrintNodeN(root);
			PrintNodeN(A1::TransformNode(root));
		}
	});
	RegisterUnaryFunction<const string>(root, "evalf", [](const string& arg){
		if(ifstream ifs{DecodeArg(arg)})
		{
			Configuration conf;
			ofstream f("out.txt");

			ifs >> conf;
			f << conf;
		}
		else
			throw LoggedEvent("Invalid file: \"" + arg + "\".", Warning);
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
		Application app;
		Interpreter intp(app, LoadFunctions);

		while(intp.WaitForLine() && intp.Process())
			;
	}, "::main") ? EXIT_FAILURE : EXIT_SUCCESS;
}

