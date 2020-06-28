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
\version r332
\author FrankHB <frankhb1989@gmail.com>
\since YSLib build 403
\par 创建时间:
	2013-05-09 17:23:17 +0800
\par 修改时间:
	2020-06-29 00:37 +0800
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

//! \since YSLib build 592
using namespace YSLib::Consoles;
//! \since YSLib build 674
using YSLib::Logger;
//! \since YSLib build 740
using A1::REPLContext;

//! \since YSLib build 304
enum class SSignal
{
	Exit,
	ClearScreen,
	About,
	Help,
	License
};


//! \since YSLib build 867
//@{
using namespace ystdex::pmr;

/*!
\brief 共享池资源。

类似 ystdex::pool_resource 的池资源，但内置预分配策略和不同的初始区块容量。
预分配策略决定对小于内部阈值的分配和去配，忽略池选项，总是使用初始化时预分配的池；
	这种池中的区块称为快速区块。
设快速区块的大小上限 max_fast_block_size ，若不忽略池选项，这相当于假定：
<tt>ystdex::resource_pool::adjust_for_block(bytes, alignment)
	<= max_fast_block_size
</tt>
总是蕴含：
<tt>bytes <= options.largest_required_pool_block</tt> 。
其中 \c bytes 和 \c alignment 是分配参数，\c options 是指定的池选项。
初始区块容量在每个池的分配时决定，使小于快速区块的块的起始分配具有较
	\c ystdex::resource_pool::default_capacity 大的值而减小分配。
*/
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


//! \since YSLib build 673
void
LogTree(const ValueNode&, Logger::Level = YSLib::Debug);

//! \since YSLib build 803
void
LogTermValue(const TermNode&, Logger::Level = YSLib::Debug);

/*!
\build 控制台默认颜色。
\since YSLib build 327
*/
yconstexpr const Color DefaultColor(Gray), TitleColor(Cyan),
	InfoColor(White), ErrorColor(Red), PromptColor(DarkGreen),
	SignalColor(DarkRed), SideEffectColor(Yellow), ReducedColor(Magenta);


/*!
\since YSLib build 304
\warning 非虚析构。
*/
class Interpreter
{
private:
	//! \since YSLib build 755
	platform_ex::Terminal terminal;
	//! \since YSLib build 674
	YSLib::RecordLevel err_threshold;
	//! \since YSLib build 867
	shared_pool_resource pool_rsrc;
	//! \since YSLib build 674
	string line;
	//! \since YSLib build 834
	shared_ptr<Environment> p_ground{};

public:
	//! \since YSLib build 885
	REPLContext Context;
	//! \since YSLib build 892
	TermNode Term{Context.Allocator};

	//! \since YSLib build 885
	Interpreter();

	//! \since YSLib build 883
	void
	EnableExtendedLiterals();

	void
	HandleSignal(SSignal);

	//! \since YSLib build 894
	//@{
	ReductionStatus
	Load(TermNode&, ContextNode&, string&&, std::istream&);

private:
	void
	Perform(string_view, ContextNode::ReducerSequence&);

	void
	PerformAndFilter(string_view);

	void
	ProcessTerm(TermNode&, ContextNode::ReducerSequence&);

public:
	//! \since YSLib build 892
	void
	Run();

	void
	RunLine(string_view);
	//@}

private:
	//! \since YSLib build 892
	ReductionStatus
	RunLoop(ContextNode&);

public:
	//! \since YSLib build 834
	bool
	SaveGround();

	std::istream&
	WaitForLine();
	//! \since YSLib build 696
	std::istream&
	WaitForLine(std::istream&, std::ostream&);

	//! \since YSLib build 885
	PDefH(void, UpdateTextColor, Color c)
		ImplExpr(terminal.UpdateForeColor(c))
};

} // namespace NPL;

#endif

