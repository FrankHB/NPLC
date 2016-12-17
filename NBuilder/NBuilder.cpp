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
\version r5544
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2016-12-17 22:06 +0800
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
#include <typeindex>
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
void
ParseOutput(LexicalAnalyzer& lex)
{
	const auto& cbuf(lex.GetBuffer());
	const auto xlst(lex.Literalize());
	using namespace std;
	const auto rlst(Tokenize(xlst));

	cout << "cbuf size:" << cbuf.size() << endl
		<< "xlst size:" << cbuf.size() << endl;
	for(const auto& str : rlst)
		cout << EncodeArg(str) << endl
			<< "* u8 length: " << str.size() << endl;
	cout << rlst.size() << " token(s) parsed." <<endl;
}

/// 737
void
ParseStream(std::istream& is)
{
	if(is)
	{
		Session sess;
		char c;

		while((c = is.get()), is)
			Session::DefaultParseByte(sess.Lexer, c);
		ParseOutput(sess.Lexer);
	}
}


/// 740
void
LoadFunctions(REPLContext& context)
{
	using namespace std::placeholders;
	using namespace Forms;
	auto& root(context.Root);

	LoadSequenceSeparators(root, context.ListTermPreprocess);
	LoadDeafultLiteralPasses(root);
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
	RegisterFormContextHandler(root, "$if", If, IsBranch);
	RegisterUnaryFunction(root, "$ifdef",
		[](TermNode& term, const ContextNode& ctx){
		return ystdex::call_value_or([&](string_view id){
			return bool(LookupName(ctx, id));
		}, AccessPtr<string>(term));
	});
	RegisterFunction(root, "$display", ystdex::bind1(LogTree, Notice));
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
	RegisterUnaryFunction<const string>(root, "echo", Echo);
	RegisterFunction(root, "eval", ystdex::bind1(Eval, std::ref(context)));
	RegisterFunction(root, "system", CallSystem);
	RegisterUnaryFunction<const string>(root, "ofs", [&](const string& path){
		if(ifstream ifs{path})
			return ifs;
		throw LoggedEvent(
			ystdex::sfmt("Failed opening file '%s'.", path.c_str()));
	});
	RegisterUnaryFunction<const string>(root, "oss", [&](const string& str){
		return std::istringstream(str);
	});
	RegisterUnaryFunction<ifstream>(root, "parse-f", ParseStream);
	RegisterUnaryFunction<std::istringstream>(root, "parse-s", ParseStream);
	RegisterUnaryFunction<const string>(root, "lex", [&](const string& unit){
		LexicalAnalyzer lex;

		for(const auto& c : unit)
			lex.ParseByte(c);
		return lex;
	});
	RegisterUnaryFunction<LexicalAnalyzer>(root, "parse-lex", ParseOutput);
	RegisterUnaryFunction<const string>(root, "put", [&](const string& str){
		std::cout << EncodeArg(str);
	});
	RegisterUnaryFunction<const string>(root, "puts", [&](const string& str){
		std::cout << EncodeArg(str) << std::endl;
	});
	RegisterUnaryFunction(root, "typeid", [](TermNode& term){
		// FIXME: Get it work with %YB_Use_LightweightTypeID.
		return std::type_index(term.Value.GetType());
	});
	RegisterUnaryFunction<const std::type_index>(root, "nameof",
		[](const std::type_index& ti){
		return string(ti.name());
	});
	RegisterFunction(root, "eq?", EqualReference);
	RegisterFunction(root, "eqv?", EqualValue);
	RegisterUnaryFunction<const string>(root, "env-get", [&](const string& var){
		string res;

		FetchEnvironmentVariable(res, var.c_str());
		return res;
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

