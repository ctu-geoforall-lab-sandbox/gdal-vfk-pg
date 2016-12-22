#ifndef GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED

#include<vector>

#include "vfkreaderp.h"
#include "sqlite3.h"

/************************************************************************/
/*                              VFKReaderSQLite                         */
/************************************************************************/

class VFKReaderSQLite : public VFKReaderDB
{
   
private:
   sqlite3      *m_poDB;

   OGRErr        ExecuteSQL(sqlite3_stmt *);
   std::vector<sqlite3_stmt *>m_hStmt; // TODO: ** ?
  
   friend class   VFKFeatureSQLite;

public:
   VFKReaderSQLite(const char *);
   virtual ~VFKReaderSQLite();

   bool          IsValid() const { return m_poDB != NULL; }

   void          PrepareStatement(const char *, unsigned int = 0);
   OGRErr        ExecuteSQL(const char *, bool = FALSE);
   OGRErr        ExecuteSQL(const char *, int&);
   OGRErr        ExecuteSQL(std::vector<VFKDbValue>&, int = 0);
}
;

/************************************************************************/
/*                              VFKFeatureSQLite                        */
/************************************************************************/
class VFKFeatureSQLite : public VFKFeatureDB
{
private:
    sqlite3_stmt        *m_hStmt;
    
    OGRErr ExecuteSQL(const char *pszSQLCommand);
    void   FinalizeSQL();
    
public:
    VFKFeatureSQLite(IVFKDataBlock * poDataBlock):
       VFKFeatureDB(poDataBlock), m_hStmt(NULL)  {}
    VFKFeatureSQLite(IVFKDataBlock * poDataBlock, int iRowId, GIntBig nFID):
       VFKFeatureDB(poDataBlock, iRowId, nFID), m_hStmt(NULL) {}
    VFKFeatureSQLite(const VFKFeature *poVFKFeature):
       VFKFeatureDB(poVFKFeature), m_hStmt(NULL) {}

    OGRErr LoadProperties(OGRFeature *);
};

#endif // GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED
