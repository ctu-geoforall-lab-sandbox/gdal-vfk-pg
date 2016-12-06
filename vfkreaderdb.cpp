/******************************************************************************
 *  * $Id: vfkreaderdb.cpp 33713 2016-03-12 17:41:57Z goatbar $
 *  *
 *  * Project:  VFK Reader (DB)
 *  * Purpose:  Implements VFKReaderDB class.
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
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

#include <cstring>

#include "ogr_geometry.h"

CPL_CVSID("$Id: vfkreadersqlite.cpp 35933 2016-10-25 16:46:26Z goatbar $");

/*!
 *   \brief VFKReaderDB constructor
 * */
VFKReaderDB::VFKReaderDB(const char *pszFileName) : 
    VFKReader(pszFileName),
    // SQLite: filepath
    // PostgreSQL: connection string
    m_pszDBname(NULL),
    // True - build geometry from DB
    // False - store also geometry in DB
    m_bDbSource(false),
    m_bNewDb(false),
    m_bSpatial(CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_SPATIAL", "YES")))
{       
}
       
/*!
  \brief VFKReaderDB destructor
 */
VFKReaderDB::~VFKReaderDB()
{
   delete[] m_pszDBname;
}

/*!
  \brief Load data block definitions (&B)

  Call VFKReader::OpenFile() before this function.

  \return number of data blocks or -1 on error
*/
int VFKReaderDB::ReadDataBlocks()
{
    CPLString osSQL;
    osSQL.Printf("SELECT table_name, table_defn FROM %s", VFK_DB_TABLE);
    sqlite3_stmt *hStmt = PrepareStatement(osSQL.c_str());
    while(ExecuteSQL(hStmt) == OGRERR_NONE) {
        const char *pszName = (const char*) sqlite3_column_text(hStmt, 0);
        const char *pszDefn = (const char*) sqlite3_column_text(hStmt, 1);
        IVFKDataBlock *poNewDataBlock =
            (IVFKDataBlock *) CreateDataBlock(pszName);
        poNewDataBlock->SetGeometryType();
        poNewDataBlock->SetProperties(pszDefn);
        VFKReader::AddDataBlock(poNewDataBlock, NULL);
    }

    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "BEGIN", NULL, NULL, NULL));
    /* Read data from VFK file */
    const int nDataBlocks = VFKReader::ReadDataBlocks();
    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "COMMIT", NULL, NULL, NULL));

    return nDataBlocks;
}

