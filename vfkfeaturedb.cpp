/******************************************************************************
 *
 * Project:  VFK Reader - Feature definition (SQLite)
 * Purpose:  Implements VFKFeatureSQLite class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Martin Landa <landa.martin gmail.com>
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

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id: vfkfeaturesqlite.cpp 35571 2016-09-30 20:37:19Z goatbar $");

/*!
  \brief VFKFeatureSQLite constructor (from DB)

  Read VFK feature from DB

  \param poDataBlock pointer to related IVFKDataBlock
*/
VFKFeatureDB::VFKFeatureDB( IVFKDataBlock *poDataBlock ) :
    IVFKFeature(poDataBlock),
    // Starts at 1.
    m_iRowId(static_cast<int>(poDataBlock->GetFeatureCount() + 1))
{
    // Set FID from DB.
    SetFIDFromDB();  // -> m_nFID
}

/*!
  \brief VFKFeatureSQLite constructor

  \param poDataBlock pointer to related IVFKDataBlock
  \param iRowId feature DB rowid (starts at 1)
  \param nFID feature id
*/
VFKFeatureDB::VFKFeatureDB(IVFKDataBlock *poDataBlock, int iRowId,
                           GIntBig nFID) :
    IVFKFeature(poDataBlock),
    m_iRowId(iRowId)
{
    m_nFID = nFID;
}

/*!
  \brief Read FID from DB
*/
OGRErr VFKFeatureDB::SetFIDFromDB()
{
    CPLString   osSQL;
    VFKReaderDB *poReader = (VFKReaderDB *) m_poDataBlock->GetReader();

    osSQL.Printf("SELECT %s FROM %s WHERE rowid = %d",
                 FID_COLUMN, m_poDataBlock->GetName(), m_iRowId);

    std::vector<VFKDbValue> record;
    record.push_back(VFKDbValue(DT_BIGINT));
    poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(record) != OGRERR_NONE) {
        m_nFID = -1;
        return OGRERR_FAILURE;
    }

    m_nFID = static_cast<GIntBig> (record[0]);

    return OGRERR_NONE;
}

/*!
  \brief Set DB row id

  \param iRowId row id to be set
*/
void VFKFeatureDB::SetRowId(int iRowId)
{
    m_iRowId = iRowId;
}

/*!
  \brief VFKFeatureSQLite constructor (derived from VFKFeature)

  Read VFK feature from VFK file and insert it into DB
*/
VFKFeatureDB::VFKFeatureDB(const VFKFeature *poVFKFeature) :
    IVFKFeature(poVFKFeature->m_poDataBlock),
    // Starts at 1.
    m_iRowId(static_cast<int>(
        poVFKFeature->m_poDataBlock->GetFeatureCount() + 1))
{
    m_nFID = poVFKFeature->m_nFID;
}

/*!
  \brief Load geometry (point layers)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureDB::LoadGeometryPoint()
{
    return false;
}

/*!
  \brief Load geometry (linestring SBP layer)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureDB::LoadGeometryLineStringSBP()
{
    return false;
}

/*!
  \brief Load geometry (linestring HP/DPM layer)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureDB::LoadGeometryLineStringHP()
{
    return false;
}

/*!
  \brief Load geometry (polygon BUD/PAR layers)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureDB::LoadGeometryPolygon()
{
    return false;
}
