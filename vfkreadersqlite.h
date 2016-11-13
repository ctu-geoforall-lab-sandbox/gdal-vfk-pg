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
   OGRErr        ExecuteSQL(sqlite3_stmt *);
   std::vector<sqlite3_stmt *>m_hStmt; // TODO: ** ?
   
protected:
   sqlite3 *m_poDB;
   
public:
   VFKReaderSQLite(const char *);
   virtual ~VFKReaderSQLite();
   
   void          PrepareStatement(const char *, int = 0);
   OGRErr        ExecuteSQL(const char *, bool = FALSE);
   OGRErr        ExecuteSQL(const char *, int&);
   OGRErr        ExecuteSQL(std::vector<VFKDbValue>&, int = 0);
}
;

#endif // GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED