/*
	© 2011-2021 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r8333
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2021-09-16 05:15 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#include "NBuilder.h" // for istringstream, FilterExceptions, EXIT_FAILURE,
//	EXIT_SUCCESS;
#include <ystdex/base.h> // for ystdex::noncopyable;
#include <iostream> // for std::clog, std::cout, std::endl;
#include <ystdex/typeindex.h> // for ystdex::type_index;
#include <sstream> // for complete istringstream;
#include <Helper/YModules.h>
#include YFM_YSLib_Core_YApplication // for YSLib, Application;
#include YFM_NPL_NPLA1Forms // for NPL::A1::Forms;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include YFM_NPL_Dependency // for NPL, NPL::A1, EnvironmentGuard;
#include YFM_YSLib_Core_YClock // for YSLib::Timers::HighResolutionClock,
//	std::chrono::duration_cast;
#include <ytest/timing.hpp> // for ytest::timing::once;
#include YFM_Helper_Initialization // for YSLib::TraceForOutermost;

namespace NPL
{

#ifdef NDEBUG
#	define NPLC_Impl_DebugAction false
#	define NPLC_Impl_TestTemporaryOrder false
#else
#	define NPLC_Impl_DebugAction true
#	define NPLC_Impl_TestTemporaryOrder true
#endif

namespace A1
{

void
RegisterLiteralSignal(ContextNode& ctx, const string& name, SSignal sig)
{
	NPL::EmplaceLeaf<LiteralHandler>(ctx, name, [=] YB_LAMBDA_ANNOTATE(
		(const ContextNode&), , noreturn) -> ReductionStatus{
		throw sig;
	});
}

} // namespace A1;

} // namespace NPL;

using namespace NPL;
using namespace A1;
using namespace platform_ex;

namespace
{

//! \since YSLib build 737
void
ParseStream(std::istream& is)
{
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


//! \since YSLib build 799
observer_ptr<REPLContext> p_context;

#if NPLC_Impl_DebugAction
//! \since YSLib build 785
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
		return ProcessDebugCommand();
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
		return ProcessDebugCommand();
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
	auto& context(intp.Context);
	auto& rctx(context.Root);
	auto& renv(rctx.GetRecordRef());
	string init_trace_option;

	rctx.Trace.FilterLevel = FetchEnvironmentVariable(init_trace_option,
		"NBUILDER_TRACE") ? Logger::Level::Debug : Logger::Level::Informative;
	p_context = NPL::make_observer(&context);
	LoadStandardContext(context);
	context.OutputStreamPtr = NPL::make_observer(&std::cout);
	LoadModuleChecked(rctx, "env_SHBuild_", [&]{
		LoadModule_SHBuild(context);
		// XXX: Overriding.
		rctx.GetRecordRef().Define("SHBuild_BaseTerminalHook_",
			ValueObject(function<void(const string&, const string&)>(
			[&](const string& n, const string& val){
				auto& os(context.GetOutputStreamRef());
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
	intp.EnableExtendedLiterals();
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
	// XXX: Temporarily unfreeze the environment to allow the external
	//	definitions in the ground environment.
	renv.Frozen = {};

	const auto rwenv(rctx.WeakenRecord());

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
	RegisterUnary(rctx, "list?", ComposeReferencedTermOp(IsList));
	RegisterUnary(rctx, "listv?", IsList);
	// TODO: Add nonnull list predicate to improve performance?
	// NOTE: Definitions of null?, nullv?, reference?, bound-lvalue?,
	//	uncollapsed?, unique?, move!, transfer!, deshare, as-const, expire are
	//	in %YFramework.NPL.Dependency.
	RegisterStrict(rctx, "make-nocopy", [](TermNode& term){
		struct NoCopy : ystdex::noncopyable
		{};

		RetainN(term, 0);

		term.Value = NoCopy();
	});
	// NOTE: Definitions of ref&, assign@!, cons, cons%, set-rest!, set-rest%!,
	//	eval, eval%, copy-environment, lock-current-environment,
	//	lock-environment, make-environment, weaken-environment are in
	//	%YFramework.NPL.Dependency.
	RegisterUnary(rctx, "resolve-environment", [](TermNode& term){
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
			return FormContextHandler(p->Handler, size_t(n));
		return FormContextHandler(h, 1);
	});
	// NOTE: Definitions of unwrap is in %YFramework.NPL.Dependency.
#endif
	// NOTE: NPLA value transferring.
	RegisterUnary(rctx, "vcopy", [](const TermNode& node){
		return node.Value.MakeCopy();
	});
	RegisterUnary(rctx, "vcopymove", [](TermNode& node){
		// NOTE: Shallow copy or move.
		return node.Value.CopyMove();
	});
	RegisterUnary(rctx, "vmove", [](const TermNode& node){
		return node.Value.MakeMove();
	});
	RegisterUnary(rctx, "vmovecopy", [](const TermNode& node){
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
#if NPLC_Impl_DebugAction
	RegisterUnary(rctx, "tt", DefaultDebugAction);
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
	RegisterUnary<Strict, const ystdex::type_index>(rctx, "nameof",
		[](const ystdex::type_index& ti){
		return string(ti.name());
	});
	// NOTE: Type operation library.
	RegisterUnary(rctx, "typeid", [](const TermNode& term){
		return ystdex::type_index(ReferenceTerm(term).Value.type());
	});
	// TODO: Copy of operand cannot be used for move-only types.
	RegisterUnary<Strict, const string>(rctx, "get-typeid",
		[&](const string& str) -> ystdex::type_index{
		if(str == "bool")
			return ystdex::type_id<bool>();
		if(str == "symbol")
			return ystdex::type_id<TokenValue>();
		// XXX: The environment type is not unique.
		if(str == "environment")
			return ystdex::type_id<EnvironmentReference>();
		if(str == "environment#owned")
			return ystdex::type_id<shared_ptr<NPL::Environment>>();
		if(str == "int")
			return ystdex::type_id<int>();
		if(str == "combiner")
			return ystdex::type_id<ContextHandler>();
		if(str == "string")
			return ystdex::type_id<string>();
		return ystdex::type_id<void>();
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
	RegisterUnary(rctx, "list-length",
		ComposeReferencedTermOp(FetchListLength));
	RegisterUnary(rctx, "listv-length", FetchListLength);
	RegisterUnary(rctx, "leaf?", ComposeReferencedTermOp(IsLeaf));
	RegisterUnary(rctx, "leafv?", IsLeaf);
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
	RegisterBinary<Strict, string, string>(renv, "string-contains?",
		[](const string& x, const string& y){
		return x.find(y) != string::npos;
	});
	// NOTE: Definitions of string->symbol, symbol->string, string->regex,
	//	regex-match? are in module std.strings in %YFramework.NPL.Dependency.
	using Number = int;
	// NOTE: Comparison.
	RegisterBinary<Strict, const Number, const Number>(rctx, "=?",
		ystdex::equal_to<>());
	RegisterBinary<Strict, const Number, const Number>(rctx, "<?",
		ystdex::less<>());
	RegisterBinary<Strict, const Number, const Number>(rctx, "<=?",
		ystdex::less_equal<>());
	RegisterBinary<Strict, const Number, const Number>(rctx, ">=?",
		ystdex::greater_equal<>());
	RegisterBinary<Strict, const Number, const Number>(rctx, ">?",
		ystdex::greater<>());
	// NOTE: Arithmetic procedures.
	// FIXME: Overflow?
	RegisterStrict(rctx, "+", std::bind(CallBinaryFold<Number, ystdex::plus<>>,
		ystdex::plus<>(), 0, _1));
	// FIXME: Overflow?
	RegisterBinary<Strict, const Number, const Number>(rctx, "add2",
		ystdex::plus<>());
	// FIXME: Underflow?
	RegisterBinary<Strict, const Number, const Number>(rctx, "-",
		ystdex::minus<>());
	// FIXME: Overflow?
	RegisterStrict(rctx, "*", std::bind(CallBinaryFold<Number,
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1));
	// FIXME: Overflow?
	RegisterBinary<Strict, const Number, const Number>(rctx, "multiply2",
		ystdex::multiplies<>());
	RegisterBinary<Strict, const Number, const Number>(rctx, "/",
		[](const Number& e1, const Number& e2){
		if(e2 != 0)
			return e1 / e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterBinary<Strict, const Number, const Number>(rctx, "%",
		[](const Number& e1, const Number& e2){
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
	RegisterStrict(rctx, "display", [&](TermNode& term){
		RetainN(term);
		LiftOther(term, NPL::Deref(std::next(term.begin())));
		LogTermValue(term, Notice);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(rctx, "logv", ystdex::bind1(LogTermValue, Notice));
	RegisterUnary<Strict, const string>(rctx, "echo", Echo);
	if(context.IsAsynchronous())
		RegisterStrict(rctx, "load-at-root",
			[&, rwenv](TermNode& term, ContextNode& ctx){
			RetainN(term);
			// NOTE: This does not support PTC.
			RelaySwitched(ctx,
				A1::NameTypedReducerHandler(std::bind([](EnvironmentGuard&){
				return ReductionStatus::Neutral;
			}, EnvironmentGuard(rctx, rctx.SwitchEnvironment(rwenv.Lock()))),
				"guard-load"));
			return A1::RelayToLoadExternal(ctx, term, context);
		});
	else
		RegisterStrict(rctx, "load-at-root",
			[&, rwenv](TermNode& term, ContextNode& ctx){
			RetainN(term);

			const EnvironmentGuard gd(ctx, ctx.SwitchEnvironment(rwenv.Lock()));

			return A1::ReduceToLoadExternal(term, ctx, context);
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
	RegisterUnary<Strict, std::istringstream>(rctx, "parse-s", ParseStream);
	RegisterUnary<Strict, const string>(rctx, "put", [&](const string& str){
		std::cout << str;
		return ValueToken::Unspecified;
	});
#if NPLC_Impl_TestTemporaryOrder
	RegisterUnary<Strict, const string>(renv, "mark-guard", [](string str){
		return MarkGuard(std::move(str));
	});
	RegisterStrict(renv, "mark-guard-test", []{
		MarkGuard("A"), MarkGuard("B"), MarkGuard("C");
	});
#endif
	// NOTE: Preloading does not use the backtrace handling in the normal
	//	interactive interpreter run (the REPL loop and the single line
	//	evaluation).
	A1::PreloadExternal(intp.Context, "std.txt");
	renv.Frozen = true;
	intp.SaveGround();
	A1::PreloadExternal(intp.Context, "init.txt");
#if NPLC_Impl_DebugAction
	rctx.EvaluateList.Add(DefaultDebugAction, 255);
	rctx.EvaluateLeaf.Add(DefaultLeafDebugAction, 255);
#endif
}

#define NPLC_NAME "NPL console"
#define NPLC_VER "V1.2 b926+"
#if YCL_Win32
#	define NPLC_PLATFORM "[MinGW32]"
#elif YCL_Linux
#	define NPLC_PLATFORM "[Linux]"
#else
#	define NPLC_PLATFORM "[unspecified platform]"
#endif
yconstexpr auto title(NPLC_NAME" " NPLC_VER" @ (" __DATE__", " __TIME__") "
	NPLC_PLATFORM);

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
					if(YB_UNLIKELY(arg == "--"))
					{
						opt_trans = {};
						continue;
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
				const auto src(std::move(args.front()));

				args.erase(args.begin());

				const auto p_cmd_args(YSLib::LockCommandArguments());

				p_cmd_args->Arguments = std::move(args);

				Interpreter intp{};

				intp.UpdateTextColor(TitleColor);
				LoadFunctions(intp);
				// NOTE: Different strings are evaluated separatly in order.
				//	This is simlilar to klisp.
				for(const auto& str : eval_strs)
					intp.RunLine(str);
			}
			else if(!eval_strs.empty())
			{
				Interpreter intp{};

				intp.UpdateTextColor(TitleColor);
				LoadFunctions(intp);
				for(const auto& str : eval_strs)
					intp.RunLine(str);
			}
		}
		else if(xargc == 1)
		{
			using namespace std;

			Deref(LockCommandArguments()).Reset(argc, argv);

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
			clog << "Type \"exit\" to exit,"
				" \"cls\" to clear screen, \"help\", \"about\", or \"license\""
				" for more information." << endl << endl;
			intp.Run();
		}
	}, yfsig, Alert, TraceForOutermost) ? EXIT_FAILURE : EXIT_SUCCESS;
}

