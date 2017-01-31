/******************************************************************************
 *
 * Project:  VFK Reader (SQLite)
 * Purpose:  Implements VFKReaderSQLite class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "cpl_vsi.h"

#include "vfkreader.h"
#include "vfkreadersqlite.h"

#include "cpl_conv.h"
#include "cpl_error.h"

#include <cstring>

#include "ogr_geometry.h"

CPL_CVSID("$Id: vfkreadersqlite.cpp 35933 2016-10-25 16:46:26Z goatbar $");

/*!
  \brief VFKReaderSQLite constructor
*/
VFKReaderSQLite::VFKReaderSQLite( const char *pszFileName ) :
  VFKReaderDB(pszFileName),
    m_poDB(NULL)
{
    size_t nLen = 0;
    VSIStatBufL sStatBufDb;
    {
        GDALOpenInfo *poOpenInfo = new GDALOpenInfo(pszFileName, GA_ReadOnly);
        m_bDbSource = poOpenInfo->nHeaderBytes >= 16 &&
            STARTS_WITH((const char*)poOpenInfo->pabyHeader, "SQLite format 3");
        delete poOpenInfo;
    }

    const char *pszDbNameConf = CPLGetConfigOption("OGR_VFK_DB_NAME", NULL);
    CPLString osDbName;

    if( !m_bDbSource )
    {
        m_bNewDb = true;

        /* open tmp SQLite DB (re-use DB file if already exists) */
        if (pszDbNameConf) {
            osDbName = pszDbNameConf;
        }
        else
        {
            osDbName = CPLResetExtension(m_pszFilename, "db");
        }
        nLen = osDbName.length();
        if( nLen > 2048 )
        {
            nLen = 2048;
            osDbName.resize(nLen);
        }
    }
    else
    {
        // m_bNewDb = false;
        nLen = strlen(pszFileName);
        osDbName = pszFileName;
    }

    m_pszDBname = new char [nLen+1];
    std::strncpy(m_pszDBname, osDbName.c_str(), nLen);
    m_pszDBname[nLen] = 0;

    CPLDebug("OGR-VFK", "Using internal DB: %s",
             m_pszDBname);

    if( !m_bDbSource && VSIStatL(osDbName, &sStatBufDb) == 0 )
    {
        /* Internal DB exists */
        if (CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_OVERWRITE", "NO"))) {
            m_bNewDb = true;     // Overwrite existing DB.
            CPLDebug("OGR-VFK", "Internal DB (%s) already exists and will be overwritten",
                     m_pszDBname);
            VSIUnlink(osDbName);
        }
        else
        {
            if (pszDbNameConf == NULL &&
                m_poFStat->st_mtime > sStatBufDb.st_mtime) {
                CPLDebug("OGR-VFK",
                         "Found %s but ignoring because it appears\n"
                         "be older than the associated VFK file.",
                         osDbName.c_str());
                m_bNewDb = true;
                VSIUnlink(osDbName);
            }
            else
            {
                m_bNewDb = false;    /* re-use existing DB */
            }
        }
    }

    CPLDebug("OGR-VFK", "New DB: %s Spatial: %s",
             m_bNewDb ? "yes" : "no", m_bSpatial ? "yes" : "no");

    if (SQLITE_OK != sqlite3_open(osDbName, &m_poDB)) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Creating SQLite DB failed: %s",
                 sqlite3_errmsg(m_poDB));
    }

    int nRowCount = 0;
    int nColCount = 0;
    CPLString osCommand;
    if( m_bDbSource )
    {
        /* check if it's really VFK DB datasource */
        char* pszErrMsg = NULL;
        char** papszResult = NULL;
        nRowCount = nColCount = 0;

        osCommand.Printf("SELECT * FROM sqlite_master WHERE type='table' AND name='%s'",
                         VFK_DB_TABLE);
        sqlite3_get_table(m_poDB,
                          osCommand.c_str(),
                          &papszResult,
                          &nRowCount, &nColCount, &pszErrMsg);
        sqlite3_free_table(papszResult);
        sqlite3_free(pszErrMsg);

        if (nRowCount != 1) {
            /* DB is not valid VFK datasource */
            sqlite3_close(m_poDB);
            m_poDB = NULL;
            return;
        }
    }

    if( !m_bNewDb )
    {
        /* check if DB is up-to-date datasource */
        char* pszErrMsg = NULL;
        char** papszResult = NULL;
        nRowCount = nColCount = 0;
        osCommand.Printf("SELECT * FROM %s LIMIT 1", VFK_DB_TABLE);
        sqlite3_get_table(m_poDB,
                          osCommand.c_str(),
                          &papszResult,
                          &nRowCount, &nColCount, &pszErrMsg);
        sqlite3_free_table(papszResult);
        sqlite3_free(pszErrMsg);

        if (nColCount != 7) {
            /* it seems that DB is outdated, let's create new DB from
             * scratch */
            if( m_bDbSource )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid VFK DB datasource");
            }

            if (SQLITE_OK != sqlite3_close(m_poDB)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Closing SQLite DB failed: %s",
                         sqlite3_errmsg(m_poDB));
            }
            VSIUnlink(osDbName);
            if (SQLITE_OK != sqlite3_open(osDbName, &m_poDB)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Creating SQLite DB failed: %s",
                         sqlite3_errmsg(m_poDB));
            }
            CPLDebug("OGR-VFK", "Internal DB (%s) is invalid - will be re-created",
                     m_pszDBname);

            m_bNewDb = true;
        }
    }

    char* pszErrMsg = NULL;
    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "PRAGMA synchronous = OFF",
                                    NULL, NULL, &pszErrMsg));
    sqlite3_free(pszErrMsg);

    if( m_bNewDb )
    {
        /* new DB, create support metadata tables */
        osCommand.Printf(
            "CREATE TABLE %s (file_name text, file_size integer, "
            "table_name text, num_records integer, "
            "num_features integer, num_geometries integer, table_defn text)",
            VFK_DB_TABLE);
        ExecuteSQL(osCommand.c_str());

        /* header table */
        osCommand.Printf(
            "CREATE TABLE %s (key text, value text)", VFK_DB_HEADER);
        ExecuteSQL(osCommand.c_str());
    }
}

