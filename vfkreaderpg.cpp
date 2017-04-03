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
   const char *pszDbName, *pszConnStr;

   pszDbName = CPLGetConfigOption("OGR_VFK_DB_NAME", NULL);
   pszConnStr = strstr(pszDbName, "PG:"); 
   if (pszConnStr == NULL) {
       CPLError(CE_Failure, CPLE_AppDefined,
                "Invalid connection string");
   }
   m_pszConnStr = CPLString(pszConnStr + strlen("PG:"));
   if (m_pszConnStr.back() == '"') {
       /* remove '"' from connection string */
       m_pszConnStr = m_pszConnStr.substr(0, m_pszConnStr.size()-1);
   }
   m_poDB = PQconnectdb(m_pszConnStr.c_str());
   if (PQstatus(m_poDB) != CONNECTION_OK)
   {	
	CPLError(CE_Failure, CPLE_AppDefined,
		 "Connection to database failed: %s",
		 PQerrorMessage(m_poDB));
	PQfinish(m_poDB);
   }

   // TODO: explain (requires > PG >= 9.2)
   // PQsetSingleRowMode(m_poDB);

   CPLString osSQL;
   int ncols;
   osSQL.Printf("SELECT COUNT(*) FROM information_schema.columns "
                "WHERE table_name='%s'", VFK_DB_TABLE);
   if (ExecuteSQL(osSQL.c_str(), ncols) != OGRERR_NONE) {
        /* VFK table does not exist */
        m_bNewDb = true;
   }
   else {
       if (ncols != 7) {
           // TODO: drop table
           m_bNewDb = true;
       }
   }
}

/*!
  \brief VFKReaderSQLite destructor
*/
VFKReaderPG::~VFKReaderPG()
{
   PQfinish(m_poDB);
}

OGRErr VFKReaderPG::ExecuteSQL(PGresult *hStmt)
{
    int idx;
    PGresult *hStmtExec;

    idx = 0;
    while (m_hStmt[idx].prepared() != hStmt)
        idx++;

    // TODO: assert when hStmt not found

    hStmtExec = PQexecPrepared(m_poDB, m_hStmt[idx].name(),
                               0, NULL, NULL, NULL, 0);

    if (hStmtExec == NULL) {
        /* TODO:
           || PQresultStatus(hStmtExec) != PGRES_SINGLE_TUPLE) */
        if (PQresultStatus(hStmtExec) != PGRES_BAD_RESPONSE) {
            if (hStmtExec)
                PQclear(hStmtExec);
            return OGRERR_NOT_ENOUGH_DATA;
        }

        if (hStmtExec)
            PQclear(hStmtExec);

        CPLError(CE_Failure, CPLE_AppDefined,
                 "In ExecuteSQL(): %s",
                 PQerrorMessage(m_poDB));
        
        return OGRERR_FAILURE;
    }
   
    m_hStmt[idx].setExec(hStmtExec);
    
    return OGRERR_NONE;
}


void VFKReaderPG::PrepareStatement(const char *pszSQLCommand, unsigned int idx)
{
   CPLString szStmtName;
   PGresult *hStmt;

   CPLDebug("OGR-VFK", "VFKReaderPG::PrepareStatement(): %s", pszSQLCommand);

   szStmtName = "stmt_"; // TODO: replace stmtname by some random string
   hStmt = PQprepare(m_poDB, szStmtName.c_str(), pszSQLCommand, -1, NULL); 
   if (hStmt == NULL ||
       // TODO: PGRES_BAD_RESPONSE would be probably enough
       // (PQresultStatus(hStmt) != PGRES_COMMAND_OK && PQresultStatus(hStmt) != PGRES_TUPLES_OK)) { 
       PQresultStatus(hStmt) != PGRES_BAD_RESPONSE) {

       CPLError(CE_Failure, CPLE_AppDefined,
                "In PrepareStatement(): PQprepare(%s):\n  %s",
                pszSQLCommand, PQerrorMessage(m_poDB));

       PQclear(hStmt);

       return;
   }

   if (idx <= m_hStmt.size()) {
       m_hStmt.push_back(VFKStmtPG(szStmtName, hStmt));
   }
}

OGRErr VFKReaderPG::ExecuteSQL(const char *pszSQLCommand, bool bQuiet)
{
    PGresult *hStmt;

    hStmt = PQexec(m_poDB, pszSQLCommand); 
    if (hStmt == NULL ||
        // TODO: PGRES_BAD_RESPONSE would be probably enough
        // (PQresultStatus(hStmt) != PGRES_COMMAND_OK && PQresultStatus(hStmt) != PGRES_TUPLES_OK)) { 
        PQresultStatus(hStmt) != PGRES_BAD_RESPONSE) {
        
        if (!bQuiet)
            CPLError(CE_Failure, CPLE_AppDefined,
                     "In ExecuteSQL(%s): %s",
                     pszSQLCommand, PQerrorMessage(m_poDB));
        else
            CPLError(CE_Warning, CPLE_AppDefined,
                     "In ExecuteSQL(%s): %s",
                     pszSQLCommand, PQerrorMessage(m_poDB));
        
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

OGRErr VFKReaderPG::ExecuteSQL(const char *pszSQLCommand, int &count)
{
    int idx;
    OGRErr ret;

    idx = m_hStmt.size() - 1;
    // TODO: assert idx < 0
    
    PrepareStatement(pszSQLCommand, idx);
    ret = ExecuteSQL(m_hStmt[idx].prepared()); // TODO: avoid arrays
    if (ret == OGRERR_NONE) {
        const char *pszCount;
        char *pszErr;
        
        pszCount = PQgetvalue(m_hStmt[idx].exec(), 0, 0);
        count = static_cast<int>(strtol(pszCount, &pszErr, 0));
        if (*pszErr != '\0') {
            CPLDebug("OGR-VFK", "Unable to cast '%s' to integer. %s",
                     pszCount, pszErr);
            return OGRERR_FAILURE;
        }
    }
    
    m_hStmt[idx].clear();
    
    return ret;
}

OGRErr VFKReaderPG::ExecuteSQL(int idx)
{
    return OGRERR_NONE;
}

OGRErr VFKReaderPG::ExecuteSQL(std::vector<VFKDbValue> &record, int idx)
{
   OGRErr ret;
   
   ret = ExecuteSQL(m_hStmt[idx].prepared());
   
   // TODO: num_of_column == size
   if (ret == OGRERR_NONE) {
      for (size_t i = 0; i < record.size(); i++) { // TODO: iterator
	 VFKDbValue *value = &(record[i]);
         PGresult * hStmt = m_hStmt[idx].exec();
	 switch (value->get_type()) {
	  case DT_INT:
	    value->set_int(*(int*)PQgetvalue(hStmt, 0, i));
	    break;
	  case DT_BIGINT:
          case DT_UBIGINT:
	    value->set_bigint(*(GIntBig*)PQgetvalue(hStmt, 0, i));
	    break;
	  case DT_DOUBLE:
	    value->set_double(*(double*)PQgetvalue(hStmt, 0, i));
	    break;
	  case DT_TEXT:
	    value->set_text(PQgetvalue(hStmt, 0, i));
	    break;
	 }
      }
   }
   else
   {
       m_hStmt[idx].clear();
       if (idx > 0) {
           m_hStmt.erase(m_hStmt.begin() + idx);
       }
   }
   
   
   return ret;
}

OGRErr VFKReaderPG::SaveGeometryToDB(GByte *papyWKB, size_t nWKBLen)
{
    return OGRERR_NONE;
}
