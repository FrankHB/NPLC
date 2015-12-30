/*
	© 2012-2013, 2015 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NPLContext.cpp
\ingroup Adaptor
\brief NPL 上下文。
\version r1300
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 329 。
\par 创建时间:
	2012-08-03 19:55:29 +0800
\par 修改时间:
	2015-12-31 00:44 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NPL::NPLContext
*/


#include "NPLContext.h"
#include YFM_NPL_SContext
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
NPLContext::CallS(const string& fn, const string& arg, size_t&)
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

pair<TLIter, size_t>
NPLContext::Reduce(size_t depth, TLIter b, TLIter e)
{
	size_t cur_off(0);
	TLIter ifn(e);

	while(b != e && *b != ")")
	{
		if(*b == "(")
		{
			const auto ileft(b);
			auto res(Reduce(depth + 1, ++b, e));

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
		else
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

TokenList&
NPLContext::Perform(const string& unit)
{
	if(unit.empty())
		throw LoggedEvent("Empty token list found;", Alert);

	Session session(unit);

	if((token_list = session.GetTokenList()).size() == 1)
		HandleIntrinsic(token_list.front());
	if(SContext::Validate(token_list.begin(), token_list.end())
		!= token_list.end())
		throw LoggedEvent("Redundant ')' found.", Alert);

	const auto res(Reduce(0, token_list.begin(), token_list.end()));

	yassume(res.first == token_list.end());

	return token_list;
}

} // namespace NPL;

