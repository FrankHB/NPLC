/*
	© 2011-2019 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r7713
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2019-12-23 18:55 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#include "NBuilder.h" // for istringstream;
#include <iostream> // for std::cout, std::endl;
#include <typeindex>
#include <sstream> // for complete istringstream;
#include <Helper/YModules.h>
#include YFM_YSLib_Core_YApplication // for YSLib, Application;
#include YFM_NPL_Dependency // for NPL, NPL::A1, LoadNPLContextGround;
#include YFM_YSLib_Service_TextFile // for
//	YSLib::IO::SharedInputMappedFileStream, YSLib::Text::OpenSkippedBOMtream,
//	YSLib::Text::BOM_UTF_8;
#include YFM_YSLib_Core_YClock // for YSLib::Timers::HighResolutionClock,
//	std::chrono::duration_cast;
#include <ytest/timing.hpp> // for ytest::timing::once;
#include YFM_Helper_Initialization // for YSLib::TraceForOutermost;

namespace NPL
{

/// 868
#ifdef NDEBUG
#	define NPL_Impl_NBuilder_EnableDebugAction false
#else
#	define NPL_Impl_NBuilder_EnableDebugAction true
#endif
/// 860
#ifdef NDEBUG
#	define NPL_Impl_NBuilder_TestTemporaryOrder false
#else
#	define NPL_Impl_NBuilder_TestTemporaryOrder true
#endif

namespace A1
{

void
RegisterLiteralSignal(ContextNode& ctx, const string& name, SSignal sig)
{
	NPL::EmplaceLeaf<LiteralHandler>(ctx, name,
		[=](const ContextNode&) YB_ATTR_LAMBDA(noreturn) -> ReductionStatus{
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
		is.clear();
		is.seekg(0);
	}
}


/// 799
observer_ptr<REPLContext> p_context;

#if NPL_Impl_NBuilder_EnableDebugAction
/// 785
//@{
bool use_debug = {};

ReductionStatus
ProcessDebugCommand()
{
	string cmd;

begin:
	getline(std::cin, cmd);
	if(cmd == "r")
		return ReductionStatus::Retrying;
	if(cmd == "q")
		use_debug = {};
	else if(p_context && !cmd.empty())
	{
		const bool u(use_debug);

		use_debug = {};
		FilterExceptions([&]{
			LogTermValue(p_context->Perform(cmd));
		}, yfsig);
		use_debug = u;
		goto begin;
	}
	return ReductionStatus::Partial;
}

/// 858
ReductionStatus
DefaultDebugAction(TermNode& term, ContextNode& ctx)
{
	if(use_debug)
	{
		YTraceDe(Debug, "List term: %p", ystdex::pvoid(&term));
		LogTermValue(term);
		YTraceDe(Debug, "Current action type: %s.",
			ctx.Current.target_type().name());
		return ProcessDebugCommand();
	}
	return ReductionStatus::Partial;
}

/// 858
ReductionStatus
DefaultLeafDebugAction(TermNode& term, ContextNode& ctx)
{
	if(use_debug)
	{
		YTraceDe(Debug, "Leaf term: %p", ystdex::pvoid(&term));
		LogTermValue(term);
		YTraceDe(Debug, "Current action type: %s.",
			ctx.Current.target_type().name());
		return ProcessDebugCommand();
	}
	return ReductionStatus::Partial;
}
//@}
#endif


/// 801
template<typename _tNode, typename _fCallable>
ReductionStatus
ListCopyOrMove(TermNode& term, _fCallable f)
{
	Forms::CallUnary([&](_tNode& node){
		auto con(node.CreateWith(f));

		ystdex::swap_dependent(term.GetContainerRef(), con);
	}, term);
	return ReductionStatus::Retained;
}

/// 801
template<typename _tNode, typename _fCallable>
ReductionStatus
TermCopyOrMove(TermNode& term, _fCallable f)
{
	Forms::CallUnary([&](_tNode& node){
		SetContentWith(term, node, f);
	}, term);
	return ReductionStatus::Retained;
}

/// 805
int
FetchListLength(TermNode& term) ynothrow
{
	return int(term.size());
}

/// 802
void
LoadExternal(REPLContext& context, const string& name, ContextNode& ctx)
{
	const auto p_is(A1::OpenFile(name.c_str()));

	if(std::istream& is{*p_is})
	{
		YTraceDe(Notice, "Test unit '%s' found.", name.c_str());
		FilterExceptions([&]{
			TryLoadSource(context, name.c_str(), is, ctx);
		});
	}
	else
		YTraceDe(Notice, "Test unit '%s' not found.", name.c_str());
}

/// 839
ValueToken
LoadExternalRoot(REPLContext& context, const string& name)
{
	LoadExternal(context, name, context.Root);
	return ValueToken::Unspecified;
}

#if NPL_Impl_NBuilder_TestTemporaryOrder
/// 860
struct MarkGuard
{
	string String;

	MarkGuard(string str)
		: String(std::move(str))
	{
		std::printf("Create guard: %s.\n", String.c_str());
	}
	MarkGuard(const MarkGuard& m)
		: String(m.String)
	{
		std::printf("Create guard by copy: %s.\n", String.c_str());
	}
	MarkGuard(const MarkGuard&& m)
		: String(std::move(m.String))
	{
		std::printf("Create guard by move: %s.\n", String.c_str());
	}
	~MarkGuard()
	{
		std::printf("Destroy guard: %s.\n", String.c_str());
	}

	DefDeCopyAssignment(MarkGuard)
	DefDeMoveAssignment(MarkGuard)
};
#endif

/// 834
void
LoadFunctions(Interpreter& intp, REPLContext& context)
{
	using namespace std::placeholders;
	using namespace Forms;
	auto& rctx(context.Root);
	auto& renv(rctx.GetRecordRef());
	const auto load_std_module([&](string_view module_name,
		void(&load_module)(REPLContext&)){
		LoadModuleChecked(rctx, "std." + string(module_name),
			std::bind(load_module, std::ref(context)));
	});
	string init_trace_option;

	rctx.Trace.FilterLevel = FetchEnvironmentVariable(init_trace_option,
		"NBUILDER_TRACE") ? Logger::Level::Debug : Logger::Level::Informative;
	p_context = make_observer(&context);
	LoadGroundContext(context);
	load_std_module("strings", LoadModule_std_strings);
	load_std_module("environments", LoadModule_std_environments),
	load_std_module("promises", LoadModule_std_promises);
	load_std_module("io", LoadModule_std_io),
	load_std_module("system", LoadModule_std_system);
	LoadModuleChecked(rctx, "env_SHBuild_", [&]{
		LoadModule_SHBuild(context);
		// XXX: Overriding.
		rctx.GetRecordRef().Define("SHBuild_BaseTerminalHook_",
			ValueObject(function<void(const string&, const string&)>(
			[](const string& n, const string& val){
				// XXX: Errors from stream operations are ignored.
				using namespace std;
				Terminal te;

				cout << te.LockForeColor(DarkCyan) << n;
				cout << " = \"";
				cout << te.LockForeColor(DarkRed) << val;
				cout << '"' << endl;
		})));
	});
	// TODO: Extract literal configuration API.
	{
		// TODO: Blocked. Use C++14 lambda initializers to simplify
		//	implementation.
		auto lit_base(std::move(rctx.EvaluateLiteral.begin()->second));
		auto lit_ext(FetchExtendedLiteralPass());

		rctx.EvaluateLiteral = [lit_base, lit_ext](TermNode& term,
			ContextNode& ctx, string_view id) -> ReductionStatus{
			const auto res(lit_ext(term, ctx, id));

			if(res == ReductionStatus::Clean
				&& term.Value.type() == ystdex::type_id<TokenValue>())
				return lit_base(term, ctx, id);
			return res;
		};
	}
	// NOTE: Literal builtins.
	RegisterLiteralSignal(rctx, "exit", SSignal::Exit);
	RegisterLiteralSignal(rctx, "cls", SSignal::ClearScreen);
	RegisterLiteralSignal(rctx, "about", SSignal::About);
	RegisterLiteralSignal(rctx, "help", SSignal::Help);
	RegisterLiteralSignal(rctx, "license", SSignal::License);
	// NOTE: Definition of %inert is in %YFramework.NPL.Dependency.
	// NOTE: Context builtins.
	renv.DefineChecked("REPL-context", ValueObject(context, OwnershipTag<>()));
	renv.DefineChecked("root-context", ValueObject(rctx, OwnershipTag<>()));
	intp.SaveGround();

	auto& root_env(rctx.GetRecordRef());

	// NOTE: Literal expression forms.
	RegisterForm(rctx, "$retain", Retain);
	RegisterForm(rctx, "$retain1", ystdex::bind1(RetainN, 1));
#if true
	// NOTE: Primitive features, listed as RnRK, except mentioned above. See
	//	%YFramework.NPL.Dependency.
	// NOTE: Definitions of eq?, eql?, eqr?, eqv? are in
	//	%YFramework.NPL.Dependency.
	// NOTE: Definition of $if is in %YFramework.NPL.Dependency.
	RegisterUnary<Strict, const string>(rctx, "symbol-string?", IsSymbol);
	RegisterUnary<>(rctx, "list?", ComposeReferencedTermOp(IsList));
	RegisterUnary<>(rctx, "listv?", IsList);
	// TODO: Add nonnull list predicate to improve performance?
	// NOTE: Definitions of null?, nullv?, reference?, bound-lvalue?,
	//	uncollapsed?, unique?, move!, transfer!, deshare, expire are in
	//	%YFramework.NPL.Dependency.
	RegisterStrict(rctx, "as-const", [](TermNode& term){
		return Forms::CallRawUnary([&](TermNode& tm){
			if(const auto p = NPL::TryAccessLeaf<TermReference>(tm))
				p->SetTags(p->GetTags() | TermTags::Nonmodifying);
			LiftTerm(term, tm);
			return ReductionStatus::Retained;
		}, term);
	});
	// NOTE: Definitions of ref&, assign@!, cons, cons%, set-rest!, set-rest%!,
	//	eval, eval%, copy-environment, lock-current-environment,
	//	lock-environment, make-environment, weaken-environment are in
	//	%YFramework.NPL.Dependency.
	RegisterUnary<>(rctx, "resolve-environment", [](const TermNode& term){
		return ResolveEnvironment(term).first;
	});
	// NOTE: Environment mutation is optional in Kernel and supported here.
	// NOTE: Definitions of $deflazy!, $def!, $defrec! are in
	//	%YFramework.NPL.Dependency.
	// NOTE: Removing definitions do not guaranteed supported by all
	//	environments. They are as-is for the current environment implementation,
	//	but may not work for some specific environments in future.
	RegisterForm(rctx, "$undef!", Undefine);
	RegisterForm(rctx, "$undef-checked!", UndefineChecked);
	// NOTE: Definitions of $vau, $vau/e, wrap and wrap% are in
	//	%YFramework.NPL.Dependency.
	// NOTE: The applicatives 'wrap1' and 'wrap1%' do check before wrapping.
	RegisterStrict(rctx, "wrap1", WrapOnce);
	RegisterStrict(rctx, "wrap1%", WrapOnceRef);
	// XXX: Use unsigned count.
	RegisterBinary<Strict, const ContextHandler, const int>(rctx, "wrap-n",
		[](const ContextHandler& h, int n) -> ContextHandler{
		if(const auto p = h.target<FormContextHandler>())
			return FormContextHandler(p->Handler, p->Check, size_t(n));
		return FormContextHandler(h, 1);
	});
	// NOTE: Definitions of unwrap is in %YFramework.NPL.Dependency.
#endif
	// NOTE: NPLA value transferring.
	RegisterUnary<>(rctx, "vcopy", [](const TermNode& node){
		return node.Value.MakeCopy();
	});
	RegisterUnary<>(rctx, "vcopymove", [](TermNode& node){
		// NOTE: Shallow copy or move.
		return node.Value.CopyMove();
	});
	RegisterUnary<>(rctx, "vmove", [](const TermNode& node){
		return node.Value.MakeMove();
	});
	RegisterUnary<>(rctx, "vmovecopy", [](const TermNode& node){
		return node.Value.MakeMoveCopy();
	});
	RegisterStrict(rctx, "lcopy", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeCopy);
	});
	RegisterStrict(rctx, "lcopymove", [](TermNode& term){
		return ListCopyOrMove<TermNode>(term, &ValueObject::CopyMove);
	});
	RegisterStrict(rctx, "lmove", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeMove);
	});
	RegisterStrict(rctx, "lmovecopy", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeMoveCopy);
	});
	RegisterStrict(rctx, "tcopy", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeCopy);
	});
	RegisterStrict(rctx, "tcopymove", [](TermNode& term){
		return TermCopyOrMove<TermNode>(term, &ValueObject::CopyMove);
	});
	RegisterStrict(rctx, "tmove", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeMove);
	});
	RegisterStrict(rctx, "tmovecopy", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeMoveCopy);
	});
	// XXX: For test or debug only.
#if NPL_Impl_NBuilder_EnableDebugAction
	RegisterUnary<>(rctx, "tt", DefaultDebugAction);
	RegisterUnary<Strict, const string>(rctx, "dbg", [](const string& cmd){
		if(cmd == "on")
			use_debug = true;
		else if(cmd == "off")
			use_debug = {};
		else if(cmd == "crash")
			terminate();
	});
#endif
	RegisterForm(rctx, "$crash", []{
		terminate();
	});
	RegisterUnary<Strict, const string>(rctx, "trace", [&](const string& cmd){
		const auto set_t_lv([&](const string& s) -> Logger::Level{
			if(s == "on")
				return Logger::Level::Debug;
			else if(s == "off")
				return Logger::Level::Informative;
			else
				throw std::invalid_argument(
					"Invalid predefined trace option found.");
		});

		if(cmd == "on" || cmd == "off")
			rctx.Trace.FilterLevel = set_t_lv(cmd);
		else if(cmd == "reset")
			rctx.Trace.FilterLevel = set_t_lv(init_trace_option);
		else
			throw std::invalid_argument("Invalid trace option found.");
	});
	// NOTE: Derived functions with probable privmitive implementation.
	// NOTE: Object interoperation.
	// NOTE: Definitions of ref is in %YFramework.NPL.Dependency.
	// NOTE: Environments library.
	RegisterUnary<Strict, const std::type_index>(rctx, "nameof",
		[](const std::type_index& ti){
		return string(ti.name());
	});
	// NOTE: Type operation library.
	RegisterUnary<>(rctx, "typeid", [](const TermNode& term){
		// FIXME: Get it work with %YB_Use_LightweightTypeID.
		return std::type_index(ReferenceTerm(term).Value.type());
	});
	// TODO: Copy of operand cannot be used for move-only types.
	RegisterUnary<Strict, const string>(rctx, "get-typeid",
		[&](const string& str) -> std::type_index{
		if(str == "bool")
			return typeid(bool);
		if(str == "symbol")
			return typeid(TokenValue);
		// XXX: The environment type is not unique.
		if(str == "environment")
			return typeid(EnvironmentReference);
		if(str == "environment#owned")
			return typeid(shared_ptr<NPL::Environment>);
		if(str == "int")
			return typeid(int);
		if(str == "combiner")
			return typeid(ContextHandler);
		if(str == "string")
			return typeid(string);
		return typeid(void);
	});
	RegisterUnary<Strict, const ContextHandler>(rctx, "get-wrapping-count",
		// FIXME: Unsigned count shall be used.
		[&](const ContextHandler& h) -> int{
		if(const auto p = h.target<FormContextHandler>())
			return int(p->Wrapping);
		return 0;
	});
	// NOTE: List library.
	// TODO: Check list type?
	RegisterUnary<>(rctx, "list-length",
		ComposeReferencedTermOp(FetchListLength));
	RegisterUnary<>(rctx, "listv-length", FetchListLength);
	RegisterUnary<>(rctx, "branch?", ComposeReferencedTermOp(IsBranch));
	RegisterUnary<>(rctx, "branchv?", IsBranch);
	RegisterUnary<>(rctx, "leaf?", ComposeReferencedTermOp(IsLeaf));
	RegisterUnary<>(rctx, "leafv?", IsLeaf);
	// NOTE: Encapsulations library is in %YFramework.NPL.Dependency.
	// NOTE: String library.
	// NOTE: Definitions of ++, string-empty?, string<- are in
	//	%YFramework.NPL.Dependency.
	RegisterBinary<Strict, const string, const string>(rctx, "string=?",
		ystdex::equal_to<>());
	RegisterUnary<Strict, const int>(rctx, "itos", [](int x){
		return string(make_string_view(to_string(x)));
	});
	RegisterUnary<Strict, const string>(rctx, "string-length",
		[&](const string& str) ynothrow{
		return int(str.length());
	});
	// NOTE: Definitions of string->symbol, symbol->string, string->regex,
	//	regex-match? are in module std.strings in %YFramework.NPL.Dependency.
	// NOTE: Comparison.
	RegisterBinary<Strict, const int, const int>(rctx, "=?",
		ystdex::equal_to<>());
	RegisterBinary<Strict, const int, const int>(rctx, "<?", ystdex::less<>());
	RegisterBinary<Strict, const int, const int>(rctx, "<=?",
		ystdex::less_equal<>());
	RegisterBinary<Strict, const int, const int>(rctx, ">=?",
		ystdex::greater_equal<>());
	RegisterBinary<Strict, const int, const int>(rctx, ">?",
		ystdex::greater<>());
	// NOTE: Arithmetic procedures.
	// FIXME: Overflow?
	RegisterStrict(rctx, "+", std::bind(CallBinaryFold<int, ystdex::plus<>>,
		ystdex::plus<>(), 0, _1));
	// FIXME: Overflow?
	RegisterBinary<Strict, const int, const int>(rctx, "add2", ystdex::plus<>());
	// FIXME: Underflow?
	RegisterBinary<Strict, const int, const int>(rctx, "-", ystdex::minus<>());
	// FIXME: Overflow?
	RegisterStrict(rctx, "*", std::bind(CallBinaryFold<int,
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1));
	// FIXME: Overflow?
	RegisterBinary<Strict, const int, const int>(rctx, "multiply2",
		ystdex::multiplies<>());
	RegisterBinary<Strict, const int, const int>(rctx, "/", [](int e1, int e2){
		if(e2 != 0)
			return e1 / e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterBinary<Strict, const int, const int>(rctx, "%", [](int e1, int e2){
		if(e2 != 0)
			return e1 % e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	// NOTE: I/O library.
	RegisterStrict(rctx, "read-line", [](TermNode& term){
		RetainN(term, 0);

		string line;

		std::getline(std::cin, line);
		term.Value = line;
	});
	RegisterStrict(rctx, "display", ystdex::bind1(LogTermValue, Notice));
	RegisterUnary<Strict, const string>(rctx, "echo", Echo);
	RegisterUnary<Strict, const string>(rctx, "load",
		std::bind(LoadExternalRoot, std::ref(context), _1));
	RegisterUnary<Strict, const string>(rctx, "load-at-root",
		[&](const string& name){
		const ystdex::guard<EnvironmentSwitcher>
			gd(rctx, rctx.SwitchEnvironment(root_env.shared_from_this()));

		return LoadExternalRoot(context, name);
	});
	RegisterUnary<Strict, const string>(rctx, "ofs", [&](const string& path){
		if(ifstream ifs{path})
			return ifs;
		throw LoggedEvent(
			ystdex::sfmt("Failed opening file '%s'.", path.c_str()));
	});
	RegisterUnary<Strict, const string>(rctx, "oss", [&](const string& str){
		return istringstream(str);
	});
	RegisterUnary<Strict, ifstream>(rctx, "parse-f", ParseStream);
	RegisterUnary<Strict, const string>(rctx, "lex", [&](const string& unit){
		LexicalAnalyzer lex;

		for(const auto& c : unit)
			lex.ParseByte(c);
		return lex;
	});
	RegisterUnary<Strict, LexicalAnalyzer>(rctx, "parse-lex", ParseOutput);
	RegisterUnary<Strict, std::istringstream>(rctx, "parse-s", ParseStream);
	RegisterUnary<Strict, const string>(rctx, "put", [&](const string& str){
		std::cout << EncodeArg(str);
		return ValueToken::Unspecified;
	});
	RegisterUnary<Strict, const string>(rctx, "puts", [&](const string& str){
		// XXX: Overridding.
		std::cout << EncodeArg(str) << std::endl;
		return ValueToken::Unspecified;
	});
#if NPL_Impl_NBuilder_TestTemporaryOrder
	RegisterUnary<Strict, const string>(renv, "mark-guard", [](string str){
		return MarkGuard(std::move(str));
	});
	RegisterStrict(renv, "mark-guard-test", []{
		MarkGuard("A"), MarkGuard("B"), MarkGuard("C");
	});
#endif
	LoadExternalRoot(context, "test.txt");
#if NPL_Impl_NBuilder_EnableDebugAction
	rctx.EvaluateList.Add(DefaultDebugAction, 255);
	rctx.EvaluateLeaf.Add(DefaultLeafDebugAction, 255);
#endif
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
	Deref(LockCommandArguments()).Reset(argc, argv);
	return FilterExceptions([]{
		Application app;
		Interpreter intp(app, [&](REPLContext& context){
			using namespace chrono;
			const auto d(ytest::timing::once(
				YSLib::Timers::HighResolutionClock::now,
				LoadFunctions, std::ref(intp), context));

			cout << "NPLC initialization finished in " << d.count() / 1e9
				<< " second(s)." << endl;
		});

		while(intp.WaitForLine() && intp.Process())
			;
	}, yfsig, Alert, TraceForOutermost) ? EXIT_FAILURE : EXIT_SUCCESS;
}

