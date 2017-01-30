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
\version r5760
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2017-01-30 09:53 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#include "NBuilder.h"
#include <iostream>
#include <typeindex>
#include <Helper/YModules.h>
#include YFM_YSLib_Core_YApplication // for YSLib, Application;
#include YFM_NPL_Dependency // for NPL, NPL::A1, LoadNPLContextForSHBuild;

namespace NPL
{

namespace A1
{

void
RegisterLiteralSignal(ContextNode& node, const string& name, SSignal sig)
{
	RegisterLiteralHandler(node, name,
		[=](const ContextNode&) YB_ATTR(noreturn) -> ReductionStatus{
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

	LoadNPLContextForSHBuild(context);
	// TODO: Extract literal configuration API.
//	LoadSequenceSeparators(root, context.ListTermPreprocess);
//	LoadDeafultLiteralPasses(root);
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
	RegisterForm(root, "$quote", Quote);
	RegisterForm(root, "$quote1",
		ystdex::bind1(QuoteN, 1));
	// NOTE: Binding and control forms.
	RegisterForm(root, "$lambda", Lambda);
	// NOTE: Privmitive procedures.
	RegisterForm(root, "$and", And);
	RegisterForm(root, "begin", ReduceOrdered),
	RegisterStrict(root, "eq?", EqualReference);
	RegisterForm(root, "list",
		static_cast<void(&)(TermNode&, ContextNode&)>(ReduceChildren));
	// NOTE: Arithmetic procedures.
	// FIXME: Overflow?
	RegisterStrict(root, "+", std::bind(CallBinaryFold<int, ystdex::plus<>>,
		ystdex::plus<>(), 0, _1));
	// FIXME: Overflow?
	RegisterStrictBinary<int>(root, "add2", ystdex::plus<>());
	// FIXME: Underflow?
	RegisterStrictBinary<int>(root, "-", ystdex::minus<>());
	// FIXME: Overflow?
	RegisterStrict(root, "*", std::bind(CallBinaryFold<int,
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1));
	// FIXME: Overflow?
	RegisterStrictBinary<int>(root, "multiply2", ystdex::multiplies<>());
	RegisterStrictBinary<int>(root, "/", [](int e1, int e2){
		if(e2 != 0)
			return e1 / e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterStrictBinary<int>(root, "%", [](int e1, int e2){
		if(e2 != 0)
			return e1 % e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
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
		// XXX: Overridding.
		std::cout << EncodeArg(str) << std::endl;
	});
	// NOTE: Interoperation library.
	RegisterStrict(root, "display", ystdex::bind1(LogTree, Notice));
	RegisterStrictUnary<const string>(root, "echo", Echo);
	RegisterStrict(root, "eval",
		ystdex::bind1(Eval, std::ref(context)));
	RegisterStrict(root, "eval-in", [](TermNode& term){
		const auto i(std::next(term.begin()));
		const auto& rctx(Access<REPLContext>(Deref(i)));

		term.Remove(i);
		Eval(term, rctx);
	}, ystdex::bind1(QuoteN, 2));
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
	// NOTE: Type operation library.
	RegisterStrictUnary(root, "typeid", [](TermNode& term){
		// FIXME: Get it work with %YB_Use_LightweightTypeID.
		return std::type_index(term.Value.GetType());
	});
	context.Perform("$define (ptype x) puts (nameof (typeid(x)))");
	RegisterStrictUnary<string>(root, "get-typeid",
		[&](const string& str) -> std::type_index{
		if(str == "bool")
			return typeid(bool);
		if(str == "int")
			return typeid(int);
		if(str == "string")
			return typeid(string);
		return typeid(void);
	});
	context.Perform("$define (bool? x) eqv? (get-typeid \"bool\")"
		" (typeid x)");
	context.Perform("$define (int? x) eqv? (get-typeid \"int\")"
		" (typeid x)");
	context.Perform("$define (string? x) eqv? (get-typeid \"string\")"
		" (typeid x)");
	// NOTE: String library.
	context.Perform(u8R"NPL($define (quote-string str) ++ "\"" str "\"")NPL");
	RegisterStrictUnary<const int>(root, "itos", [](int x){
		return to_string(x);
	});
	RegisterStrictUnary<const string>(root, "strlen", [&](const string& str){
		return int(str.length());
	});
	// NOTE: SHBuild builitins.
	// XXX: Overriding.
	DefineValue(root, "SHBuild_BaseTerminalHook_",
		ValueObject(std::function<void(const string&, const string&)>(
		[](const string& n, const string& val){
			// XXX: Errors from stream operations are ignored.
			using namespace std;
			Terminal te;

			cout << te.LockForeColor(DarkCyan) << n;
			cout << " = \"";
			cout << te.LockForeColor(DarkRed) << val;
			cout << '"' << endl;
	})), true);
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

