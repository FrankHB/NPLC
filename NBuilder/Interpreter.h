/*
	© 2013-2020 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file Interpreter.h
\ingroup NBuilder
\brief NPL 解释器。
\version r226
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2020-02-02 06:01 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::Interpreter
*/


#ifndef INC_NPL_Interpreter_h_
#define INC_NPL_Interpreter_h_

#include <Helper/YModules.h>
#include "NPLContext.h"
#include YFM_YSLib_Core_YConsole
#include YFM_Helper_Environment
#include <iosfwd>
#include <functional>

namespace NPL
{

/// 592
using namespace YSLib::Consoles;
/// 674
using YSLib::Logger;
/// 740
using A1::REPLContext;

/// 304
enum class SSignal
{
	Exit,
	ClearScreen,
	About,
	Help,
	License
};


/// 867
//@{
using namespace ystdex::pmr;

class shared_pool_resource : public memory_resource
{
private:
	using pools_t
		= std::vector<resource_pool, polymorphic_allocator<resource_pool>>;

	pool_options saved_options;
	oversized_map oversized;
	pools_t pools;

public:
	/*!
	\note 实现定义：参见 adjust_pool_options 的调整的值。
	\sa adjust_pool_options
	*/
	//@{
	shared_pool_resource() ynothrow
		: shared_pool_resource(pool_options(), get_default_resource())
	{}
	//! \pre 断言：指针参数非空。
	YB_NONNULL(3)
	shared_pool_resource(const pool_options&, memory_resource*) ynothrow;
	//! \pre 间接断言：指针参数非空。
	explicit
	shared_pool_resource(memory_resource* upstream)
		: shared_pool_resource(pool_options(), upstream)
	{}
	explicit
	shared_pool_resource(const pool_options& opts)
		: shared_pool_resource(opts, get_default_resource())
	{}
	//@}
	~shared_pool_resource() override = default;

	void
	release() yimpl(ynothrow);

	YB_ATTR_nodiscard YB_ATTR_returns_nonnull YB_PURE memory_resource*
	upstream_resource() const yimpl(ynothrow)
	{
		return pools.get_allocator().resource();
	}

	YB_ATTR_nodiscard YB_PURE pool_options
	options() const yimpl(ynothrow)
	{
		return saved_options;
	}

protected:
	YB_ALLOCATOR void*
	do_allocate(size_t, size_t) override;

	void
	do_deallocate(void*, size_t, size_t) yimpl(ynothrowv) override;

	YB_ATTR_nodiscard yimpl(YB_STATELESS) bool
	do_is_equal(const memory_resource&) const ynothrow override;

private:
	pools_t::iterator
	emplace(pools_t::const_iterator, size_t);

	YB_ATTR_nodiscard YB_PURE std::pair<pools_t::iterator, size_t>
	find_pool(size_t) ynothrow;
};
//@}


/// 673
void
LogTree(const ValueNode&, Logger::Level = YSLib::Debug);

/// 803
void
LogTermValue(const TermNode&, Logger::Level = YSLib::Debug);

/*!
\build 控制台默认颜色。
\since YSLib build 327
*/
yconstexpr const Color DefaultColor(Gray), TitleColor(Cyan),
	InfoColor(White), ErrorColor(Red), PromptColor(DarkGreen),
	SignalColor(DarkRed), SideEffectColor(Yellow), ReducedColor(Magenta);


/// 304
class Interpreter
{
private:
	/// 755
	platform_ex::Terminal terminal;
	/// 674
	YSLib::RecordLevel err_threshold;
	/// 867
	shared_pool_resource pool_rsrc;
	/// 689
	YSLib::unique_ptr<YSLib::Environment> p_env;
	/// 674
	string line;
	/// 834
	shared_ptr<Environment> p_ground{};
	/// 740
	REPLContext context;

public:
	/// 740
	Interpreter(YSLib::Application&, std::function<void(REPLContext&)>);

	void
	HandleSignal(SSignal);

	bool
	Process();

	/// 834
	bool
	SaveGround();

	std::istream&
	WaitForLine();
	/// 696
	std::istream&
	WaitForLine(std::istream&, std::ostream&);
};

} // namespace NPL;

#endif