/*!
  \brief Load data records (&D)

  Call VFKReader::OpenFile() before this function.

  \param poDataBlock limit to selected data block or NULL for all

  \return number of data records or -1 on error
*/
int VFKReaderDB::ReadDataRecords(IVFKDataBlock *poDataBlock)
{
    CPLString   osSQL;
    IVFKDataBlock *poDataBlockCurrent = NULL;
    sqlite3_stmt *hStmt = NULL;
    const char *pszName = NULL;
    int nDataRecords = 0;
    bool bReadVfk = !m_bDbSource;
    bool bReadDb = false;

    if (poDataBlock) { /* read records only for selected data block */
        /* table name */
        pszName = poDataBlock->GetName();

        /* check for existing records (re-use already inserted data) */
        osSQL.Printf("SELECT num_records FROM %s WHERE "
                     "table_name = '%s'",
                     VFK_DB_TABLE, pszName);
        hStmt = PrepareStatement(osSQL.c_str());
        if (ExecuteSQL(hStmt) == OGRERR_NONE) {
            nDataRecords = sqlite3_column_int(hStmt, 0);
            if (nDataRecords > 0)
                bReadDb = true; /* -> read from DB */
            else
                nDataRecords = 0;
        }
        sqlite3_finalize(hStmt);
    }
    else
    {                     /* read all data blocks */
        /* check for existing records (re-use already inserted data) */
        osSQL.Printf("SELECT COUNT(*) FROM %s WHERE num_records > 0", VFK_DB_TABLE);
        hStmt = PrepareStatement(osSQL.c_str());
        if (ExecuteSQL(hStmt) == OGRERR_NONE &&
            sqlite3_column_int(hStmt, 0) != 0)
            bReadDb = true;     /* -> read from DB */
        sqlite3_finalize(hStmt);

        /* check if file is already registered in DB (requires file_size column) */
        osSQL.Printf("SELECT COUNT(*) FROM %s WHERE file_name = '%s' AND "
                     "file_size = " CPL_FRMT_GUIB " AND num_records > 0",
                     VFK_DB_TABLE, CPLGetFilename(m_pszFilename),
                     (GUIntBig) m_poFStat->st_size);
        hStmt = PrepareStatement(osSQL.c_str());
        if (ExecuteSQL(hStmt) == OGRERR_NONE &&
            sqlite3_column_int(hStmt, 0) > 0) {
            /* -> file already registered (filename & size is the same) */
            CPLDebug("OGR-VFK", "VFK file %s already loaded in DB", m_pszFilename);
            bReadVfk = false;
        }
        sqlite3_finalize(hStmt);
    }

    if( bReadDb )
    {  /* read records from DB */
        /* read from  DB */
        VFKFeatureSQLite *poNewFeature = NULL;

        poDataBlockCurrent = NULL;
        for( int iDataBlock = 0;
             iDataBlock < GetDataBlockCount();
             iDataBlock++ )
        {
            poDataBlockCurrent = GetDataBlock(iDataBlock);

            if (poDataBlock && poDataBlock != poDataBlockCurrent)
                continue;

            poDataBlockCurrent->SetFeatureCount(0); /* avoid recursive call */

            pszName = poDataBlockCurrent->GetName();
            CPLAssert(NULL != pszName);

            osSQL.Printf("SELECT %s,_rowid_ FROM %s ",
                         FID_COLUMN, pszName);
            if (EQUAL(pszName, "SBP"))
              osSQL += "WHERE PORADOVE_CISLO_BODU = 1 ";
            osSQL += "ORDER BY ";
            osSQL += FID_COLUMN;
            hStmt = PrepareStatement(osSQL.c_str());
            nDataRecords = 0;
            while (ExecuteSQL(hStmt) == OGRERR_NONE) {
                const long iFID = sqlite3_column_int(hStmt, 0);
                int iRowId = sqlite3_column_int(hStmt, 1);
                poNewFeature = new VFKFeatureSQLite(poDataBlockCurrent, iRowId, iFID);
                poDataBlockCurrent->AddFeature(poNewFeature);
                nDataRecords++;
            }

            /* check DB consistency */
            osSQL.Printf("SELECT num_features FROM %s WHERE table_name = '%s'",
                         VFK_DB_TABLE, pszName);
            hStmt = PrepareStatement(osSQL.c_str());
            if (ExecuteSQL(hStmt) == OGRERR_NONE) {
                const int nFeatDB = sqlite3_column_int(hStmt, 0);
                if (nFeatDB > 0 && nFeatDB != poDataBlockCurrent->GetFeatureCount())
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: Invalid number of features " CPL_FRMT_GIB " (should be %d)",
                             pszName, poDataBlockCurrent->GetFeatureCount(), nFeatDB);
            }
            sqlite3_finalize(hStmt);
        }
    }

    if( bReadVfk )
    {  /* read from VFK file and insert records into DB */
        /* begin transaction */
        ExecuteSQL("BEGIN");

        /* Store VFK header to DB */
        StoreInfo2DB();

        /* Insert VFK data records into DB */
        nDataRecords += VFKReader::ReadDataRecords(poDataBlock);

        /* update VFK_DB_TABLE table */
        poDataBlockCurrent = NULL;
        for( int iDataBlock = 0;
             iDataBlock < GetDataBlockCount();
             iDataBlock++)
        {
            poDataBlockCurrent = GetDataBlock(iDataBlock);

            if (poDataBlock && poDataBlock != poDataBlockCurrent)
                continue;

            osSQL.Printf("UPDATE %s SET num_records = %d WHERE "
                         "table_name = '%s'",
                         VFK_DB_TABLE, poDataBlockCurrent->GetRecordCount(),
                         poDataBlockCurrent->GetName());

            ExecuteSQL(osSQL);
        }

        /* commit transaction */
        ExecuteSQL("COMMIT");
    }

    return nDataRecords;
}


