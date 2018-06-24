/*
	© 2011-2018 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.cpp
\ingroup NBuilder
\brief NPL 解释实现。
\version r6958
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2018-06-25 03:35 +0800
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
#include YFM_YSLib_Service_TextFile // for
//	YSLib::IO::SharedInputMappedFileStream, YSLib::Text::OpenSkippedBOMtream,
//	YSLib::Text::BOM_UTF_8;

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
	return ReductionStatus::Clean;
}

ReductionStatus
DefaultDebugAction(TermNode& term)
{
	if(use_debug)
	{
		YTraceDe(Debug, "List term: %p", ystdex::pvoid(&term));
		LogTermValue(term);
		return ProcessDebugCommand();
	}
	return ReductionStatus::Clean;
}

ReductionStatus
DefaultLeafDebugAction(TermNode& term)
{
	if(use_debug)
	{
		YTraceDe(Debug, "Leaf term: %p", ystdex::pvoid(&term));
		LogTermValue(term);
		return ProcessDebugCommand();
	}
	return ReductionStatus::Clean;
}
//@}


/// 801
template<typename _tNode, typename _fCallable>
ReductionStatus
ListCopyOrMove(TermNode& term, _fCallable f)
{
	Forms::CallUnary([&](_tNode& node){
		term.GetContainerRef() = node.CreateWith(f);
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
	const auto p_sifs(Text::OpenSkippedBOMtream<
		IO::SharedInputMappedFileStream>(Text::BOM_UTF_8, name.c_str()));
	std::istream& is(*p_sifs);

	if(is)
	{
		YTraceDe(Notice, "Test unit '%s' found.", name.c_str());
		FilterExceptions([&]{
			TryLoadSouce(context, name.c_str(), is, ctx);
		});
	}
	else
		YTraceDe(Notice, "Test unit '%s' not found.", name.c_str());
}

/// 802
void
LoadExternalRoot(REPLContext& context, const string& name)
{
	LoadExternal(context, name, context.Root);
}

/// 797
ArgumentsVector CommandArguments;


/// 740
void
LoadFunctions(REPLContext& context)
{
	using namespace std::placeholders;
	using namespace Forms;
	auto& root(context.Root);
	auto& root_env(root.GetRecordRef());

	p_context = make_observer(&context);
	LoadNPLContextForSHBuild(context);
	// TODO: Extract literal configuration API.
	{
		// TODO: Blocked. Use C++14 lambda initializers to simplify
		//	implementation.
		auto lit_base(std::move(root.EvaluateLiteral.begin()->second));
		auto lit_ext(FetchExtendedLiteralPass());

		root.EvaluateLiteral = [lit_base, lit_ext](TermNode& term,
			ContextNode& ctx, string_view id) -> ReductionStatus{
			const auto res(lit_ext(term, ctx, id));

			if(res == ReductionStatus::Clean
				&& term.Value.type() == ystdex::type_id<TokenValue>())
				return lit_base(term, ctx, id);
			return res;
		};
	}
	// NOTE: Literal builtins.
	RegisterLiteralSignal(root, "exit", SSignal::Exit);
	RegisterLiteralSignal(root, "cls", SSignal::ClearScreen);
	RegisterLiteralSignal(root, "about", SSignal::About);
	RegisterLiteralSignal(root, "help", SSignal::Help);
	RegisterLiteralSignal(root, "license", SSignal::License);
	// NOTE: Definition of %inert is in %YFramework.NPL.Dependency.
	// NOTE: Context builtins.
	root_env.Define("REPL-context", ValueObject(context, OwnershipTag<>()), {});
	root_env.Define("root-context", ValueObject(root, OwnershipTag<>()), {});
	// NOTE: Literal expression forms.
	RegisterForm(root, "$retain", Retain);
	RegisterForm(root, "$retain1", ystdex::bind1(RetainN, 1));
#if true
	// NOTE: Primitive features, listed as RnRK, except mentioned above. See
	//	%YFramework.NPL.Dependency.
	// NOTE: Definitions of eq?, eql?, eqr?, eqv? are in
	//	%YFramework.NPL.Dependency.
	RegisterStrictUnary<const string>(root, "symbol-string?", IsSymbol);
	// NOTE: Definitions of $if is in %YFramework.NPL.Dependency.
	RegisterStrictUnary(root, "list?", ComposeReferencedTermOp(IsList));
	RegisterStrictUnary(root, "listpr?", IsList);
	// TODO: Add nonnull list predicate to improve performance?
	// NOTE: Definitions of null?, cons, cons&, eval, copy-environment,
	//	make-environment, get-current-environment, weaken-environment,
	//	lock-environment are in %YFramework.NPL.Dependency.
	// NOTE: Environment mutation is optional in Kernel and supported here.
	// NOTE: Definitions of $deflazy!, $def!, $defrec! are in
	//	%YFramework.NPL.Dependency.
	RegisterForm(root, "$undef!", ystdex::bind1(Undefine, _2, true));
	RegisterForm(root, "$undef-checked!", ystdex::bind1(Undefine, _2, false));
	// NOTE: Definitions of $vau, $vaue, wrap are in %YFramework.NPL.Dependency.
	// NOTE: The applicative 'wrap1' does check before wrapping.
	RegisterStrictUnary<const ContextHandler>(root, "wrap1", WrapOnce);
	// NOTE: Definitions of unwrap is in %YFramework.NPL.Dependency.
#endif
	// NOTE: NPLA value transferring.
	RegisterStrictUnary(root, "vcopy", [](const TermNode& node){
		return node.Value.MakeCopy();
	});
	RegisterStrictUnary(root, "vcopymove", [](TermNode& node){
		// NOTE: Shallow copy or move.
		return node.Value.CopyMove();
	});
	RegisterStrictUnary(root, "vmove", [](const TermNode& node){
		return node.Value.MakeMove();
	});
	RegisterStrictUnary(root, "vmovecopy", [](const TermNode& node){
		return node.Value.MakeMoveCopy();
	});
	RegisterStrict(root, "lcopy", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeCopy);
	});
	RegisterStrict(root, "lcopymove", [](TermNode& term){
		return ListCopyOrMove<TermNode>(term, &ValueObject::CopyMove);
	});
	RegisterStrict(root, "lmove", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeMove);
	});
	RegisterStrict(root, "lmovecopy", [](TermNode& term){
		return ListCopyOrMove<const TermNode>(term, &ValueObject::MakeMoveCopy);
	});
	RegisterStrict(root, "tcopy", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeCopy);
	});
	RegisterStrict(root, "tcopymove", [](TermNode& term){
		return TermCopyOrMove<TermNode>(term, &ValueObject::CopyMove);
	});
	RegisterStrict(root, "tmove", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeMove);
	});
	RegisterStrict(root, "tmovecopy", [](TermNode& term){
		return TermCopyOrMove<const TermNode>(term, &ValueObject::MakeMoveCopy);
	});
	// XXX: For test or debug only.
	RegisterStrictUnary(root, "tt", DefaultDebugAction);
	RegisterStrictUnary<const string>(root, "dbg", [](const string& cmd){
		if(cmd == "on")
			use_debug = true;
		else if(cmd == "off")
			use_debug = {};
		else if(cmd == "crash")
			terminate();
	});
	RegisterForm(root, "$crash", []{
		terminate();
	});
	// NOTE: Derived functions with probable privmitive implementation.
	// NOTE: Definitions of id, idv, list, list&, $quote, $defl!, first, first%,
	//first&, rest, rest%, rest& are in
	//	%YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! xcons (&x &y) cons y x;
		$defl! xcons% (&x &y) cons% y x;
	)NPL");
	// NOTE: Definitions of $set!, $defv!, $lambda, $setrec!, $defl!, first,
	//	rest, apply, list*, $defw!, $lambdae, $sequence, $cond,
	//	make-standard-environment, not?, $when, $unless are in
	//	%YFramework.NPL.Dependency.
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
	//	list-rest, accl, accr are in %YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defl! foldl1 (&kons &knil &l) accl l null? knil first rest kons;
		$defw! map1-reverse (&appv &l) env foldl1
			($lambda (&x &xs) cons (apply appv (list x) env) xs) () l;
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
			((null? (rest ls)) first ls)
			(#t bin (first ls) (reduce (rest ls) bin id));
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
	// NOTE: Definitions of $let* are in
	//	%YFramework.NPL.Dependency.
	context.Perform(u8R"NPL(
		$defv%! $letrec (&bindings .&body) env forward
			(eval% (list $let () $sequence (list% $def! (map1 first% bindings)
				(list*% () list (map1 rest% bindings))) body) env);
		$defv%! $letrec* (&bindings .&body) env forward
			(eval% ($if (null? bindings) (list*% $letrec bindings body)
				(list $letrec (list% (first% bindings))
				(list*% $letrec* (rest% bindings) body))) env);
		$defv! $let-redirect (&expr &bindings .&body) env
			eval (list* () (eval (list* $lambda (map1 first bindings) body)
				(eval expr env)) (map1 list-rest bindings)) env;
		$defv! $let-safe (&bindings .&body) env
			eval (list* () $let-redirect
				(() make-standard-environment) bindings body) env;
		$defv! $remote-eval (&o &e) d eval o (eval e d);
		$defv! $bindings->environment &bindings denv
			eval (list $let-redirect (() make-environment) bindings
				(list lock-environment (list () get-current-environment))) denv;
		$defv! $provide! (&symbols .&body) env
			eval (list $def! symbols
				(list $let () $sequence body (list* list symbols))) env;
		$defv! $import! (&expr .&symbols) env
			eval (list $set! env symbols (cons list symbols)) (eval expr env);
		$def! foldr $let ((&cenv () make-standard-environment)) wrap
		(
			$set! cenv cxrs $lambdae (weaken-environment cenv) (ls cxr)
				accr ls null? () ($lambda (&l) cxr (first l)) rest cons;
			$vaue cenv (kons knil .ls) env
				(accr ls unfoldable? knil ($lambda (&ls) cxrs ls first)
				($lambda (&ls) cxrs ls rest) ($lambda (&x &st)
					apply kons (list-concat x (list st)) env))
		);
		$def! map $let ((&cenv () make-standard-environment)) wrap
		(
			$set! cenv cxrs $lambdae (weaken-environment cenv) (ls cxr)
				accr ls null? () ($lambda (&l) cxr (first l)) rest cons;
			$vaue cenv (appv .ls) env accr ls unfoldable? ()
				($lambda (&ls) cxrs ls first) ($lambda (&ls) cxrs ls rest)
					($lambda (&x &xs) cons (apply appv x env) xs)
		);
		$defw! for-each-rtl &ls env $sequence (apply map ls env) inert;
	)NPL");
	// NOTE: Definitions of unfoldable?, map-reverse for-each-ltr
	//	are in %YFramework.NPL.Dependency.
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
	// NOTE: Object interoperation.
	// NOTE: Definitions of ref is in %YFramework.NPL.Dependency.
	// NOTE: Environments.
	// NOTE: Definitions of bound?, value-of is in %YFramework.NPL.Dependency.
	// NOTE: Only '$binds?' is like in Kernel.
	context.Perform(u8R"NPL(
		$defw! environment-bound? (&expr &str) env
			eval (list bound? str) (eval expr env);
		$defv! $binds1? (&expr &s) env
			eval (list (unwrap bound?) (symbol->string s)) (eval expr env);
		$defv! $binds? (&expr .&ss) env $let ((&senv eval expr env))
			foldl1 $and? #t (map1 ($lambda (s) (wrap $binds1?) senv s) ss);
	)NPL");
	RegisterStrict(root, "eval-u",
		ystdex::bind1(EvaluateUnit, std::ref(context)));
	RegisterStrict(root, "eval-u-in", [](TermNode& term){
		const auto i(std::next(term.begin()));
		const auto& rctx(Access<REPLContext>(Deref(i)));

		term.Remove(i);
		EvaluateUnit(term, rctx);
	}, ystdex::bind1(RetainN, 2));
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
	RegisterStrictUnary(root, "typeid", [](const TermNode& term){
		// FIXME: Get it work with %YB_Use_LightweightTypeID.
		return std::type_index(ReferenceTerm(term).Value.type());
	});
	// TODO: Copy of operand cannot be used for move-only types.
	context.Perform("$defl! ptype (&x) puts (nameof (typeid x))");
	RegisterStrictUnary<const string>(root, "get-typeid",
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
	RegisterStrictUnary<const ContextHandler>(root, "get-combiner-type",
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
	RegisterStrictUnary(root, "list-length",
		ComposeReferencedTermOp(FetchListLength));
	RegisterStrictUnary(root, "listv-length", FetchListLength);
	RegisterStrictUnary(root, "branch?", ComposeReferencedTermOp(IsBranch));
	RegisterStrictUnary(root, "branchpr?", IsBranch);
	RegisterStrictUnary(root, "leaf?", ComposeReferencedTermOp(IsLeaf));
	RegisterStrictUnary(root, "leafpr?", IsLeaf);
	// NOTE: String library.
	// NOTE: Definitions of ++, string-empty?, string<- are in
	//	%YFramework.NPL.Dependency.
	RegisterStrictBinary<string>(root, "string=?", ystdex::equal_to<>());
	context.Perform(u8R"NPL($defl! retain-string (&str) ++ "\"" str "\"")NPL");
	RegisterStrictUnary<const int>(root, "itos", [](int x){
		return to_string(x);
	});
	RegisterStrictUnary<const string>(root, "string-length",
		[&](const string& str) ynothrow{
		return int(str.length());
	});
	// NOTE: Definitions of string->symbol, symbol->string, string->regex,
	//	regex-match? are in %YFramework.NPL.Dependency.
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
	RegisterStrict(root, "display", ystdex::bind1(LogTermValue, Notice));
	RegisterStrictUnary<const string>(root, "echo", Echo);
	RegisterStrictUnary<const string>(root, "load",
		std::bind(LoadExternalRoot, std::ref(context), _1));
	RegisterStrictUnary<const string>(root, "load-at-root",
		[&](const string& name){
		const ystdex::guard<EnvironmentSwitcher>
			gd(root, root.SwitchEnvironment(root_env.shared_from_this()));

		LoadExternalRoot(context, name);
	});
	context.Perform(u8R"NPL(
		$defl! get-module (&filename .&opt)
			$let ((&env () make-standard-environment)) $sequence
				($unless (null? opt) ($set! env module-parameters (first opt)))
				(eval (list load filename) env) env;
	)NPL");
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
	// NOTE: Definitions of env-get, system
	//	are in %YFramework.NPL.Dependency.
	// NOTE: Definition of env-set, cmd-get-args, system-get are also in
	//	%Tools.SHBuild.Main.
	RegisterStrictBinary<const string>(root, "env-set",
		[&](const string& var, const string& val){
		SetEnvironmentVariable(var.c_str(), val.c_str());
	});
	RegisterStrict(root, "cmd-get-args", [](TermNode& term){
		term.Clear();
		for(const auto& s : CommandArguments.Arguments)
			term.AddValue(MakeIndex(term), s);
		return ReductionStatus::Retained;
	});
	RegisterStrict(root, "system-get", [](TermNode& term){
		CallUnaryAs<const string>([&](const string& cmd){
			auto res(FetchCommandOutput(cmd.c_str()));

			term.Clear();
			term.AddValue(MakeIndex(0), ystdex::trim(std::move(res.first)));
			term.AddValue(MakeIndex(1), res.second);
		}, term);
		return ReductionStatus::Retained;
	});
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
	root.EvaluateList.Add(DefaultDebugAction, 255);
	root.EvaluateLeaf.Add(DefaultLeafDebugAction, 255);
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
	CommandArguments.Reset(argc, argv);
	return FilterExceptions([]{
		Application app;
		Interpreter intp(app, LoadFunctions);

		while(intp.WaitForLine() && intp.Process())
			;
	}, "::main") ? EXIT_FAILURE : EXIT_SUCCESS;
}

