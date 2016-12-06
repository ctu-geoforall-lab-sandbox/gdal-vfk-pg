/******************************************************************************
 * $Id: vfkreaderp.h 35769 2016-10-16 19:25:21Z rouault $
 *
 * Project:  VFK Reader
 * Purpose:  Private Declarations for OGR free VFK Reader code.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012-2013, Martin Landa <landa.martin gmail.com>
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

#ifndef GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADERP_H_INCLUDED

#include <map>
#include <string>

#include "vfkreader.h"
#include "ogr_api.h"

class VFKReader;

enum VFKSQLTYPE { DT_INT, DT_BIGINT, DT_DOUBLE, DT_TEXT };

class VFKDbValue
{
private:
   int m_iVal;
   GIntBig m_iValB;
   double m_dVal;
   CPLString m_sVal;
   
   VFKSQLTYPE m_type;
 
public:
   VFKDbValue(VFKSQLTYPE type): m_type (type) {};
       
   VFKSQLTYPE get_type() const { return m_type; }
   
   void set_int(int val)        { m_iVal = val; } //TODO: zjednodusit
   void set_bigint(GIntBig val) { m_iValB = val; }
   void set_double(double val)  { m_dVal = val; }
   void set_text(char *val)     { m_sVal.assign(val, strlen(val)); }
    
   operator int()       { return m_iVal; }
   operator GIntBig()   { return m_iValB; }
   operator double()    { return m_dVal; }
   operator CPLString() { return m_sVal; }
};


/************************************************************************/
/*                              VFKReader                               */
/************************************************************************/
class VFKReader : public IVFKReader
{
private:
    bool           m_bLatin2;

    VSILFILE      *m_poFD;
    char          *ReadLine( bool = false );

    void          AddInfo(const char *);

protected:
    char           *m_pszFilename;
    VSIStatBuf     *m_poFStat;
    bool            m_bAmendment;
    int             m_nDataBlockCount;
    IVFKDataBlock **m_papoDataBlock;

    IVFKDataBlock  *CreateDataBlock(const char *);
    void            AddDataBlock(IVFKDataBlock *, const char *);
    OGRErr          AddFeature(IVFKDataBlock *, VFKFeature *);

    // Metadata.
    std::map<CPLString, CPLString> poInfo;

public:
    VFKReader( const char *pszFilename );
    virtual ~VFKReader();

    bool           IsLatin2() const { return m_bLatin2; }
    bool           IsSpatial() const { return false; }
    bool           IsPreProcessed() const { return false; }
    bool           IsValid() const { return true; }
    int            ReadDataBlocks();
    int            ReadDataRecords(IVFKDataBlock * = NULL);
    int            LoadGeometry();

    int            GetDataBlockCount() const { return m_nDataBlockCount; }
    IVFKDataBlock *GetDataBlock(int) const;
    IVFKDataBlock *GetDataBlock(const char *) const;

    const char    *GetInfo(const char *);
};

/************************************************************************/
/*                              VFKReaderDB                             */
/************************************************************************/

class VFKReaderDB : public VFKReader
{
private:
    IVFKDataBlock *CreateDataBlock(const char *);
    void           AddDataBlock(IVFKDataBlock *, const char *);
    OGRErr         AddFeature(IVFKDataBlock *, VFKFeature *);

    void           StoreInfo2DB();

    void           CreateIndex(const char *, const char *, const char *, bool = true);

    friend class   VFKFeatureDB;

protected:
    char          *m_pszDBname;
    bool           m_bDbSource;
    bool           m_bNewDb;
    bool           m_bSpatial;

public:
    VFKReaderDB(const char *);
    virtual ~VFKReaderDB();

    bool          IsSpatial() const { return m_bSpatial; }
    bool          IsPreProcessed() const { return !m_bNewDb; }
    int           ReadDataBlocks();
    int           ReadDataRecords(IVFKDataBlock * = NULL);

    virtual bool    IsValid() const = 0;

    virtual void    PrepareStatement(const char *, unsigned int = 0) = 0;
    virtual OGRErr  ExecuteSQL(const char *, bool = FALSE) = 0;
    virtual OGRErr  ExecuteSQL(const char *, int&) = 0;
    virtual OGRErr  ExecuteSQL(std::vector<VFKDbValue>&, int = 0) = 0;
};

#endif // GDAL_OGR_VFK_VFKREADERP_H_INCLUDED
