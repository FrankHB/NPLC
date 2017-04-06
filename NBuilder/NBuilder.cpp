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
\version r5816
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2017-04-06 14:06 +0800
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


/// 780
//@{
bool
IfDef(observer_ptr<const string> p, const ContextNode& ctx)
{
	return ystdex::call_value_or([&](string_view id){
		return bool(ResolveName(ctx, id));
	}, p);
}

void
LoadExternal(REPLContext& context, const string& name)
{
	platform::ifstream ifs(name, std::ios_base::in);

	if(ifs)
	{
		YTraceDe(Notice, "Test unit '%s' found.", name.c_str());
		FilterExceptions([&]{
			context.LoadFrom(ifs);
		});
	}
	else
		YTraceDe(Notice, "Test unit '%s' not found.", name.c_str());
}
//@}


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
	// NOTE: This is named after '#inert' in Kernel, but essentially
	//	unspecified in NPLA.
	DefineValue(root, "inert", ValueToken::Unspecified, {});
	// NOTE: Context builtins.
	DefineValue(root, "REPL-context", ValueObject(context, OwnershipTag<>()),
		{});
	DefineValue(root, "root-context", ValueObject(root, OwnershipTag<>()),
		{});
	// NOTE: Literal expression forms.
	RegisterForm(root, "$retain", Retain);
	RegisterForm(root, "$retain1", ystdex::bind1(RetainN, 1));
#if true
	// NOTE: Primitive features, listed as RnRK, except mentioned above.
/*
	The primitives are provided here to maintain acyclic dependencies on derived
		forms, also for simplicity of semantics.
	The primitives are listed in order as Chapter 4 of Revised^-1 Report on the
		Kernel Programming Language. Derived forms are not ordered likewise.
	There are some difference of listed primitives.
	See $2017-02 @ %Documentation::Workflow::Annual2017.
*/
	RegisterStrict(root, "eq?", EqualReference);
//	RegisterStrict(root, "eqv?", EqualValue);
//	RegisterForm(root, "$if", If);
	// NOTE: Though NPLA does not use cons pairs, corresponding primitives are
	//	still necessary.
	RegisterStrictUnary(root, "list?", IsList);
	// TODO: Add nonnull list predicate to improve performance?
	RegisterStrictUnary(root, "null?", IsEmpty);
	// NOTE: The applicative 'copy-es-immutable' is unsupported currently due to
	//	different implementation of control primitives.
	RegisterStrict(root, "eval", Eval);
	RegisterStrict(root, "get-current-environment",
		[](TermNode& term, ContextNode& ctx){
		term.Value = ValueObject(ctx, OwnershipTag<>());
	});
	RegisterStrict(root, "make-environment",
		[](TermNode& term, ContextNode& ctx){
		// FIXME: Parent environments?
		term.Value = ValueObject(ctx, OwnershipTag<>());
	});
//	RegisterForm(root, "$vau", Vau);
	RegisterStrictUnary<ContextHandler>(root, "wrap",
		[](const ContextHandler& h){
		// TODO: Use 'MakeMoveCopy'?
		return ToContextHandler(h);
	});
	// NOTE: This does check before wrapping.
	RegisterStrictUnary<ContextHandler>(root, "wrap1",
		[](const ContextHandler& h){
		// TODO: Use 'MakeMoveCopy'?
		if(const auto p = h.target<FormContextHandler>())
			return ToContextHandler(*p);
		throw NPLException("Wrapping failed.");
	});
	RegisterStrictUnary<ContextHandler>(root, "unwrap",
		[](const ContextHandler& h){
		// TODO: Use 'MakeMoveCopy'?
		if(const auto p = h.target<StrictContextHandler>())
			return ContextHandler(p->Handler);
		throw NPLException("Unwrapping failed.");
	});
#endif
#if true
	// NOTE: Some combiners are provided here as host primitives for
	//	more efficiency and less dependencies.
	// NOTE: The sequence operator is also available as infix ';' syntax sugar.
	RegisterForm(root, "$sequence", ReduceOrdered);
	RegisterStrict(root, "list", ReduceToList);
