/*
	© 2012-2016 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.h
\ingroup NBuilder
\brief NPL 解释实现。
\version r1887
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-04-23 15:25:02 +0800
\par 修改时间:
	2016-02-15 16:16 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#ifndef INC_NPL_NBuilder_h_
#define INC_NPL_NBuilder_h_

#include "NPLContext.h"
#include YFM_YSLib_Core_YString

namespace NPL
{

/// 673
//@{
using YSLib::ValueObject;


YB_NORETURN void
ThrowArityMismatch(size_t, size_t);


void
ReduceHead(TermNode::Container&) ynothrowv;
inline PDefH(void, ReduceHead, TermNode& term) ynothrowv
	ImplExpr(ReduceHead(term.GetContainerRef()))


ValueNode
TransformForSeperator(const TermNode&, const ValueObject&, const ValueObject&,
	const string& = {});

ValueNode
TransformForSeperatorRecursive(const TermNode&, const ValueObject&,
	const ValueObject&, const string& = {});
//@}

} // namespace NPL;

#endif

