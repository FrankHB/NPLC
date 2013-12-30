/*
	© 2012-2013 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.cpp
\ingroup Adaptor
\brief NPL 上下文。
\version r?1289
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2013-12-27 10:42 +0800
	2013-12-30 11:05 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#include "NPLContext.h"
#include YFM_NPL_SContext
#include <ystdex/container.hpp>
#include <iostream>

namespace NPL
{

NPLContext::NPLContext(const FunctionMap& fproto)
	: Root(), Map(fproto), token_list(), sem()
{}
#if 0
NPLContext::NPLContext(const NPLContext& proto, TokenList new_token_list)
	: Root(), Map(proto.Map), token_list(new_token_list), sem()
{}
#endif

TLIter
NPLContext::Call(TLIter b, TLIter e, size_t& cur_off)
{
	yconstraint(cur_off != 0),
	yconstraint(b != e);
	yconstraint(*b != "("),
	yconstraint(*b != ")");

	const auto ib(b++);

	yconstraint(*b != "("),
	yconstraint(*b != ")");

	const auto& fn(*ib);
	const auto& arg(*b);

	if(fn == "()" && arg == "()")
	{
		--cur_off;
		return token_list.erase(ib);
	}
	else
	{
		const auto it(Map.find(fn));

		if(it != Map.end())
			it->second(arg);
		else
			goto match_fail;
		cur_off -= std::distance(ib, e) - 1;
		return token_list.erase(ib, e);
	}
match_fail:
	return std::next(ib, 1);
//	throw LoggedEvent("No matching function found.", 0xC0);
}

/// 328
void
NPLContext::CallS(const string& fn, const string& arg, size_t& cur_off)
{
	using namespace std;

	if(!fn.compare(0, 4, "$__+"))
		cout << "XXX" << arg << endl;
	else
		cout << "YYY" << fn << ":" << arg << endl;
}

void
NPLContext::Eval(const string& arg)
{
	if(CheckLiteral(arg) == '\'')
		NPLContext(Map).Perform(ystdex::get_mid(arg));
#if 0
	{
		NPLContext new_context(*this, MangleToken(ystdex::get_mid(arg)));
		const auto b(new_context.token_list.begin());
		const auto e(new_context.token_list.end());

		if(new_context.Reduce(0, b, e, false).first != e)
			throw LoggedEvent("eval: Redundant ')' found.", 0x20);
		auto res(new_context.Reduce(0, b, e, true));

		yassume(res.first == e);
	}
#endif
}

ValueObject
NPLContext::FetchValue(const ValueNode& ctx, const string& name)
{
	static_cast<void>(ctx);
	static_cast<void>(name);

	return ValueObject();
}

void
NPLContext::HandleIntrinsic(const string& cmd)
{
	if(cmd == "exit")
		throw SSignal(SSignal::Exit);
	if(cmd == "cls")
		throw SSignal(SSignal::ClearScreen);
	if(cmd == "about")
		throw SSignal(SSignal::About);
	if(cmd == "help")
		throw SSignal(SSignal::Help);
	if(cmd == "license")
		throw SSignal(SSignal::License);
}

#define NPL_TRACE 1

/// 327
/// \pre b and e shall be iterator in or one-past-end of token_list.
/// \pre b shall be dereferanceable when e is dereferanceable.
pair<TLIter, size_t>
NPLContext::Reduce(size_t depth, TLIter b, TLIter e, bool eval)
{
	size_t cur_off(0);
	TLIter ifn(e);

	while(b != e && *b != ")")
	{
		if(*b == "(")
		{
			const auto ileft(b);
			auto res(Reduce(depth + 1, ++b, e, eval));

			if(res.first == e || *res.first != ")")
				throw LoggedEvent("Redundant '(' found.", Alert);
			if(res.second < 2)
			{
				token_list.erase(res.first);
				b = token_list.erase(ileft);
				if(res.second == 0)
					b = token_list.insert(b, "()");
			}
			else
				yunseq(b = ++res.first, ++cur_off);
		}
		else if(*b == ";" || *b == ",")
			yunseq(ifn = e, ++b, ++cur_off);
		else if(eval)
		{
			if(!sem.empty())
			{
				CallS(sem, *b, cur_off);
				sem.clear(),
				yunseq(++b, ifn = e);
			}
			else if(ifn == e)
			{
				if(CheckLiteral(*b) == '\0')
					ifn = b++;
				else
					++b;
				++cur_off;
			}
			else
			{
				b = Call(ifn, ++b, cur_off);
				ifn = e;
			}
		}
		else
			yunseq(++b, ++cur_off);
#if !NDEBUG && NPL_TRACE
		using namespace std;

		cout << "[depth:cur_off] = [ " << depth << " : "
			<< cur_off << " ];" << endl << "TokenList: ";
		for(const auto& s : token_list)
			cout << s << ' ';
		cout << endl;
#endif
	}
	return make_pair(b, cur_off);
}

void
NPLContext::ReduceS(size_t depth, ValueNode ctx, const ValueNode& sema)
{
	++depth;
	if(auto p = sema.GetContainerPtr())
	{
		const auto n(p->size());

		if(n == 0)
			//空表。
			sema.Value = ValueToken::Null;
		else if(n == 1)
		{
			//单元素表化简。
			sema.Value = std::move(p->begin()->Value);
			ReduceS(depth, std::move(ctx), sema);
		}
		else
		{
			//表应用。
			for(auto& term : *p)
				ReduceS(depth, ctx, term);
			ystdex::erase_all_if(*p, p->begin(), p->end(),
				[](const ValueNode& term){
					return !term;
			});
			//函数匹配。
			try
			{
				auto i(p->begin());
				const auto fn(i->Value.Access<string>());

				++i;
				if(fn == "$lambda")
				{
					std::cout << "lambda with param.n=:" << n << std::endl;
				}
				else if(n == 3 && fn == "+")
				{
					const auto e1(std::stoi(i->Value.Access<string>()));
					++i;
					const auto e2(std::stoi(i->Value.Access<string>()));
					++i;

					sema.Value = to_string(e1 + e2);
					return;
				}
			//	else
				//	throw LoggedEvent("No matching functions found.", Warning);
				return;
			}
			catch(ystdex::bad_any_cast&)
			{}
			throw LoggedEvent("Mismatched types found.", Warning);
		}
	}
	else if(sema.Value.AccessPtr<ValueToken>())
		;
	else if(auto p = sema.Value.AccessPtr<string>())
	{
		if(*p == "," || *p == ";")
			sema.Value.Clear();
		else
		{
			HandleIntrinsic(*p);
			//值重写。
			if(auto v = FetchValue(ctx, *p))
				sema.Value = std::move(v);
		//	else
		//		throw LoggedEvent((string("Unknown name found: '")
		//			+ *p + "'.").c_str(), Warning);
		}
	}
}

namespace
{

inline std::ostream&
WritePrefix(std::ostream& f, size_t n = 1, char c = '\t')
{
	while(n--)
		f << c;
	return f;
}

string
EscapeNodeString(const string& str)
{
	const char c(CheckLiteral(str));
	auto content(MakeEscape(c == char() ? str : ystdex::get_mid(str)));

	return c == char() ? std::move(content) : c + content + c;
}

std::ostream&
WriteNodeC(std::ostream& f, const ValueNode& node, size_t depth)
{
	WritePrefix(f, depth);
	f << node.GetName();
	if(node)
	{
		try
		{
			const auto& s(Access<string>(node));

			f << ' ' << '"' << EscapeNodeString(s) << '"' << '\n';
			return f;
		}
		catch(ystdex::bad_any_cast&)
		{}
		f << '\n';
		for(const auto& n : node)
		{
			WritePrefix(f, depth);
			f << '(' << '\n';
			try
			{
				WriteNodeC(f, n, depth + 1);
			}
			catch(std::out_of_range&)
			{}
			WritePrefix(f, depth);
			f << ')' << '\n';
		}
	}
	return f;
}

void
ConvTokens(const ValueNode& sema)
{
	using namespace std;

	WriteNodeC(cout, sema, 0);
	cout << endl;
}

}

TokenList&
NPLContext::Perform(const string& unit)
{
#if 1
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	auto sema(SContext::Analyze(Session(unit)));

	ReduceS(0, Root, sema);

	ConvTokens(sema);
	return token_list;
#else
	Session session(unit);

	if((token_list = session.GetTokenList()).size() == 1)
		HandleIntrinsic(token_list.front());
	if(token_list.empty())
		throw LoggedEvent("Empty token list found;", Alert);
	if(Reduce(0, token_list.begin(), token_list.end(), false).first
		!= token_list.end())
		throw LoggedEvent("Redundant ')' found.", Alert);

	const auto res(Reduce(0, token_list.begin(), token_list.end(), true));

	yassume(res.first == token_list.end());

	return token_list;
#endif
}

} // namespace NPL;