#else
	// NOTE: They can be derived as Kernel does.
	context.Perform(u8R"NPL($define! list $lambda x x)NPL");
#endif
	RegisterStrict(root, "id", [](TermNode& term){
		RetainN(term);
		LiftTerm(term, Deref(std::next(term.begin())));
		return
			IsBranch(term) ? ReductionStatus::Retained : ReductionStatus::Clean;
	});
	// NOTE: Derived functions with privmitive implementation.
	RegisterForm(root, "$and?", And);
	RegisterForm(root, "$delay", [](TermNode& term, ContextNode&){
		term.Remove(term.begin());
		
		ValueObject x(DelayedTerm(std::move(term)));

		term.Value = std::move(x);
		return ReductionStatus::Clean;
	});
	// TODO: Provide 'equal?'.
	RegisterForm(root, "evalv",
		static_cast<void(&)(TermNode&, ContextNode&)>(ReduceChildren));
	RegisterStrict(root, "force", [](TermNode& term){
		RetainN(term);
		return EvaluateDelayed(term,
			Access<DelayedTerm>(Deref(std::next(term.begin()))));
	});
	// NOTE: Comparison.
	RegisterStrictBinary<int>(root, "=?", ystdex::equal_to<>());
	RegisterStrictBinary<int>(root, "<?", ystdex::less<>());
	RegisterStrictBinary<int>(root, "<=?", ystdex::less_equal<>());
	RegisterStrictBinary<int>(root, ">=?", ystdex::greater_equal<>());
	RegisterStrictBinary<int>(root, ">?", ystdex::greater<>());
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
	RegisterStrict(root, "eval-u",
		ystdex::bind1(EvaluateUnit, std::ref(context)));
	RegisterStrict(root, "eval-u-in", [](TermNode& term){
		const auto i(std::next(term.begin()));
		const auto& rctx(Access<REPLContext>(Deref(i)));

		term.Remove(i);
		EvaluateUnit(term, rctx);
	}, ystdex::bind1(RetainN, 2));
	RegisterStrictUnary(root, "ifdef",
		[](TermNode& term, const ContextNode& ctx){
		return IfDef(AccessPtr<string>(term), ctx);
	});
	RegisterStrictBinary(root, "ifdefc", [&](TermNode& t, TermNode& c){
		return IfDef(AccessPtr<string>(t), Access<ContextNode>(c));
	});
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
		if(str == "environment")
			// XXX: The environment is same to context node currently.
			// FIXME: %ContextNode should be different to %ValueNode.
			return typeid(ContextNode);
		if(str == "int")
			return typeid(int);
		if(str == "operative")
			return typeid(FormContextHandler);
		if(str == "applicative")
			return typeid(StrictContextHandler);
		if(str == "string")
			return typeid(string);
		return typeid(void);
	});
	context.Perform("$define (bool? x) eqv? (get-typeid \"bool\")"
		" (typeid x)");
	context.Perform("$define (environment? x) eqv? (get-typeid \"environment\")"
		" (typeid x)");
	// FIXME: Wrong result.
	context.Perform("$define (operative? x) eqv? (get-typeid \"operative\")"
		" (typeid x)");
	// FIXME: Wrong result.
	context.Perform("$define (applicative? x) eqv? (get-typeid \"applicative\")"
		" (typeid x)");
	context.Perform("$define (int? x) eqv? (get-typeid \"int\")"
		" (typeid x)");
	context.Perform("$define (string? x) eqv? (get-typeid \"string\")"
		" (typeid x)");
	// NOTE: String library.
	RegisterStrict(root, "string=?", std::bind(CallBinaryFold<string,
		ystdex::equal_to<>>, ystdex::equal_to<>(), string(), _1));
	context.Perform(u8R"NPL($define (Retain-string str) ++ "\"" str "\"")NPL");
	RegisterStrictUnary<const int>(root, "itos", [](int x){
		return to_string(x);
	});
	RegisterStrictUnary<const string>(root, "string-length",
		[&](const string& str){
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
	context.Perform("$define (iput x) puts (itos x)");
	RegisterStrictUnary<const string>(root, "load",
		std::bind(LoadExternal, std::ref(context), _1));
	LoadExternal(context, "test.txt");
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

