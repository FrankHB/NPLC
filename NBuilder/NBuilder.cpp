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
\version r5028
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 301
\par 创建时间:
	2011-07-02 07:26:21 +0800
\par 修改时间:
	2016-05-31 11:03 +0800
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

namespace NPL
{

namespace A1
{

bool
ExtractModifier(TermNode::Container& con, const ValueObject& mod)
{
	if(!con.empty())
	{
		const auto i(con.cbegin());

		if(const auto p = AccessPtr<string>(Deref(i)))
		{
			if(*p == mod)
			{
				con.erase(i);
				return true;
			}
		}
	}
	return {};
}

void
ReduceTail(TermNode& term, ContextNode& ctx, TNIter i)
{
	auto& con(term.GetContainerRef());

	con.erase(con.begin(), i);
	Reduce(term, ctx);
}

void
RemoveHeadAndReduceAll(TermNode& term, ContextNode& ctx)
{
	RemoveHead(term.GetContainerRef());
	std::for_each(term.begin(), term.end(),
		std::bind(Reduce, std::placeholders::_1, std::ref(ctx)));
}

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
	using std::bind;
	using namespace std::placeholders;
	auto& root(context.Root);

	context.ListTermPreprocess += std::bind(ReplaceTermForSeperator, _1,
		string("$;"), string(";"));
	context.ListTermPreprocess += std::bind(ReplaceTermForSeperator, _1,
		string("$,"), string(","));
	RegisterContextHandler(root, "$;",
		FormContextHandler(RemoveHeadAndReduceAll));
	RegisterContextHandler(root, "$,",
		FormContextHandler(RemoveHeadAndReduceAll));
	RegisterLiteralSignal(root, "exit", SSignal::Exit);
	RegisterLiteralSignal(root, "cls", SSignal::ClearScreen);
	RegisterLiteralSignal(root, "about", SSignal::About);
	RegisterLiteralSignal(root, "help", SSignal::Help);
	RegisterLiteralSignal(root, "license", SSignal::License);
	RegisterContextHandler(root, "$quote",
		FormContextHandler([](TermNode& term){
		YAssert(!term.empty(), "Invalid term found.");
	}));
	RegisterContextHandler(root, "$quote1",
		FormContextHandler([](TermNode& term){
		YAssert(!term.empty(), "Invalid term found.");

		const auto n(term.size() - 1);

		if(n != 1)
			ThrowArityMismatch(1, n);
	}));
	RegisterContextHandler(root, "$define",
		FormContextHandler([](TermNode& term, ContextNode& ctx){
		auto& con(term.GetContainerRef());

		RemoveHead(con);

		const bool mod(ExtractModifier(con));

		if(con.empty())
			throw InvalidSyntax(ystdex::sfmt(
				"Syntax error in definition, no arguments are found."));

		auto i(con.begin());

		if(const auto p_id = AccessPtr<string>(Deref(i)))
		{
			const auto id(*p_id);

			YTraceDe(Debug, "Found identifier '%s'.", id.c_str());
			if(++i != con.end())
			{
				ReduceTail(term, ctx, i);
				// TODO: Error handling.
				if(mod)
					ctx[id].Value = std::move(term.Value);
				else
				{
					if(!ctx.Add(id, std::move(term.Value)))
						throw LoggedEvent("Duplicate name '" + id + "' found.",
							Err);
				}
			}
			else
			{
				TryExpr(AccessNode(ctx, id))
				catch(std::out_of_range& e)
				{
					if(!mod)
						throw;
				}
				// TODO: Simplify?
				ctx.Remove(id);
			}
			con.clear();
		}
		else
			throw ystdex::unimplemented("List substitution unimplemented.");
	}));
	RegisterContextHandler(root, "$set",
		FormContextHandler([](TermNode& term, ContextNode& ctx){
		auto& con(term.GetContainerRef());

		RemoveHead(con);
		if(con.empty())
			throw InvalidSyntax(ystdex::sfmt(
				"Syntax error in definition, no arguments are found."));

		auto i(con.begin());

		if(const auto p_id = AccessPtr<string>(Deref(i)))
		{
			const auto id(*p_id);

			YTraceDe(Debug, "Found identifier '%s'.", id.c_str());
			if(++i != con.end())
			{
				ReduceTail(term, ctx, i);
				// TODO: Error handling.
				AccessNode(ctx, id).Value = std::move(term.Value);
			}
			else
				throw LoggedEvent("No argument found.");
		}
		else
			throw ystdex::unimplemented("List assignment unimplemented.");
	}));
	RegisterContextHandler(root, "$lambda",
		FormContextHandler([](TermNode& term, ContextNode& ctx){
		auto& con(term.GetContainerRef());
		auto size(con.size());

		YAssert(size != 0, "Invalid term found.");
		if(size > 1)
		{
			auto i(con.begin());
			auto& plist_con((++i)->GetContainerRef());
			// TODO: Blocked. Use C++14 lambda initializers to reduce
			//	initialization cost by directly moving.
			const auto p_params(make_shared<vector<string>>());

			YTraceDe(Debug, "Found lambda abstraction form with %zu"
				" argument(s).", plist_con.size());
			// TODO: Simplify?
			std::for_each(plist_con.begin(), plist_con.end(),
				[&](decltype(*i)& pv){
				p_params->push_back(Access<string>(pv));
			});
			con.erase(con.cbegin(), ++i);

			// FIXME: Cyclic reference to '$lambda' context handler?
			const auto p_ctx(make_shared<ContextNode>(ctx));
			const auto p_closure(make_shared<TermNode>(std::move(con),
				term.GetName(), std::move(term.Value)));

			term.Value = ToContextHandler([=](TermNode& app_term){
				const auto& params(Deref(p_params));
				const auto n_params(params.size());
				// TODO: Optimize for performance.
				auto app_ctx(Deref(p_ctx));
				const auto n_terms(app_term.size());

				YTraceDe(Debug, "Lambda called, with %ld shared context(s),"
					" %zu parameter(s).", p_ctx.use_count(), n_params);
				if(n_terms == 0)
					throw LoggedEvent("Invalid application found.", Alert);

				const auto n_args(n_terms - 1);

				if(n_args == n_params)
				{
					auto i(app_term.begin());

					++i;
					for(const auto& param : params)
					{
						// XXX: Moved.
						app_ctx[param].Value = std::move(i->Value);
						++i;
					}
					YAssert(i == app_term.end(),
						"Invalid state found on passing arguments.");
					// NOTE: Beta reduction.
					// TODO: Why the container should be copied?
					yunseq(
						app_term.GetContainerRef() = p_closure->GetContainer(),
						app_term.Value = p_closure->Value);
					// TODO: Test for normal form?
					// XXX: Term reused.
					// FIXME: Return value?
					A1::Reduce(app_term, app_ctx);
				//	app_term.GetContainerRef().clear();
				}
				else
					ThrowArityMismatch(n_params, n_args);
			});
			con.clear();
		}
		else
			throw InvalidSyntax(
				ystdex::sfmt("Syntax error in lambda abstraction."));
	}));
	// NOTE: Examples.
	// FIXME: Overflow?
	RegisterFunction(root, "+", std::bind(DoIntegerNAryArithmetics<
		ystdex::plus<>>, ystdex::plus<>(), 0, _1));
	// FIXME: Overflow?
	RegisterFunction(root, "add2", std::bind(DoIntegerBinaryArithmetics<
		ystdex::plus<>>, ystdex::plus<>(), _1));
	// FIXME: Underflow?
	RegisterFunction(root, "-", std::bind(DoIntegerBinaryArithmetics<
		ystdex::minus<>>, ystdex::minus<>(), _1));
	// FIXME: Overflow?
	RegisterFunction(root, "*", std::bind(DoIntegerNAryArithmetics<
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1));
	// FIXME: Overflow?
	RegisterFunction(root, "multiply2", std::bind(DoIntegerBinaryArithmetics<
		ystdex::multiplies<>>, ystdex::multiplies<>(), _1));
	RegisterFunction(root, "/", [](TermNode& term){
		DoIntegerBinaryArithmetics([](int e1, int e2){
			if(e2 != 0)
				return e1 / e2;
			throw std::domain_error("Runtime error: divided by zero.");
		}, term);
	});
	RegisterFunction(root, "%", [](TermNode& term){
		DoIntegerBinaryArithmetics([](int e1, int e2){
			if(e2 != 0)
				return e1 % e2;
			throw std::domain_error("Runtime error: divided by zero.");
		}, term);
	});
	RegisterFunction(root, "system", [](TermNode& term){
		DoUnary([](const string& arg){
			std::system(arg.c_str());
		}, term);
	});
	RegisterFunction(root, "echo", [](TermNode& term){
		DoUnary([](const string& arg){
			if(CheckLiteral(arg) != char())
				std::cout << ystdex::get_mid(arg) << std::endl;
		}, term);
	});
	RegisterFunction(root, "parse", [](TermNode& term){
		DoUnary([](const string& path_gbk)
		{
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
		}, term);
	});
	RegisterFunction(root, "print", [](TermNode& term){
		DoUnary([](const string& path_gbk){
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
		}, term);
	});
	RegisterFunction(root, "mangle", [](TermNode& term){
		DoUnary([](const string& arg){
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
		}, term);
	});
	RegisterFunction(root, "search", [&](TermNode& term){
		DoUnary([&](const string& arg){
			using namespace std;

			try
			{
				const auto& name(arg);
				auto& node(AccessNode(root, name));
				const auto& s(Access<string>(node));

				cout << "Found: " << name << " = " << s << endl;
			}
			CatchExpr(out_of_range&, cout << "Not found." << endl)
		}, term);
	});
	RegisterFunction(root, "eval", [&](TermNode& term){
		DoUnary([&](const string& arg){
			if(CheckLiteral(arg) == '\'')
				NPLContext(context).Perform(ystdex::get_mid(arg));
		}, term);
	});
	RegisterFunction(root, "evals", [](TermNode& term){
		DoUnary([](const string& arg){
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
		}, term);
	});
	RegisterFunction(root, "evalf", [](TermNode& term){
		DoUnary([](const string& arg){
			if(ifstream ifs{DecodeArg(arg)})
			{
				Configuration conf;
				ofstream f("out.txt");

				ifs >> conf;
				f << conf;
			}
			else
				throw LoggedEvent("Invalid file: \"" + arg + "\".", Warning);
		}, term);
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
		try
		{
			Application app;
			Interpreter intp(app, LoadFunctions);

			while(intp.WaitForLine() && intp.Process())
				;
		}
		catch(ystdex::bad_any_cast& e)
		{
			cerr << "Wrong type in any_cast from [" << typeid(e.from()).name()
				<< "] to [" << typeid(e.from()).name() << "]: " << e.what()
				<< endl;
		}
	}, "::main") ? EXIT_FAILURE : EXIT_SUCCESS;
}

