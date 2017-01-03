/*
	© 2011-2017 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r5661
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2017-01-03 15:54 +0800
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
	// NOTE: Literal builtins.
	RegisterLiteralSignal(root, "exit", SSignal::Exit);
	RegisterLiteralSignal(root, "cls", SSignal::ClearScreen);
	RegisterLiteralSignal(root, "about", SSignal::About);
	RegisterLiteralSignal(root, "help", SSignal::Help);
	RegisterLiteralSignal(root, "license", SSignal::License);
	// NOTE: Context builtins.
	DefineValue(root, "REPL-context", ValueObject(context, OwnershipTag<>()),
		{});
	DefineValue(root, "root-context", ValueObject(root, OwnershipTag<>()),
		{});
	// NOTE: Literal expression forms.
	RegisterFormContextHandler(root, "$quote", Quote, IsBranch);
	RegisterFormContextHandler(root, "$quote1",
		ystdex::bind1(QuoteN, 1), IsBranch);
	// NOTE: Binding and control forms.
	RegisterFormContextHandler(root, "$define",
		std::bind(DefineOrSet, _1, _2, true), IsBranch);
	RegisterFormContextHandler(root, "$if", If, IsBranch);
	RegisterFormContextHandler(root, "$lambda", Lambda, IsBranch);
	RegisterFormContextHandler(root, "$set",
		std::bind(DefineOrSet, _1, _2, false), IsBranch);
	// NOTE: Privmitive procedures.
	RegisterFormContextHandler(root, "$and", And);
	RegisterFormContextHandler(root, "$or", Or);
	RegisterFormContextHandler(root, "begin", ReduceOrdered),
	RegisterStrict(root, "eq?", EqualReference);
	RegisterStrict(root, "eqv?", EqualValue);
	RegisterFormContextHandler(root, "list",
		static_cast<void(&)(TermNode&, ContextNode&)>(ReduceChildren));
	context.Perform("$define (not x) eqv? x #f");
	// NOTE: Arithmetic procedures.
	// FIXME: Overflow?
	RegisterStrict(root, "+", std::bind(CallBinaryFold<int, ystdex::plus<>>,
		ystdex::plus<>(), 0, _1), IsBranch);
	// FIXME: Overflow?
	RegisterStrict(root, "add2", std::bind(CallBinaryAs<int, ystdex::plus<>>,
		ystdex::plus<>(), _1), IsBranch);
	// FIXME: Underflow?
	RegisterStrict(root, "-", std::bind(CallBinaryAs<int, ystdex::minus<>>,
		ystdex::minus<>(), _1), IsBranch);
	// FIXME: Overflow?
	RegisterStrict(root, "*", std::bind(CallBinaryFold<int,
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1), IsBranch);
	// FIXME: Overflow?
	RegisterStrict(root, "multiply2", std::bind(CallBinaryAs<int,
		ystdex::multiplies<>>, ystdex::multiplies<>(), _1), IsBranch);
	RegisterStrict(root, "/", [](TermNode& term){
		CallBinaryAs<int>([](int e1, int e2){
			if(e2 != 0)
				return e1 / e2;
			throw std::domain_error("Runtime error: divided by zero.");
		}, term);
	}, IsBranch);
	RegisterStrict(root, "%", [](TermNode& term){
		CallBinaryAs<int>([](int e1, int e2){
			if(e2 != 0)
				return e1 % e2;
			throw std::domain_error("Runtime error: divided by zero.");
		}, term);
	}, IsBranch);
	// NOTE: I/O library.
	RegisterStrictUnary<const string>(root, "ofs", [&](const string& path){
		if(ifstream ifs{path})
			return ifs;
		throw LoggedEvent(
			ystdex::sfmt("Failed opening file '%s'.", path.c_str()));
	});
	RegisterStrictUnary<const string>(root, "oss", [&](const string& str){
		return std::istringstream(str);
	});
	RegisterStrictUnary<ifstream>(root, "parse-f", ParseStream);
	RegisterStrictUnary<LexicalAnalyzer>(root, "parse-lex", ParseOutput);
	RegisterStrictUnary<std::istringstream>(root, "parse-s", ParseStream);
	RegisterStrictUnary<const string>(root, "put", [&](const string& str){
		std::cout << EncodeArg(str);
	});
	RegisterStrictUnary<const string>(root, "puts", [&](const string& str){
		std::cout << EncodeArg(str) << std::endl;
	});
	// NOTE: Interop procedures.
	RegisterStrict(root, "display", ystdex::bind1(LogTree, Notice));
	RegisterStrictUnary<const string>(root, "echo", Echo);
	RegisterStrictUnary<const string>(root, "env-get", [&](const string& var){
		string res;

		FetchEnvironmentVariable(res, var.c_str());
		return res;
	});
	RegisterStrict(root, "eval",
		ystdex::bind1(Eval, std::ref(context)), IsBranch);
	RegisterStrict(root, "eval-in", [](TermNode& term){
		const auto& rctx(Access<REPLContext>(Deref(term.rbegin())));

		term.Remove(std::prev(term.end()));
		Eval(term, rctx);
	}, ystdex::bind1(QuoteN, 2));
	RegisterStrict(root, "value-of", ValueOf);
	RegisterStrictUnary<const string>(root, "lex", [&](const string& unit){
		LexicalAnalyzer lex;

		for(const auto& c : unit)
			lex.ParseByte(c);
		return lex;
	});
	RegisterStrictUnary<const std::type_index>(root, "nameof",
		[](const std::type_index& ti){
		return string(ti.name());
	});
	RegisterStrict(root, "system", CallSystem);
	// NOTE: Type interop library.
	RegisterStrictUnary(root, "typeid", [](TermNode& term){
		// FIXME: Get it work with %YB_Use_LightweightTypeID.
		return std::type_index(term.Value.GetType());
	});
	context.Perform("$define (ptype x) puts (nameof (typeid(x)))");
	// NOTE: String library.
	RegisterStrictUnary<const int>(root, "itos", [](int x){
		return to_string(x);
	});
	RegisterStrictUnary<const string>(root, "strlen", [&](const string& str){
		return int(str.length());
	});

}

} // unnamed namespace;


/// 304
int
main(int argc, char* argv[])
{
	using namespace std;

	// XXX: Windows 10 rs2_prerelease may have some bugs for MBCS console
	//	output. String from 'std::putc' or iostream with stdio buffer (unless
	//	set with '_IONBF') seems to use wrong width of characters, resulting
	//	mojibake. This is one of the possible workaround. Note that
	//	'ios_base::sync_with_stdio({})' would also fix this problem but it would
	//	disturb prompt color setting.
	ystdex::setnbuf(stdout);
	yunused(argc), yunused(argv);
	return FilterExceptions([]{
		Application app;
		Interpreter intp(app, LoadFunctions);

		while(intp.WaitForLine() && intp.Process())
			;
	}, "::main") ? EXIT_FAILURE : EXIT_SUCCESS;
}

