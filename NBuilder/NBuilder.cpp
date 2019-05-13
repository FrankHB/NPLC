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
\version r7297
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2019-05-13 14:34 +0800
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

namespace A1
{

void
RegisterLiteralSignal(ContextNode& ctx, const string& name, SSignal sig)
{
	NPL::EmplaceLeaf<LiteralHandler>(ctx, name,
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
		is.clear();
		is.seekg(0);
	}
}


/// 799
observer_ptr<REPLContext> p_context;

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
		LoadModule(rctx, "std." + string(module_name),
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
	LoadModule(rctx, "env_SHBuild_",
		std::bind(LoadModule_SHBuild, std::ref(context)));
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
	renv.Define("REPL-context", ValueObject(context, OwnershipTag<>()), {});
	renv.Define("root-context", ValueObject(rctx, OwnershipTag<>()), {});
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
	// NOTE: Definitions of $if is in %YFramework.NPL.Dependency.
	RegisterStrictUnary<const string>(rctx, "symbol-string?", IsSymbol);
	RegisterStrictUnary(rctx, "list?", ComposeReferencedTermOp(IsList));
	RegisterStrictUnary(rctx, "listpr?", IsList);
	// TODO: Add nonnull list predicate to improve performance?
	// NOTE: Definitions of null?, cons, cons%, set-first!, set-first%!,
	//	set-rest!, set-rest%!, eval, copy-environment, lock-current-environment,
	//	lock-environment, make-environment, weaken-environment are in
	//	%YFramework.NPL.Dependency.
	RegisterStrictUnary(rctx, "resolve-environment", [](const TermNode& term){
		return ResolveEnvironment(term).first;
	});
	// NOTE: Environment mutation is optional in Kernel and supported here.
	// NOTE: Definitions of $deflazy!, $def!, $defrec! are in
	//	%YFramework.NPL.Dependency.
	// NOTE: Removing definitions do not guaranteed supported by all
	//	environments. They are as-is for the current environment implementation,
	//	but may not work for some specific environments in future.
	RegisterForm(rctx, "$undef!", ystdex::bind1(Undefine, _2, true));
	RegisterForm(rctx, "$undef-checked!", ystdex::bind1(Undefine, _2, false));
	// NOTE: Definitions of $vau, $vau/e, wrap are in %YFramework.NPL.Dependency.
	// NOTE: The applicative 'wrap1' does check before wrapping.
	RegisterStrictUnary<const ContextHandler>(rctx, "wrap1", WrapOnce);
	// NOTE: Definitions of unwrap is in %YFramework.NPL.Dependency.
#endif
	// NOTE: NPLA value transferring.
	RegisterStrictUnary(rctx, "vcopy", [](const TermNode& node){
		return node.Value.MakeCopy();
	});
	RegisterStrictUnary(rctx, "vcopymove", [](TermNode& node){
		// NOTE: Shallow copy or move.
		return node.Value.CopyMove();
	});
	RegisterStrictUnary(rctx, "vmove", [](const TermNode& node){
		return node.Value.MakeMove();
	});
	RegisterStrictUnary(rctx, "vmovecopy", [](const TermNode& node){
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
	RegisterStrictUnary(rctx, "tt", DefaultDebugAction);
	RegisterStrictUnary<const string>(rctx, "dbg", [](const string& cmd){
		if(cmd == "on")
			use_debug = true;
		else if(cmd == "off")
			use_debug = {};
		else if(cmd == "crash")
			terminate();
	});
	RegisterForm(rctx, "$crash", []{
		terminate();
	});
	RegisterStrictUnary<const string>(rctx, "trace", [&](const string& cmd){
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
	// NOTE: Definitions of id, idv, list, list&, $quote, $defl!, first, first&,
	//	rest, rest%, rest& %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! xcons (&x &y) cons y x;
		$defl! xcons% (&x &y) cons% y x;
	)NPL");
	// NOTE: Definitions of $set!, $defv!, $lambda, $setrec!, $defl!, first,
	//	rest, apply, list*, $defw!, $lambda/e, $sequence,
	//	get-current-environment, $cond, make-standard-environment, not?, $when,
	//	$unless are in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! and? &x $sequence
			($defl! and2? (&x &y) $if (null? y) x
				($sequence ($def! h first y) (and2? ($if h x #f) (rest y))))
			(and2? #t x);
		$defl! or? &x $sequence
			($defl! or2? (&x &y) $if (null? y) x
				($sequence ($def! h first y) (or2? ($if h h x) (rest y))))
			(or2? #f x);
	)NPL");
	// NOTE: Definitions of $and?, $or?, first-null?,
	//	list-rest%, accl, accr are in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! foldl1 (&kons &knil &l) accl l null? knil first rest kons;
		$defw! map1-reverse (&appv &l) d foldl1
			($lambda (&x &xs) cons (apply appv (list x) d) xs) () l;
	)NPL");
	// NOTE: Definitions of foldr1, map1, list-concat are in
	//	%YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! list-copy-strict (l) foldr1 cons () l;
		$defl! list-copy (obj) $if (list? obj) (list-copy-strict obj) obj;
	)NPL");
	// NOTE: Definitions of append is in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! reverse (&l) foldl1 cons () l;
		$defl! snoc (&x &r) (list-concat r (list x));
		$defl! snoc% (&x &r) (list-concat r (list% x));
	)NPL");
	// NOTE: Definitions of $let is in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! filter (&accept? &ls) apply append
			(map1 ($lambda (&x) $if (apply accept? (list x)) (list x) ()) ls);
		$defl! reduce (&ls &bin &id) $cond
			((null? ls) id)
			((null? (rest& ls)) first& ls)
			(#t bin (first& ls) (reduce (rest& ls) bin id));
		$defl! assv (&object &alist) $let
			((alist
				filter ($lambda (&record) eqv? object (first record)) alist))
				$if (null? alist) () (first alist);
		$defl! memv? (&object &ls)
			apply or? (map1 ($lambda (&x) eqv? object x) ls);
		$defl! assq (&object &alist) $let
			((alist filter ($lambda (&record) eq? object (first record)) alist))
				($if (null? alist) () (first alist));
		$defl! memq? (&object &ls)
			apply or? (map1 ($lambda (&x) eq? object x) ls);
		$defl! equal? (&x &y) $if ($and? (branch? x) (branch? y))
			($and? (equal? (first x) (first y)) (equal? (rest x) (rest y)))
			(eqv? x y);
	)NPL");
	// NOTE: Definitions of $let*, $letrec are in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defv%! $letrec* (&bindings .&body) d forward
			(eval% ($if (null? bindings) (list*% $letrec bindings body)
				(list $letrec (list% (first bindings))
				(list*% $letrec* (rest% bindings) body))) d);
		$defv! $let-safe (&bindings .&body) d forward
			(eval% (list* () $let/e
				(() make-standard-environment) bindings body) d);
		$defv! $remote-eval (&o &e) d forward (eval% o (eval e d));
	)NPL");
	// NOTE: Definitions of $bindings/p->environment, $bindings->environment,
	//	$provide!, $import! are in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$def! foldr $let ((&cenv () make-standard-environment)) wrap
		(
			$set! cenv cxrs $lambda/e (weaken-environment cenv) (ls cxr)
				accr ls null? () ($lambda (&l) cxr (first l)) rest cons;
			$vau/e cenv (kons knil .ls) d
				(accr ls unfoldable? knil ($lambda (&ls) cxrs ls first)
				($lambda (&ls) cxrs ls rest) ($lambda (&x &st)
					apply kons (list-concat x (list st)) d))
		);
		$def! map $let ((&cenv () make-standard-environment)) wrap
		(
			$set! cenv cxrs $lambda/e (weaken-environment cenv) (ls cxr)
				accr ls null? () ($lambda (&l) cxr (first l)) rest cons;
			$vau/e cenv (appv .ls) d accr ls unfoldable? ()
				($lambda (&ls) cxrs ls first) ($lambda (&ls) cxrs ls rest)
					($lambda (&x &xs) cons (apply appv x d) xs)
		);
		$defw! for-each-rtl &ls env $sequence (apply map ls env) inert;
	)NPL");
	// NOTE: Definitions of unfoldable?, map-reverse for-each-ltr
	//	are in %YFramework.NPL.Dependency.
	RegisterForm(rctx, "$delay", [](TermNode& term, ContextNode&){
		RemoveHead(term);

		ValueObject x(DelayedTerm(std::move(term)));

		term.Value = std::move(x);
		return ReductionStatus::Clean;
	});
	// TODO: Provide 'equal?'.
	RegisterForm(rctx, "evalv",
		static_cast<void(&)(TermNode&, ContextNode&)>(ReduceChildren));

	// NOTE: Object interoperation.
	// NOTE: Definitions of ref is in %YFramework.NPL.Dependency.
	// NOTE: Environments.
	// NOTE: Definitions of lock-current-environment, bound?, value-of is in
	//	%YFramework.NPL.Dependency.
	// NOTE: Only '$binds?' is like in Kernel.
	context.Perform(u8R"NPL(
		$defw! environment-bound? (&e &str) d
			eval (list bound? str) (eval e d);
	)NPL");
	// NOTE: Definitions of $binds1? is in module std.environments in
	//	%YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defv! $binds? (&e .&ss) d $let ((&senv eval e d))
			foldl1 $and? #t (map1 ($lambda (s) (wrap $binds1?) senv s) ss);
	)NPL");
	RegisterStrictUnary<const string>(rctx, "lex", [&](const string& unit){
		LexicalAnalyzer lex;

		for(const auto& c : unit)
			lex.ParseByte(c);
		return lex;
	});
	RegisterStrictUnary<const std::type_index>(rctx, "nameof",
		[](const std::type_index& ti){
		return string(ti.name());
	});
	// NOTE: Type operation library.
	RegisterStrictUnary(rctx, "typeid", [](const TermNode& term){
		// FIXME: Get it work with %YB_Use_LightweightTypeID.
		return std::type_index(ReferenceTerm(term).Value.type());
	});
	// TODO: Copy of operand cannot be used for move-only types.
	context.Perform("$defl! ptype (&x) puts (nameof (typeid x))");
	RegisterStrictUnary<const string>(rctx, "get-typeid",
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
		if(str == "operative")
			return typeid(FormContextHandler);
		if(str == "applicative")
			return typeid(StrictContextHandler);
		if(str == "combiner")
			return typeid(ContextHandler);
		if(str == "string")
			return typeid(string);
		return typeid(void);
	});
	RegisterStrictUnary<const ContextHandler>(rctx, "get-combiner-type",
		[&](const ContextHandler& h){
		return std::type_index(h.target_type());
	});
	context.Perform(u8R"NPL(
		$defl! typeid-match? (&x &id) eqv? (get-typeid id) (typeid x);
		$defl! bool? (&x) typeid-match? x "bool";
		$defl! symbol? (&x) $and? (typeid-match? x "symbol")
			(symbol-string? (symbol->string x));
		$defl! environment? (&x) $or? (typeid-match? x "environment")
			(typeid-match? x "environment#owned");
		$defl! combiner? (&x) typeid-match? x "combiner";
		$defl! operative? (&x) $and? (combiner? x)
			(eqv? (get-combiner-type x) (get-typeid "operative"));
		$defl! applicative? (&x) $and? (combiner? x)
			(eqv? (get-combiner-type x) (get-typeid "applicative"));
		$defl! int? (&x) typeid-match? x "int";
		$defl! string? (&x) typeid-match? x "string";
	)NPL");
	// NOTE: List library.
	// TODO: Check list type?
	RegisterStrictUnary(rctx, "list-length",
		ComposeReferencedTermOp(FetchListLength));
	RegisterStrictUnary(rctx, "listv-length", FetchListLength);
	RegisterStrictUnary(rctx, "branch?", ComposeReferencedTermOp(IsBranch));
	RegisterStrictUnary(rctx, "branchpr?", IsBranch);
	RegisterStrictUnary(rctx, "leaf?", ComposeReferencedTermOp(IsLeaf));
	RegisterStrictUnary(rctx, "leafpr?", IsLeaf);
	// NOTE: Encapsulations.
	// NOTE: Definition of make-encapsulation-type is in
	//	%YFramework.NPL.Dependency.
	// NOTE: String library.
	// NOTE: Definitions of ++, string-empty?, string<- are in
	//	%YFramework.NPL.Dependency.
	RegisterStrictBinary<const string, const string>(rctx, "string=?",
		ystdex::equal_to<>());
	context.Perform(u8R"NPL($defl! retain-string (&str) ++ "\"" str "\"")NPL");
	RegisterStrictUnary<const int>(rctx, "itos", [](int x){
		return to_string(x);
	});
	RegisterStrictUnary<const string>(rctx, "string-length",
		[&](const string& str) ynothrow{
		return int(str.length());
	});
	// NOTE: Definitions of string->symbol, symbol->string, string->regex,
	//	regex-match? are in module std.strings in %YFramework.NPL.Dependency.
	// NOTE: Comparison.
	RegisterStrictBinary<const int, const int>(rctx, "=?",
		ystdex::equal_to<>());
	RegisterStrictBinary<const int, const int>(rctx, "<?", ystdex::less<>());
	RegisterStrictBinary<const int, const int>(rctx, "<=?",
		ystdex::less_equal<>());
	RegisterStrictBinary<const int, const int>(rctx, ">=?",
		ystdex::greater_equal<>());
	RegisterStrictBinary<const int, const int>(rctx, ">?",
		ystdex::greater<>());
	// NOTE: Arithmetic procedures.
	// FIXME: Overflow?
	RegisterStrict(rctx, "+", std::bind(CallBinaryFold<int, ystdex::plus<>>,
		ystdex::plus<>(), 0, _1));
	// FIXME: Overflow?
	RegisterStrictBinary<const int, const int>(rctx, "add2", ystdex::plus<>());
	// FIXME: Underflow?
	RegisterStrictBinary<const int, const int>(rctx, "-", ystdex::minus<>());
	// FIXME: Overflow?
	RegisterStrict(rctx, "*", std::bind(CallBinaryFold<int,
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1));
	// FIXME: Overflow?
	RegisterStrictBinary<const int, const int>(rctx, "multiply2",
		ystdex::multiplies<>());
	RegisterStrictBinary<const int, const int>(rctx, "/", [](int e1, int e2){
		if(e2 != 0)
			return e1 / e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterStrictBinary<const int, const int>(rctx, "%", [](int e1, int e2){
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
	RegisterStrictUnary<const string>(rctx, "echo", Echo);
	RegisterStrictUnary<const string>(rctx, "load",
		std::bind(LoadExternalRoot, std::ref(context), _1));
	RegisterStrictUnary<const string>(rctx, "load-at-root",
		[&](const string& name){
		const ystdex::guard<EnvironmentSwitcher>
			gd(rctx, rctx.SwitchEnvironment(root_env.shared_from_this()));

		return LoadExternalRoot(context, name);
	});
	context.Perform(u8R"NPL(
		$defl! get-module (&filename .&opt)
			$let ((&env () make-standard-environment)) $sequence
				($unless (null? opt) ($set! env module-parameters (first& opt)))
				(eval (list load filename) env) env;
	)NPL");
	RegisterStrictUnary<const string>(rctx, "ofs", [&](const string& path){
		if(ifstream ifs{path})
			return ifs;
		throw LoggedEvent(
			ystdex::sfmt("Failed opening file '%s'.", path.c_str()));
	});
	RegisterStrictUnary<const string>(rctx, "oss", [&](const string& str){
		return std::istringstream(str);
	});
	RegisterStrictUnary<ifstream>(rctx, "parse-f", ParseStream);
	RegisterStrictUnary<LexicalAnalyzer>(rctx, "parse-lex", ParseOutput);
	RegisterStrictUnary<std::istringstream>(rctx, "parse-s", ParseStream);
	RegisterStrictUnary<const string>(rctx, "put", [&](const string& str){
		std::cout << EncodeArg(str);
		return ValueToken::Unspecified;
	});
	RegisterStrictUnary<const string>(rctx, "puts", [&](const string& str){
		// XXX: Overridding.
		std::cout << EncodeArg(str) << std::endl;
		return ValueToken::Unspecified;
	});
	// NOTE: Interoperation library.
	// NOTE: Definitions of cmd-get-args, env-get, env-set, env-empty?, system,
	//	system-get, system-quote are introduced.
	LoadModule_std_system(context);
	// NOTE: SHBuild builitins.
	// XXX: Overriding.
	root_env.Define("SHBuild_BaseTerminalHook_",
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
	context.Perform("$defl! iput (&x) puts (itos x)");
	LoadExternalRoot(context, "test.txt");
	rctx.EvaluateList.Add(DefaultDebugAction, 255);
	rctx.EvaluateLeaf.Add(DefaultLeafDebugAction, 255);
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

