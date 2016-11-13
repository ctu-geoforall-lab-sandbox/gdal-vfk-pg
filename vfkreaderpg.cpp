/******************************************************************************
 *  * $Id: vfkreaderpg.cpp 33713 2016-03-12 17:41:57Z goatbar $
 *  *
 *  * Project:  VFK Reader (PG)
 *  * Purpose:  Implements VFKReaderPG class.
 *  * Author:   Martin Landa, landa.martin gmail.com
 *  *
 *  ******************************************************************************
 *  * Copyright (c) 2012-2014, Martin Landa <landa.martin gmail.com>
 *  * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
 *  *
 *  * Permission is hereby granted, free of charge, to any person
 *  * obtaining a copy of this software and associated documentation
 *  * files (the "Software"), to deal in the Software without
 *  * restriction, including without limitation the rights to use, copy,
 *  * modify, merge, publish, distribute, sublicense, and/or sell copies
 *  * of the Software, and to permit persons to whom the Software is
 *  * furnished to do so, subject to the following conditions:
 *  *
 *  * The above copyright notice and this permission notice shall be
 *  * included in all copies or substantial portions of the Software.
 *  *
 *  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 *  * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 *  * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  * SOFTWARE.
 *  ****************************************************************************/

#include "cpl_vsi.h"

#include "vfkreader.h"
#include "vfkreaderpg.h"

#include "cpl_conv.h"
#include "cpl_error.h"

#include <cstring>

#include "ogr_geometry.h"

CPL_CVSID("$Id: vfkreadersqlite.cpp 35933 2016-10-25 16:46:26Z goatbar $");

/*!
 *   \brief VFKReaderPG constructor
 * */
VFKReaderPG::VFKReaderPG(const char *pszFileName) : VFKReaderDB(pszFileName)
{
}

/*!
  \brief VFKReaderSQLite destructor
*/
VFKReaderPG::~VFKReaderPG()
{
}
