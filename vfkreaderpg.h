#ifndef GDAL_OGR_VFK_VFKREADERPG_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADERPG_H_INCLUDED

#include "vfkreaderp.h"
#include "libpq-fe.h"

/************************************************************************/
/*                              VFKReaderPG                             */
/************************************************************************/

class VFKReaderPG : public VFKReaderDB
{
   
private:
   PGconn     *m_poDB;
   CPLString   m_pszConnStr;

   OGRErr        ExecuteSQL(PGresult *);
   std::vector<CPLString, PGresult *, PGresult *> m_hStmt;
  
public:
   VFKReaderPG(const char *);
   virtual ~VFKReaderPG();

   bool          IsValid() const { return m_poDB != NULL; }
   
   void          PrepareStatement(const char *, unsigned int = 0);
   OGRErr        ExecuteSQL(const char *, bool = FALSE);
   OGRErr        ExecuteSQL(const char *, int&);
   OGRErr        ExecuteSQL(std::vector<VFKDbValue>&, int = 0);
}
;

#endif // GDAL_OGR_VFK_VFKREADERPG_H_INCLUDED
