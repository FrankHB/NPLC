/*
	© 2012-2017 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.h
\ingroup NBuilder
\brief NPL 解释实现。
\version r2060
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-04-23 15:25:02 +0800
\par 修改时间:
	2017-01-03 15:27 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#ifndef INC_NPL_NBuilder_h_
#define INC_NPL_NBuilder_h_

#include "Interpreter.h"

namespace NPL
{

/// 674
using namespace YSLib;

namespace A1
{

/// 674
void
RegisterLiteralSignal(ContextNode&, const string&, SSignal);

} // namespace A1;

} // namespace NPL;

#endif


