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
   m_pszConnStr = CPLGetConfigOption("OGR_VFK_DB_NAME", NULL);
   
   m_poDB = PQconnectdb(m_pszConnStr);
   if (PQstatus(m_poDB) != CONNECTION_OK)
   {	
	CPLError(CE_Failure, CPLE_AppDefined,
		 "Connection to database failed: %s",
		 PQerrorMessage(m_poDB));
	PQfinish(m_poDB);
   }
   
}

/*!
  \brief VFKReaderSQLite destructor
*/
VFKReaderPG::~VFKReaderPG()
{
   PQfinish(m_poDB);
}

OGRErr VFKReaderSQLite::ExecuteSQL(PGresult *hStmt)
{
   int rc;
   
   // assert
    
   rc = PQntuples(hStmt); //rc = sqlite3_step(hStmt);
   
   /*
   if (rc != SQLITE_ROW) { // TODO:
      if (rc == SQLITE_DONE) {
	 // sqlite3_finalize(hStmt); // TODO:
	 return OGRERR_NOT_ENOUGH_DATA;
      }
      CPLError(CE_Failure, CPLE_AppDefined,
	       "In ExecuteSQL(): sqlite3_step:\n  %s",
	       PQerrorMessage(m_poDB));
      if (hStmt)
	// sqlite3_finalize(hStmt); // TODO:
	return OGRERR_FAILURE;
   }
   */
   
   
   return OGRERR_NONE;
}


void   VFKReaderPG::PrepareStatement(const char *pszSQLCommand, unsigned int idx)
{
   CPLDebug("OGR-VFK", "VFKReaderDB::PrepareStatement(): %s", pszSQLCommand);
   
   PGresult *hStmt;
   if (idx < m_hStmt.size()) 
   {	
      hStmt = PQprepare(m_poDB, "stmtname", pszSQLCommand, -1, NULL);
      m_hStmt.push_back(hStmt);
   }
   else 
   {	
      hStmt = PQprepare(m_poDB, "stmtname", pszSQLCommand, -1, NULL);
   }
   
   if (PQresultStatus(hStmt) != PGRES_TUPLES_OK) 
   {
	
      CPLError(CE_Failure, CPLE_AppDefined,
	       "In PrepareStatement(): PQprepare(%s):\n  %s",
	       pszSQLCommand, PQresultErrorMessage(hStmt));
      
      if(m_hStmt[idx] != NULL) 
      {	 
	 // sqlite3_finalize(m_hStmt[idx]); // TODO: nevim jak
	 //             m_hStmt.erase(m_hStmt.begin() + idx);
      }
   }
}

OGRErr VFKReaderPG::ExecuteSQL(const char *pszSQLCommand, bool bQuiet)
{
   
   m_res = PQexec(m_poDB, pszSQLCommand);
   if (PQresultStatus(m_res) != PGRES_COMMAND_OK)
   {	
      if (!bQuiet)
	CPLError(CE_Failure, CPLE_AppDefined,
		 "In ExecuteSQL(%s): %s",
		 pszSQLCommand, PQerrorMessage(m_poDB));
      else
	CPLError(CE_Warning, CPLE_AppDefined,
		 "In ExecuteSQL(%s): %s",
		 pszSQLCommand, PQerrorMessage(m_poDB));
      PQclear(m_res);
    
      return  OGRERR_FAILURE;
   }
   
   PQclear(m_res);
      
   return OGRERR_NONE;
}

OGRErr VFKReaderPG::ExecuteSQL(const char *pszSQLCommand, int &count)
{
   OGRErr ret;
   
   PrepareStatement(pszSQLCommand);
   ret = ExecuteSQL(m_hStmt[0]); // TODO: solve
   if (ret == OGRERR_NONE) 
   {	
      count = PQfnumber(m_hStmt[0], 0); // TODO:
   }
   
   // sqlite3_finalize(m_hStmt[0]); // TODO
   m_hStmt[0] = NULL; // TODO
    
   
   return ret;
}

OGRErr VFKReaderPG::ExecuteSQL(std::vector<VFKDbValue> &record, int idx)
{
   OGRErr ret;
   
   ret = ExecuteSQL(m_hStmt[idx]);
   
   // TODO: num_of_column == size
   if (ret == OGRERR_NONE) {
      for (int i = 0; i < record.size(); i++) { // TODO: iterator
	 VFKDbValue *value = &(record[i]);
	 switch (value->get_type()) {
	  case DT_INT:
	    value->set_int(*(int*)PQgetvalue(m_hStmt[idx], 0, i));
	    break;
	  case DT_BIGINT:
	    value->set_bigint(*(GIntBig*)PQgetvalue(m_hStmt[idx], 0, i));
	    break;
	  case DT_DOUBLE:
	    value->set_double(*(double*)PQgetvalue(m_hStmt[idx], 0, i));
	    break;
	  case DT_TEXT:
	    value->set_text(PQgetvalue(m_hStmt[idx], 0, i));
	    break;
	 }
      }
   }
   else
   {
      // sqlite3_finalize(m_hStmt[idx]);
      m_hStmt[idx] = NULL;
      if (idx > 0) {
	 m_hStmt.erase(m_hStmt.begin() + idx);
      }
   }
   
   
   return ret;
}