/*!
  \brief Store header info to VFK_DB_HEADER
*/
void VFKReaderDB::StoreInfo2DB()
{
    for( std::map<CPLString, CPLString>::iterator i = poInfo.begin();
         i != poInfo.end(); ++i )
    {
        const char *value = i->second.c_str();

        const char q = (value[0] == '"') ? ' ' : '"';

        CPLString osSQL;
        osSQL.Printf("INSERT INTO %s VALUES(\"%s\", %c%s%c)",
                     VFK_DB_HEADER, i->first.c_str(),
                     q, value, q);
        ExecuteSQL(osSQL);
    }
}

/*!
  \brief Create index

  If creating unique index fails, then non-unique index is created instead.

  \param name index name
  \param table table name
  \param column column(s) name
  \param unique true to create unique index
*/
void VFKReaderDB::CreateIndex(const char *name, const char *table, const char *column,
                                  bool unique)
{
    CPLString   osSQL;

    if (unique) {
        osSQL.Printf("CREATE UNIQUE INDEX %s ON %s (%s)",
                     name, table, column);
        if (ExecuteSQL(osSQL.c_str()) == OGRERR_NONE) {
            return;
        }
    }

    osSQL.Printf("CREATE INDEX %s ON %s (%s)",
                 name, table, column);
    ExecuteSQL(osSQL.c_str());
}


/*!
  \brief Create new data block

  \param pszBlockName name of the block to be created

  \return pointer to VFKDataBlockSQLite instance
*/
IVFKDataBlock *VFKReaderDB::CreateDataBlock(const char *pszBlockName)
{
    /* create new data block, i.e. table in DB */
    return new VFKDataBlockSQLite(pszBlockName, (IVFKReader *) this);
}

/*!
  \brief Create DB table from VFKDataBlock (SQLITE only)

  \param poDataBlock pointer to VFKDataBlock instance
*/
void VFKReaderDB::AddDataBlock(IVFKDataBlock *poDataBlock, const char *pszDefn)
{
    CPLString osColumn;

    const char *pszBlockName = poDataBlock->GetName();

    /* register table in VFK_DB_TABLE */
    CPLString osCommand;
    osCommand.Printf("SELECT COUNT(*) FROM %s WHERE "
                     "table_name = '%s'",
                     VFK_DB_TABLE, pszBlockName);
    sqlite3_stmt *hStmt = PrepareStatement(osCommand.c_str());

    if (ExecuteSQL(hStmt) == OGRERR_NONE &&
        sqlite3_column_int(hStmt, 0) == 0) {

        osCommand.Printf("CREATE TABLE IF NOT EXISTS '%s' (", pszBlockName);
        for (int i = 0; i < poDataBlock->GetPropertyCount(); i++) {
            VFKPropertyDefn *poPropertyDefn = poDataBlock->GetProperty(i);
            if (i > 0)
                osCommand += ",";
            osColumn.Printf("%s %s", poPropertyDefn->GetName(),
                            poPropertyDefn->GetTypeSQL().c_str());
            osCommand += osColumn;
        }
        osColumn.Printf(",%s integer", FID_COLUMN);
        osCommand += osColumn;
        if (poDataBlock->GetGeometryType() != wkbNone) {
            osColumn.Printf(",%s blob", GEOM_COLUMN);
            osCommand += osColumn;
        }
        osCommand += ")";
        ExecuteSQL(osCommand.c_str()); /* CREATE TABLE */

        /* create indices */
        osCommand.Printf("%s_%s", pszBlockName, FID_COLUMN);
        CreateIndex(osCommand.c_str(), pszBlockName, FID_COLUMN,
                    !EQUAL(pszBlockName, "SBP"));

        const char *pszKey = ((VFKDataBlockSQLite *) poDataBlock)->GetKey();
        if (pszKey) {
            osCommand.Printf("%s_%s", pszBlockName, pszKey);
            CreateIndex(osCommand.c_str(), pszBlockName, pszKey, !m_bAmendment);
        }

        if (EQUAL(pszBlockName, "SBP")) {
            /* create extra indices for SBP */
            CreateIndex("SBP_OB",        pszBlockName, "OB_ID", false);
            CreateIndex("SBP_HP",        pszBlockName, "HP_ID", false);
            CreateIndex("SBP_DPM",       pszBlockName, "DPM_ID", false);
            CreateIndex("SBP_OB_HP_DPM", pszBlockName, "OB_ID,HP_ID,DPM_ID", true);
            CreateIndex("SBP_OB_POR",    pszBlockName, "OB_ID,PORADOVE_CISLO_BODU", false);
            CreateIndex("SBP_HP_POR",    pszBlockName, "HP_ID,PORADOVE_CISLO_BODU", false);
            CreateIndex("SBP_DPM_POR",   pszBlockName, "DPM_ID,PORADOVE_CISLO_BODU", false);
        }
        else if (EQUAL(pszBlockName, "HP")) {
            /* create extra indices for HP */
            CreateIndex("HP_PAR1",        pszBlockName, "PAR_ID_1", false);
            CreateIndex("HP_PAR2",        pszBlockName, "PAR_ID_2", false);
        }
        else if (EQUAL(pszBlockName, "OB")) {
            /* create extra indices for OP */
            CreateIndex("OB_BUD",        pszBlockName, "BUD_ID", false);
        }

        /* update VFK_DB_TABLE meta-table */
        osCommand.Printf("INSERT INTO %s (file_name, file_size, table_name, "
                         "num_records, num_features, num_geometries, table_defn) VALUES "
                         "('%s', " CPL_FRMT_GUIB ", '%s', -1, 0, 0, '%s')",
                         VFK_DB_TABLE, CPLGetFilename(m_pszFilename),
                         (GUIntBig) m_poFStat->st_size,
                         pszBlockName, pszDefn);

        ExecuteSQL(osCommand.c_str());

        sqlite3_finalize(hStmt);
    }

    return VFKReader::AddDataBlock(poDataBlock, NULL);
}


