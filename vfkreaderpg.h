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
    class VFKStmtPG {
    private:
        CPLString m_hStmtName;
        PGresult * m_hStmtPrep;
        PGresult * m_hStmtExec;
    public:
    VFKStmtPG(CPLString szStmtName, PGresult *hStmtPrep) :
        m_hStmtName(szStmtName),
            m_hStmtPrep(hStmtPrep) {}

        const char * name() const { return m_hStmtName.c_str(); }
        PGresult *   prepared() const { return m_hStmtPrep; }
        PGresult *   exec() const { return m_hStmtExec; }
        
        void setExec(PGresult *hStmtExec) { m_hStmtExec = hStmtExec; }
        void clear() {
            m_hStmtName.clear();
            PQclear(m_hStmtPrep);
            PQclear(m_hStmtExec);
            m_hStmtPrep = m_hStmtExec = NULL;
        }
    };

   PGconn     *m_poDB;
   CPLString   m_pszConnStr;

   OGRErr        ExecuteSQL(PGresult *);
   // TODO: do it better
   std::vector<VFKStmtPG>  m_hStmt;
  
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
