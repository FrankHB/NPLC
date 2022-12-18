/*
	© 2011-2022 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r8906
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2022-12-18 19:33 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#include "NBuilder.h" // for NPL::EmplaceLeaf, NPL::A1::LiteralHandler,
//	std::ios_base, std::istream, trivial_swap, ContextState, A1::MoveKeptGuard,
//	istringstream, FilterExceptions, EXIT_FAILURE, EXIT_SUCCESS;
#include <ystdex/base.h> // for ystdex::noncopyable;
#include <iostream> // for std::clog, std::cout, std::endl;
#include <string> // for getline;
#include YFM_YSLib_Core_YObject // for PolymorphicAllocatorHolder,
//	YSLib::default_allocator, PolymorphicValueHolder, type_index, to_string,
//	make_string_view, YSLib::to_std_string, std::stoi;
#include <sstream> // for complete istringstream;
#include <Helper/YModules.h>
#include YFM_YSLib_Core_YApplication // for YSLib, Application;
#include YFM_NPL_NPLA1Forms // for NPL::A1::Forms;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include YFM_NPL_Dependency // for EnvironmentGuard, A1::RelayToLoadExternal;
#include YFM_YSLib_Core_YClock // for YSLib::Timers::HighResolutionClock,
//	std::chrono::duration_cast;
#include <ytest/timing.hpp> // for ytest::timing::once;
#include YFM_Helper_Initialization // for YSLib::TraceForOutermost;

//! \since YSLib build 957
namespace NBuilder
{

//! \since YSLib build 878
//@{
#ifdef NDEBUG
#	define NPLC_Impl_DebugAction false
#	define NPLC_Impl_TestTemporaryOrder false
#else
#	define NPLC_Impl_DebugAction true
#	define NPLC_Impl_TestTemporaryOrder true
#endif
//@}

void
RegisterLiteralSignal(BindingMap& m, const string& name, SSignal sig)
{
	NPL::EmplaceLeaf<NPL::A1::LiteralHandler>(m, name, trivial_swap,
		std::allocator_arg, m.get_allocator(),
		[=] YB_LAMBDA_ANNOTATE((const ContextNode&), , noreturn)
		-> ReductionStatus{
		throw sig;
	});
}

} // namespace NBuilder;

//! \since YSLib build 957
using namespace NBuilder;
using namespace A1;
using namespace platform_ex;

namespace
{

//! \since YSLib build 954
//@{
#if true
// XXX: This might or might not be a bit efficient.
template<class _tStream>
using GPortHolder = PolymorphicAllocatorHolder<std::ios_base, _tStream,
	YSLib::default_allocator<yimpl(byte)>>;
#else
template<class _tStream>
using GPortHolder = PolymorphicValueHolder<std::ios_base, _tStream>;
#endif

void
ParseStream(std::ios_base& sbase)
{
	auto& is(dynamic_cast<std::istream&>(sbase));

	if(is)
	{
		Session sess;
		auto& lex(sess.Lexer);
		ByteParser parse(lex);
		char c;

		while((c = is.get()), is)
			parse(c);

		const auto& cbuf(parse.GetBuffer());
		const auto rlst(sess.GetResult(parse));
		using namespace std;

		clog << "cbuf size:" << cbuf.size() << endl
			<< "token list size:" << rlst.size() << endl;
		for(const auto& str : rlst)
			clog << str << endl << "* u8 length: " << str.size() << endl;
		clog << rlst.size() << " token(s) parsed." <<endl;
		is.clear();
		is.seekg(0);
	}
}
//@}


//! \since YSLib build 955
observer_ptr<const GlobalState> p_global;

#if NPLC_Impl_DebugAction
//! \since YSLib build 785
//@{
bool use_debug = {};

//! \since YSLib build 955
ReductionStatus
ProcessDebugCommand(ContextState& cs)
{
	string cmd;

begin:
	getline(std::cin, cmd);
	if(cmd == "r")
		return ReductionStatus::Retrying;
	if(cmd == "q")
		use_debug = {};
	else if(p_global && !cmd.empty())
	{
		const bool u(use_debug);

		use_debug = {};
		FilterExceptions([&]{
			LogTermValue(p_global->Perform(cs, cmd));
		}, yfsig);
		use_debug = u;
		goto begin;
	}
	return ReductionStatus::Partial;
}

//! \since YSLib build 858
ReductionStatus
DefaultDebugAction(TermNode& term, ContextNode& ctx)
{
	if(use_debug)
	{
		YTraceDe(Debug, "List term: %p", ystdex::pvoid(&term));
		LogTermValue(term);
		YTraceDe(Debug, "Current action type: %s.",
			ctx.GetCurrentActionType().name());
		return ProcessDebugCommand(ContextState::Access(ctx));
	}
	return ReductionStatus::Partial;
}

//! \since YSLib build 858
ReductionStatus
DefaultLeafDebugAction(TermNode& term, ContextNode& ctx)
{
	if(use_debug)
	{
		YTraceDe(Debug, "Leaf term: %p", ystdex::pvoid(&term));
		LogTermValue(term);
		YTraceDe(Debug, "Current action type: %s.",
			ctx.GetCurrentActionType().name());
		return ProcessDebugCommand(ContextState::Access(ctx));
	}
	return ReductionStatus::Partial;
}
//@}
#endif


//! \since YSLib build 801
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

//! \since YSLib build 801
template<typename _tNode, typename _fCallable>
ReductionStatus
TermCopyOrMove(TermNode& term, _fCallable f)
{
	Forms::CallUnary([&](_tNode& node){
		SetContentWith(term, node, f);
	}, term);
	return ReductionStatus::Retained;
}

//! \since YSLib build 805
int
FetchListLength(TermNode& term) ynothrow
{
	return int(term.size());
}


//! \since YSLib build 945
struct NoCopy : ystdex::noncopyable
{};


#if NPLC_Impl_TestTemporaryOrder
//! \since YSLib build 860
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

//! \since YSLib build 885
void
LoadFunctions(Interpreter& intp)
{
	using namespace std::placeholders;
	using namespace Forms;
	using NPL::Environment;
	auto& global(intp.Global);
	auto& cs(intp.Main);
	auto& renv(cs.GetRecordRef());
	auto& m(renv.GetMapRef());
	string init_trace_option;

	cs.Trace.FilterLevel = FetchEnvironmentVariable(init_trace_option,
		"NBUILDER_TRACE") ? Logger::Level::Debug : Logger::Level::Informative;
	p_global = NPL::make_observer(&global);
	LoadStandardContext(cs);
	global.OutputStreamPtr = NPL::make_observer(&std::cout);
	LoadModuleChecked(m, cs, "env_SHBuild_", [&]{
		LoadModule_SHBuild(cs);
		// XXX: Overriding.
		Environment::Define(cs.GetRecordRef().GetMapRef(),
			"SHBuild_BaseTerminalHook_",
			ValueObject(function<void(const string&, const string&)>(
			[&](const string& n, const string& val){
				auto& os(global.GetOutputStreamRef());
				Terminal te;

				{
					const auto t_gd(te.LockForeColor(DarkCyan));

					YSLib::IO::StreamPut(os, n.c_str());
				}
				ystdex::write_literal(os, " = \"");
				{
					const auto t_gd(te.LockForeColor(DarkRed));

					YSLib::IO::StreamPut(os, val.c_str());
				}
				os.put('"') << std::endl;
		})));
	});
	// NOTE: Literal builtins.
	RegisterLiteralSignal(m, "exit", SSignal::Exit);
	RegisterLiteralSignal(m, "cls", SSignal::ClearScreen);
	RegisterLiteralSignal(m, "about", SSignal::About);
	RegisterLiteralSignal(m, "help", SSignal::Help);
	RegisterLiteralSignal(m, "license", SSignal::License);
	// NOTE: Definition of %inert is in %YFramework.NPL.Dependency.
	// NOTE: Context builtins.
	Environment::DefineChecked(m, "REPL-context",
		ValueObject(global, OwnershipTag<>()));
	Environment::DefineChecked(m, "root-context",
		ValueObject(cs, OwnershipTag<>()));
	// XXX: Temporarily unfreeze the environment to allow the external
	//	definitions in the ground environment.
	renv.Unfreeze();

	const auto rwenv(cs.WeakenRecord());

	// NOTE: Literal expression forms.
	RegisterForm(m, "$retain", Retain);
	RegisterForm(m, "$retain1", trivial_swap, ystdex::bind1(RetainN, 1));
#if true
	// NOTE: Primitive features, listed as RnRK, except mentioned above. See
	//	%YFramework.NPL.Dependency.
	// NOTE: Definitions of eq?, eql?, eqr?, eqv? are in
	//	%YFramework.NPL.Dependency.
	// NOTE: Definition of $if is in %YFramework.NPL.Dependency.
	RegisterUnary<Strict, const string>(m, "symbol-string?", IsSymbol);
	RegisterUnary(m, "listv?", IsList);
	// TODO: Add nonnull list predicate to improve performance?
	// NOTE: Definitions of null?, nullv?, reference?, bound-lvalue?,
	//	uncollapsed?, unique?, move!, transfer!, deshare, as-const, expire are
	//	in %YFramework.NPL.Dependency.
	RegisterStrict(m, "make-nocopy", [](TermNode& term){
		RetainN(term, 0);
		term.Value = NoCopy();
	});
	RegisterStrict(m, "make-nocopy-fn", [](TermNode& term){
		RetainN(term, 0);
		// TODO: Blocked. Use C++14 lambda initializers to simplify the
		//	implementation.
		term.Value = A1::MakeForm(term.get_allocator(), std::bind([&](NoCopy&){
			return ReductionStatus::Clean;
		}, NoCopy()), Strict);
	});
	// NOTE: Definitions of ref&, assign@!, cons, cons%, set-rest!, set-rest%!,
	//	eval, eval%, copy-environment, lock-current-environment,
	//	lock-environment, make-environment, weaken-environment are in
	//	%YFramework.NPL.Dependency.
	RegisterUnary(m, "resolve-environment", [](TermNode& x){
		return ResolveEnvironment(x).first;
	});
	// NOTE: Environment mutation is optional in Kernel and supported here.
	// NOTE: Definitions of $deflazy!, $def!, $defrec! are in
	//	%YFramework.NPL.Dependency.
	// NOTE: Removing definitions do not guaranteed supported by all
	//	environments. They are as-is for the current environment implementation,
	//	but may not work for some specific environments in future.
	RegisterForm(m, "$undef!", Undefine);
	RegisterForm(m, "$undef-checked!", UndefineChecked);
	// NOTE: Definitions of $vau, $vau/e, wrap and wrap% are in
	//	%YFramework.NPL.Dependency.
	// NOTE: The applicatives 'wrap1' and 'wrap1%' do check before wrapping.
	RegisterStrict(m, "wrap1", WrapOnce);
	RegisterStrict(m, "wrap1%", WrapOnceRef);
	// XXX: Use unsigned count.
	RegisterBinary<Strict, const ContextHandler, const int>(m, "wrap-n",
		[](const ContextHandler& h, int n) -> ContextHandler{
		if(const auto p = h.target<FormContextHandler>())
			return FormContextHandler(p->Handler, size_t(n));
		return FormContextHandler(h, Strict);
	});
	// NOTE: Definitions of unwrap is in %YFramework.NPL.Dependency.
#endif
	// NOTE: NPLA value transferring.
	RegisterUnary(m, "vcopy", [](const TermNode& x){
		return x.Value.MakeCopy();
	});
	RegisterUnary(m, "vcopymove", [](TermNode& x){
		// NOTE: Shallow copy or move.
		return x.Value.CopyMove();
	});
	RegisterUnary(m, "vmove", [](const TermNode& x){
		return x.Value.MakeMove();
	});
	RegisterUnary(m, "vmovecopy", [](const TermNode& x){
		return x.Value.MakeMoveCopy();
	});
	RegisterStrict(m, "lcopy", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeCopy);
	});
	RegisterStrict(m, "lcopymove", [](TermNode& term){
		return ListCopyOrMove<TermNode>(term, &ValueObject::CopyMove);
	});
	RegisterStrict(m, "lmove", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeMove);
	});
	RegisterStrict(m, "lmovecopy", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeMoveCopy);
	});
	RegisterStrict(m, "tcopy", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeCopy);
	});
	RegisterStrict(m, "tcopymove", [](TermNode& term){
		return TermCopyOrMove<TermNode>(term, &ValueObject::CopyMove);
	});
	RegisterStrict(m, "tmove", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeMove);
	});
	RegisterStrict(m, "tmovecopy", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeMoveCopy);
	});
	// XXX: For test or debug only.
#if NPLC_Impl_DebugAction
	RegisterUnary(m, "tt", DefaultDebugAction);
	RegisterUnary<Strict, const string>(m, "dbg", [](const string& cmd){
		if(cmd == "on")
			use_debug = true;
		else if(cmd == "off")
			use_debug = {};
		else if(cmd == "crash")
			terminate();
	});
#endif
	RegisterForm(m, "$crash", []{
		terminate();
	});
	RegisterUnary<Strict, const string>(m, "trace", trivial_swap,
		[&](const string& cmd){
		const auto set_t_lv([&](const string& str) -> Logger::Level{
			if(str == "on")
				return Logger::Level::Debug;
			else if(str == "off")
				return Logger::Level::Informative;
			else
				throw std::invalid_argument(
					"Invalid predefined trace option found.");
		});

		if(cmd == "on" || cmd == "off")
			cs.Trace.FilterLevel = set_t_lv(cmd);
		else if(cmd == "reset")
			cs.Trace.FilterLevel = set_t_lv(init_trace_option);
		else
			throw std::invalid_argument("Invalid trace option found.");
	});
	// NOTE: Derived functions with probable privmitive implementation.
	// NOTE: Object interoperation.
	// NOTE: Definitions of ref is in %YFramework.NPL.Dependency.
	// NOTE: Environments library.
	RegisterUnary<Strict, const type_index>(m, "nameof",
		[](const type_index& ti){
		return string(ti.name());
	});
	// NOTE: Type operation library.
	RegisterUnary(m, "typeid", [](const TermNode& x){
		return type_index(ReferenceTerm(x).Value.type());
	});
	// TODO: Copy of operand cannot be used for move-only types.
	RegisterUnary<Strict, const string>(m, "get-typeid",
		[](const string& str) -> type_index{
		if(str == "bool")
			return type_id<bool>();
		if(str == "symbol")
			return type_id<TokenValue>();
		// XXX: The environment type is not unique.
		if(str == "environment")
			return type_id<EnvironmentReference>();
		if(str == "environment#owned")
			return type_id<shared_ptr<NPL::Environment>>();
		if(str == "int")
			return type_id<int>();
		if(str == "combiner")
			return type_id<ContextHandler>();
		if(str == "string")
			return type_id<string>();
		return type_id<void>();
	});
	RegisterUnary<Strict, const ContextHandler>(m, "get-wrapping-count",
		// FIXME: Unsigned count shall be used.
		[](const ContextHandler& h) -> int{
		if(const auto p = h.target<FormContextHandler>())
			return int(p->GetWrappingCount());
		return 0;
	});
	// NOTE: List library.
	// TODO: Check list type?
	RegisterUnary(m, "list-length",
		ComposeReferencedTermOp(FetchListLength));
	RegisterUnary(m, "listv-length", FetchListLength);
	RegisterUnary(m, "leaf?", ComposeReferencedTermOp(IsLeaf));
	RegisterUnary(m, "leafv?", IsLeaf);
	// NOTE: Encapsulations library is in %YFramework.NPL.Dependency.
	// NOTE: String library.
	// NOTE: Definitions of ++, string-empty?, string<- are in
	//	%YFramework.NPL.Dependency.
	RegisterBinary<Strict, const string, const string>(m, "string=?",
		ystdex::equal_to<>());
	RegisterUnary<Strict, const string>(m, "string-length",
		[&](const string& str) ynothrow{
		return int(str.length());
	});
	RegisterBinary<Strict, string, string>(m, "string-contains?",
		[](const string& x, const string& y){
		return x.find(y) != string::npos;
	});
	// NOTE: Definitions of string->symbol, symbol->string, string->regex,
	//	regex-match? are in module std.strings in %YFramework.NPL.Dependency.
	// NOTE: Definitions of number functions are in module std.math in
	//	%YFramework.NPL.Dependency.
	using Number = int;
	RegisterBinary<Strict, const Number, const Number>(m, "%",
		[](const Number& e1, const Number& e2){
		if(e2 != 0)
			return e1 % e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterUnary<Strict, const int>(m, "itos", [](int x){
		return string(make_string_view(to_string(x)));
	});
	RegisterUnary<Strict, const string>(m, "stoi", [](const string& x){
		return std::stoi(YSLib::to_std_string(x));
	});
	// NOTE: I/O library.
	RegisterStrict(m, "read-line", [](TermNode& term){
		RetainN(term, 0);

		string line(term.get_allocator());

		getline(std::cin, line);
		term.SetValue(line);
	});
	RegisterUnary(m, "write", trivial_swap, [&](TermNode& term){
		WriteTermValue(global.GetOutputStreamRef(), term);
		return ValueToken::Unspecified;
	});
	RegisterUnary(m, "display", trivial_swap, [&](TermNode& term){
		DisplayTermValue(global.GetOutputStreamRef(), term);
		return ValueToken::Unspecified;
	});
	RegisterUnary(m, "logd", [](TermNode& term){
		LogTermValue(term, Notice);
		return ValueToken::Unspecified;
	});
	RegisterStrict(m, "logv", trivial_swap,
		ystdex::bind1(LogTermValue, Notice));
	RegisterUnary<Strict, const string>(m, "echo", Echo);
	if(global.IsAsynchronous())
		RegisterStrict(m, "load-at-root", trivial_swap,
			[&, rwenv](TermNode& term, ContextNode& ctx){
			RetainN(term);
			// NOTE: This does not support PTC.
			RelaySwitched(ctx, A1::MoveKeptGuard(
				EnvironmentGuard(ctx, ctx.SwitchEnvironment(rwenv.Lock()))));
			return A1::RelayToLoadExternal(ctx, term);
		});
	else
		RegisterStrict(m, "load-at-root", trivial_swap,
			[&, rwenv](TermNode& term, ContextNode& ctx){
			RetainN(term);

			const EnvironmentGuard gd(ctx, ctx.SwitchEnvironment(rwenv.Lock()));

			return A1::ReduceToLoadExternal(term, ctx);
		});
	RegisterUnary<Strict, const string>(m, "open-input-file",
		[](const string& path){
		if(ifstream ifs{path, std::ios_base::in | std::ios_base::binary})
			return ValueObject(std::allocator_arg, path.get_allocator(),
				any_ops::use_holder, in_place_type<GPortHolder<ifstream>>,
				std::move(ifs));
		throw LoggedEvent(
			ystdex::sfmt("Failed opening file '%s'.", path.c_str()));
	});
	RegisterUnary<Strict, const string>(m, "open-input-string",
		[](const string& str){
		// XXX: Blocked. Use of explicit allocator requires C++20 and later.
		//	Hopefully other changes of LWG 3006 are implemented in the previous
		//	modes (which is true for all recent libstdc++, libc++ and Microsoft
		//	VC++ STL).
		if(istringstream iss{str})
			return ValueObject(std::allocator_arg, str.get_allocator(),
				any_ops::use_holder, in_place_type<GPortHolder<istringstream>>,
				std::move(iss));
		throw LoggedEvent(
			ystdex::sfmt("Failed opening string '%s'.", str.c_str()));
	});
	RegisterUnary<Strict, std::ios_base>(m, "parse-stream", ParseStream);
#if NPLC_Impl_TestTemporaryOrder
	RegisterUnary<Strict, const string>(m, "mark-guard", [](string str){
		return MarkGuard(std::move(str));
	});
	RegisterStrict(m, "mark-guard-test", []{
		MarkGuard("A"), MarkGuard("B"), MarkGuard("C");
	});
#endif
	// NOTE: Preloading does not use the backtrace handling in the normal
	//	interactive interpreter run (the REPL loop and the single line
	//	evaluation).
	A1::PreloadExternal(cs, "std.txt");
	renv.Freeze();
	intp.SaveGround();
	A1::PreloadExternal(cs, "init.txt");
#if NPLC_Impl_DebugAction
	global.EvaluateList.Add(DefaultDebugAction, 255);
	global.EvaluateLeaf.Add(DefaultLeafDebugAction, 255);
#endif
}

//! \since YSLib build 926
//@{
template<class _tString>
auto
Quote(_tString&& str) -> decltype(ystdex::quote(yforward(str), '\''))
{
	return ystdex::quote(yforward(str), '\'');
}


const struct Option
{
	const char *prefix, *option_arg;
	// XXX: Similar to %Tools.SHBuild.Main in YSLib.
	std::vector<const char*> option_details;

	Option(const char* pfx, const char* opt_arg,
		std::initializer_list<const char*> il)
		: prefix(pfx), option_arg(opt_arg), option_details(il)
	{}
} OptionsTable[]{
	// NOTE: Alphabatical as %Tools.SHBuild.Main in YSLib.
	{"-h, --help", "", {"Print this message and exit, without entering"
		" execution modes."}},
	{"-e", " [STRING]", {"Evaluate a string if the following argument exists."
		" This option can occur more than once and combined with SRCPATH.\n"
		"\tAny instance of this option implies the interpreter running in"
		" scripting mode.\n"
		"\tEach instance of this option (with its optional argument) will be"
		" evaluated in order before evaluate the script specified by SRCPATH"
		" (if any)."}}
};

const array<const char*, 3> DeEnvs[]{
	// NOTE: The NPLA1 environment variables are from %NPL implemenations.
	{{"NPLA1_PATH", "", "NPLA1 loader path template string."}},
	// NOTE: NBuilder environment variables are specified by this
	//	implementation.
	{{"NBUILDER_TRACE", "", "Use debug tracing if set."}}
};


void
PrintHelpMessage(const string& prog)
{
	using IO::StreamPut;
	using ystdex::sfmt;
	auto& os(std::cout);

	StreamPut(os, sfmt("Usage: \"%s\" [OPTIONS ...] SRCPATH"
		" [OPTIONS ... [-- [ARGS...]]]\n"
		"  or:  \"%s\" [OPTIONS ... [-- [[SRCPATH] ARGS...]]]\n"
		"\tThis program is an interpreter of NPLA1.\n"
		"\tThere are two execution modes, scripting mode and interactive mode,"
		" exclusively. In both modes, the interpreter is initialized before"
		" further execution of the code. In scripting mode, a script file"
		" specified by the command line argument SRCPATH (if any) is run."
		" Otherwise, the program runs in the interactive mode and the REPL"
		" (read-eval-print loop) is entered, see below for details.\n"
		"\tThere are no checks on the encoding of the input code.\n"
		"\tThere are no checks on the values. Any behaviors depending"
		" on the locale-specific values are unspecified.\n"
		"\tCurrently accepted environment variable are:\n\n",
		prog.c_str(), prog.c_str()).c_str());
	for(const auto& env : DeEnvs)
		StreamPut(os, sfmt("  %s\n\t%s Default value is %s.\n\n", env[0],
			env[2], env[1][0] == '\0' ? "empty"
			: Quote(string(env[1])).c_str()).c_str());
	ystdex::write_literal(os,
		"SRCPATH\n"
		"\tThe source path. It is handled if and only if this program runs in"
		" scripting mode. In this case, SRCPATH is the 1st command line"
		" argument not recognized as an option (see below). Otherwise, the"
		" command line argument is treated as an option.\n"
		"\tSRCPATH shall specify a path to a source file, or the special value"
		" '-' which indicates the standard input."
		"\tThe source specified by SRCPATH shall have NPLA1 source tokens"
		" encoded in a text stream with optional UTF-8 BOM (byte-order mark),"
		" which are to be read and evaluated in the initial environment of the"
		" interpreter. Otherwise, errors are raise to reject the source.\n\n"
		"OPTIONS ...\nOPTIONS ... -- [[SRCPATH] ARGS ...]\n"
		"\tThe options and arguments for the program execution. After '--',"
		" options parsing is turned off and every remained command line"
		" argument (if any) is interpreted as an argument, except that the"
		" 1st command line argument is treated as SRCPATH (implying scripting"
		" mode) when there is no SRCPATH before '--'.\n"
		"\tRecognized options are handled in this program, and the remained"
		" arguments will be passed to the NPLA1 user program (in the script or"
		" in the interactive environment) which can access the arguments via"
		" appropriate API.\n\t"
		"The recognized options are:\n\n");
	for(const auto& opt : OptionsTable)
	{
		StreamPut(os,
			sfmt("  %s%s\n", opt.prefix, opt.option_arg).c_str());
		for(const auto& des : opt.option_details)
			StreamPut(os, sfmt("\t%s\n", des).c_str());
		os << '\n';
	}
	ystdex::write_literal(os,
		"The exit status is 0 on success and 1 otherwise.\n");
}
//@}


#define NPLC_NAME "NPL console"
#define NPLC_VER "V1.4+ b961+"
#if YCL_Win32
#	define NPLC_PLATFORM "[MinGW32]"
#elif YCL_Linux
#	define NPLC_PLATFORM "[Linux]"
#else
#	define NPLC_PLATFORM "[unspecified platform]"
#endif
yconstexpr auto title(NPLC_NAME" " NPLC_VER" @ (" __DATE__", " __TIME__") "
	NPLC_PLATFORM);

//! \since YSLib build 962
void
RunEvalStrings(Interpreter& intp, vector<string>& eval_strs)
{
	intp.UpdateTextColor(TitleColor);
	LoadFunctions(intp);
	// NOTE: Different strings are evaluated separatly in order.
	//	This is simlilar to klisp.
	for(const auto& str : eval_strs)
		intp.RunLine(str);
}

//! \since YSLib build 962
void
RunInteractive()
{
	using namespace std;
	Interpreter intp{};

	intp.UpdateTextColor(TitleColor, true);
	clog << title << endl << "Initializing...";
	{
		using namespace chrono;
		const auto d(ytest::timing::once(
			YSLib::Timers::HighResolutionClock::now,
			LoadFunctions, std::ref(intp)));

		clog << "NPLC initialization finished in " << d.count() / 1e9
			<< " second(s)." << endl;
	}
	intp.UpdateTextColor(InfoColor, true);
	clog << "Type \"exit\" to exit, \"cls\" to clear screen, \"help\","
		" \"about\", or \"license\" for more information." << endl << endl;
	intp.Run();
}

} // unnamed namespace;


//! \since YSLib build 304
int
main(int argc, char* argv[])
{
	// XXX: Windows 10 rs2_prerelease may have some bugs for MBCS console
	//	output. String from 'std::putc' or iostream with stdio buffer (unless
	//	set with '_IONBF') seems to use wrong width of characters, resulting
	//	mojibake. This is one of the possible workaround. Note that
	//	'ios_base::sync_with_stdio({})' would also fix this problem but it would
	//	disturb prompt color setting.
	ystdex::setnbuf(stdout);
	return FilterExceptions([&]{
		// NOTE: Allocators are specified in the interpreter instance, not here.
		//	This also makes it easier to prevent the invalid resource used as it
		//	in %Tools.SHBuild.Main in YSLib.
		const YSLib::CommandArguments xargv(argc, argv);
		const auto xargc(xargv.size());

		if(xargc > 1)
		{
			vector<string> args;
			bool opt_trans(true);
			bool requires_eval = {};
			vector<string> eval_strs;

			for(size_t i(1); i < xargc; ++i)
			{
				string arg(xargv[i]);

				if(opt_trans)
				{
					// NOTE: This conforms to POSIX.1-2017 utility convention,
					//	Guideline 10, see https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html.
					//	Also note '-' is not treated as a delimeter, which is
					//	different to some other utilities like klisp.
					if(YB_UNLIKELY(arg == "--"))
					{
						opt_trans = {};
						continue;
					}
					if(arg == "-h" || arg == "--help")
					{
						PrintHelpMessage(xargv[0]);
						return;
					}
					else if(arg == "-e")
					{
						requires_eval = true;
						continue;
					}
				}
				if(requires_eval)
				{
					eval_strs.push_back(std::move(arg));
					requires_eval = {};
				}
				else
					args.push_back(std::move(arg));
			}
			if(!args.empty())
			{
				auto src(std::move(args.front()));

				args.erase(args.begin());

				const auto p_cmd_args(YSLib::LockCommandArguments());

				p_cmd_args->Arguments = std::move(args);

				Interpreter intp{};

				RunEvalStrings(intp, eval_strs);
				// NOTE: The special name '-' is handled here. This conforms to
				//	POSIX.1-2017 utility convention, Guideline 13.
				intp.RunScript(std::move(src));
			}
			else if(!eval_strs.empty())
			{
				Interpreter intp{};

				RunEvalStrings(intp, eval_strs);
			}
			else
				RunInteractive();
		}
		else if(xargc == 1)
		{
			Deref(LockCommandArguments()).Reset(argc, argv);
			RunInteractive();
		}
	}, yfsig, Alert, TraceForOutermost) ? EXIT_FAILURE : EXIT_SUCCESS;
}