/*!
  \brief Add feature

  \param poDataBlock pointer to VFKDataBlock instance
  \param poFeature pointer to VFKFeature instance
*/
OGRErr VFKReaderDB::AddFeature( IVFKDataBlock *poDataBlock,
                                    VFKFeature *poFeature )
{
    CPLString osValue;

    const VFKProperty *poProperty = NULL;

    const char *pszBlockName = poDataBlock->GetName();
    CPLString osCommand;
    osCommand.Printf("INSERT INTO '%s' VALUES(", pszBlockName);

    for( int i = 0; i < poDataBlock->GetPropertyCount(); i++ )
    {
        const OGRFieldType ftype = poDataBlock->GetProperty(i)->GetType();
        poProperty = poFeature->GetProperty(i);
        if (i > 0)
            osCommand += ",";

        if( poProperty->IsNull() )
        {
            osValue.Printf("NULL");
        }
        else
        {
            switch (ftype) {
            case OFTInteger:
                osValue.Printf("%d", poProperty->GetValueI());
                break;
            case OFTReal:
                osValue.Printf("%f", poProperty->GetValueD());
                break;
            case OFTString:
                if (poDataBlock->GetProperty(i)->IsIntBig())
                    osValue.Printf("%s", poProperty->GetValueS());
                else
                    osValue.Printf("'%s'", poProperty->GetValueS(true));
                break;
            default:
                osValue.Printf("'%s'", poProperty->GetValueS());
                break;
            }
        }
        osCommand += osValue;
    }
    osValue.Printf("," CPL_FRMT_GIB, poFeature->GetFID());
    if (poDataBlock->GetGeometryType() != wkbNone) {
        osValue += ",NULL";
    }
    osValue += ")";
    osCommand += osValue;

    if( ExecuteSQL(osCommand.c_str(), true) != OGRERR_NONE )
        return OGRERR_FAILURE;

    if (EQUAL(pszBlockName, "SBP")) {
        poProperty = poFeature->GetProperty("PORADOVE_CISLO_BODU");
        if( poProperty == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find property PORADOVE_CISLO_BODU");
            return OGRERR_FAILURE;
        }
        if (!EQUAL(poProperty->GetValueS(), "1"))
            return OGRERR_NONE;
    }

    VFKFeatureSQLite *poNewFeature =
        new VFKFeatureSQLite(poDataBlock,
                             poDataBlock->GetRecordCount(RecordValid) + 1,
                             poFeature->GetFID());
    poDataBlock->AddFeature(poNewFeature);

    return OGRERR_NONE;
}