/*!
  \brief VFKReaderSQLite destructor
*/
VFKReaderSQLite::~VFKReaderSQLite()
{
    // Close tmp SQLite DB.
    if( SQLITE_OK != sqlite3_close(m_poDB) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Closing SQLite DB failed: %s",
                 sqlite3_errmsg(m_poDB));
    }
    CPLDebug("OGR-VFK", "Internal DB (%s) closed", m_pszDBname);

    /* delete tmp SQLite DB if requested */
    if( CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_DELETE", "NO")) )
    {
        CPLDebug("OGR-VFK", "Internal DB (%s) deleted", m_pszDBname);
        VSIUnlink(m_pszDBname);
    }
}

/*!
  \brief Prepare SQL statement

  \param pszSQLCommand SQL statement to be prepared

  \return pointer to sqlite3_stmt instance or NULL on error
*/
void VFKReaderSQLite::PrepareStatement(const char *pszSQLCommand, unsigned int idx)
{
    int rc;
    sqlite3_stmt *hStmt;

    CPLDebug("OGR-VFK", "VFKReaderDB::PrepareStatement(): %s", pszSQLCommand);

    rc = sqlite3_prepare(m_poDB, pszSQLCommand, -1,
                         &hStmt, NULL);

    // TODO(schwehr): if( rc == SQLITE_OK ) return NULL;    
    if (rc != SQLITE_OK) {
        sqlite3_finalize(hStmt);
        
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In PrepareStatement(): sqlite3_prepare(%s):\n  %s",
                 pszSQLCommand, sqlite3_errmsg(m_poDB));

        return;
    }

    if (idx <= m_hStmt.size()) {
        m_hStmt.push_back(hStmt);
    }
}

/*!
  \brief Execute prepared SQL statement

  \param hStmt pointer to sqlite3_stmt

  \return OGRERR_NONE on success
*/
OGRErr VFKReaderSQLite::ExecuteSQL(sqlite3_stmt *hStmt)
{
    const int rc = sqlite3_step(hStmt);
    if (rc != SQLITE_ROW) {
        if (rc == SQLITE_DONE) {
            sqlite3_finalize(hStmt);
            return OGRERR_NOT_ENOUGH_DATA;
        }

        CPLError(CE_Failure, CPLE_AppDefined,
                 "In ExecuteSQL(): sqlite3_step:\n  %s",
                 sqlite3_errmsg(m_poDB));
        if (hStmt) {
            sqlite3_finalize(hStmt);
        }
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/*!
  \brief Execute SQL statement (SQLITE only)

  \param pszSQLCommand SQL command to execute
  \param bQuiet true to print debug message on failure instead of error message

  \return OGRERR_NONE on success or OGRERR_FAILURE on failure
*/
OGRErr VFKReaderSQLite::ExecuteSQL( const char *pszSQLCommand, bool bQuiet )
{
    char *pszErrMsg = NULL;

    if( SQLITE_OK != sqlite3_exec(m_poDB, pszSQLCommand,
                                  NULL, NULL, &pszErrMsg) )
    {
        if (!bQuiet)
            CPLError(CE_Failure, CPLE_AppDefined,
                     "In ExecuteSQL(%s): %s",
                     pszSQLCommand, pszErrMsg);
        else
            CPLError(CE_Warning, CPLE_AppDefined,
                     "In ExecuteSQL(%s): %s",
                     pszSQLCommand, pszErrMsg);

        return  OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

OGRErr VFKReaderSQLite::ExecuteSQL(const char *pszSQLCommand, int& count)
{
    int idx;
    OGRErr ret;

    idx = m_hStmt.size();

    PrepareStatement(pszSQLCommand, idx);
    ret = ExecuteSQL(m_hStmt[idx]); // TODO: avoid arrays
    if (ret == OGRERR_NONE) {
        count = sqlite3_column_int(m_hStmt[idx], 0); 
    }

    sqlite3_finalize(m_hStmt[0]); 
    m_hStmt[0] = NULL; 

    return ret;
}

OGRErr VFKReaderSQLite::ExecuteSQL(std::vector<VFKDbValue>& record, int idx)
{
    OGRErr ret;
    
    ret = ExecuteSQL(m_hStmt[idx]);
    // TODO: num_of_column == size
    if (ret == OGRERR_NONE) {
        for (unsigned int i = 0; i < record.size(); i++) { // TODO: iterator
            VFKDbValue *value = &(record[i]);
            switch (value->get_type()) {
            case DT_INT:
                value->set_int(sqlite3_column_int(m_hStmt[idx], i));
                break;
            case DT_BIGINT:
            case DT_UBIGINT:
                value->set_bigint(sqlite3_column_int64(m_hStmt[idx], i));
                break;
            case DT_DOUBLE:
                value->set_double(sqlite3_column_double(m_hStmt[idx], i));
                break;
            case DT_TEXT:
                value->set_text((char *)sqlite3_column_text(m_hStmt[idx], i));
                break;
            }
        }
    }
    else {
        /* hStmt should be already finalized by ExecuteSQL() */
        if (idx > 0) {
            m_hStmt.erase(m_hStmt.begin() + idx);
        }
        else { /* idx == 0 */
            m_hStmt[idx] = NULL;
        }
    }

    return ret;
}
