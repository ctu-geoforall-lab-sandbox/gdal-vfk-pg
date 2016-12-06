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

#endif // GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED
