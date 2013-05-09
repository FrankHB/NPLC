/*
	Copyright by FrankHB 2012 - 2013.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file NBuilder.h
\ingroup NBuilder
\brief NPL 解释实现。
\version r1833
\author FrankHB<frankhb1989@gmail.com>
\since YSLib build 304
\par 创建时间:
	2012-04-23 15:25:02 +0800
\par 修改时间:
	2013-05-09 17:32 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	NBuilder::NBuilder
*/


#ifndef INC_NPL_NBuilder_h_
#define INC_NPL_NBuilder_h_

#include "NPLContext.h"
#include <YSLib/Adaptor/ycont.h>
#include <YSLib/Core/ystring.h>

///334
extern NPL::ValueNode GlobalRoot;


YSLib::string
SToMBCS(YSLib::Text::String, int);

YSLib::string
UTF8ToGBK(YSLib::string);

YSL_BEGIN_NAMESPACE(NPL)

///334
void
EvalS(const ValueNode&);
///334
template<typename _type>
void
EvalS(const _type& arg)
{
	EvalS(SContext::Analyze(arg));
}

YSL_END_NAMESPACE(NPL)

#endif

