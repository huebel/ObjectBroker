// Projekt.......: XMLBROKER
// Navn..........: ODBCdatabase.cpp
// Ansvarlig.....: PH

// #define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>

#include "database.h"
#include "datatype.h"
#include "ODBCConnection.h"
#include <tools/trace.h>

#include <exception>

// #pragma comment(lib,"odbc32")

BOOL inline ODBC_OK(RETCODE retcode)
{
  return retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_NO_DATA_FOUND;
}

BOOL inline ODBC_NotFound(RETCODE retcode)
{
  return retcode == SQL_NO_DATA_FOUND;
}

#define ODBC_TRACE_ANY_ERROR(retcode) \
  ODBC_TraceError(hEnv, hDBC, retcode)

void inline ODBC_TraceError(SQLHENV hEnv, SQLHDBC hDBC, RETCODE retcode) {
  if (retcode == SQL_ERROR) {
      SQLCHAR sqlState[512];
      SQLCHAR msgbuf[512];
      SQLSMALLINT msglen = sizeof(msgbuf)-1;
      SQLINTEGER sdwNative;

      SQLError(
          hEnv,
          hDBC,
          (SQLHSTMT)SQL_NULL_HSTMT,
          sqlState,
          &sdwNative,
          msgbuf,
          msglen,
          &msglen
      );
      TRACE((LPCSTR)msgbuf);
      TRACE("\nSQLSTATE='%s'\n", sqlState);
  }
}

int inline ODBC_GetErrorText(HENV hEnv, HDBC hDBC, HSTMT hStmt, char* buffer, int buflen)
{
  SQLINTEGER dwNative;
  SQLSMALLINT cbBuf = buflen;
  SQLCHAR* pszBuf = (SQLCHAR*)buffer;
  SQLSMALLINT cbMsg;
  while (SQL_SUCCESS == SQLError(hEnv,hDBC,hStmt,0,&dwNative,pszBuf,cbBuf,&cbMsg)) {
    cbBuf -= cbMsg;
    if (cbBuf < 1) {
      lstrcpyn(buffer-4, "...", 4);
      break;
    }
    pszBuf+= cbMsg;
    *pszBuf++ = '\n';
  }
  *pszBuf = '\0';
  return buflen - cbBuf;
}

int inline ODBC_Error(HENV hEnv, HDBC hDBC, HSTMT hStmt)
{
  static char szBuf[1024]; const int cbBuf = sizeof(szBuf);
  ODBC_GetErrorText(hEnv,hDBC,hStmt,szBuf,cbBuf);
	throw DBException(szBuf);
  // MessageBox(0, szBuf, "ODBC Error", MB_OK | MB_ICONSTOP | MB_TASKMODAL);
  // return 0;
}

int inline ODBC_GetErrorState(HENV hEnv, HDBC hDBC, char* buffer)
{
  RETCODE retcode = SQLError(hEnv,hDBC,0,(UCHAR FAR *)buffer,0,0,0,0);
  return retcode;
}

class ODBCCursor : public Cursor {
public:
  ODBCCursor(HENV hEnv, HDBC hDBC) : hEnv(hEnv), hDBC(hDBC), iRow(0), dwRowsFetched(0)
  {
    errcode = SQLAllocStmt(hDBC, &hStmt);
    if (!ODBC_OK(errcode))
      hStmt = 0;
  }
  ~ODBCCursor()
  {
    if (hStmt) SQLFreeStmt(hStmt, SQL_DROP);
  }
  virtual int GetErrorCode() const { return errcode; };
  virtual int GetErrorText(char* buffer, int buflen) const
  {
    return ODBC_GetErrorText(hEnv,hDBC,hStmt,buffer,buflen);
  }
  virtual int IsOK() const { return ODBC_OK(errcode); }
  virtual int IsLocked() const { return errcode == -1;}
  virtual int RowsFetched() const { return dwRowsFetched; }
  // SQL
  virtual int Parse(const char* sql)
  {
    errcode = SQLPrepare(hStmt, (UCHAR FAR *)sql, SQL_NTS);
    if (ODBC_OK(errcode))
      return 1;
    else
      return ODBC_Error(hEnv,hDBC,hStmt);
  }
  virtual int Execute()
  {
    errcode = SQLExecute(hStmt);
    if (ODBC_OK(errcode))
      return 1;
    else
      return ODBC_Error(hEnv,hDBC,hStmt);
  }
  virtual int ExecuteArray(int n)
  {
    errcode = SQLParamOptions(hStmt, n, &iRow);
    if (ODBC_OK(errcode)) {
      errcode = SQLExecute(hStmt);
      if (ODBC_OK(errcode))
        return 1;
    }
    return ODBC_Error(hEnv,hDBC,hStmt);
  }
  virtual int Fetch()
  {
    errcode = SQLFetch(hStmt);
    if (ODBC_OK(errcode))
      return 1;
    else
      return ODBC_Error(hEnv,hDBC,hStmt);
  }
  virtual int FetchArray(int n)
  {
    dwRowsFetched = 0;
    UWORD* pwRowStatus = new UWORD[n];
    errcode = SQLSetStmtOption(hStmt, SQL_ROWSET_SIZE, n);
    if (!ODBC_OK(errcode))
      return ODBC_Error(hEnv,hDBC,hStmt);
    errcode = SQLExtendedFetch(hStmt, SQL_FETCH_NEXT, 1, &dwRowsFetched, pwRowStatus);
    int result = ODBC_OK(errcode) || ODBC_NotFound(errcode) ? 1 : ODBC_Error(hEnv,hDBC,hStmt);
    delete [] pwRowStatus;
    return result;
  }
  virtual int BindColumn(int col, void* buffer, int buflen, int datatype, void* indicator)
  {
    errcode = SQLBindCol(hStmt, col, datatype, buffer, buflen, (SDWORD FAR*)indicator);
    if (ODBC_OK(errcode))
      return 1;
    else
      return ODBC_Error(hEnv,hDBC,hStmt);
  }
  virtual int BindBuffer(int pos, void* buffer, int buflen, int datatype, void* indicator)
  {
    errcode = SQLBindParameter(hStmt, pos, SQL_PARAM_INPUT, datatype,
                      datatype, buflen, 0, buffer, buflen, (SDWORD FAR*)indicator);
    if (ODBC_OK(errcode))
      return 1;
    else
      return ODBC_Error(hEnv,hDBC,hStmt);
  }
private:
  SQLHENV  hEnv;
  SQLHDBC  hDBC;
  SQLHSTMT hStmt;
  RETCODE errcode;
  UDWORD iRow;
  UDWORD dwRowsFetched;
};


class ODBCConnection : public Connection {
public:
  ODBCConnection()
  {
    /*
    SQLConfigDataSource(0, ODBC_CONFIG_DSN, 
      "Microsoft Access Driver (*.mdb)",
      "DSN=Taskplanner\0Exclusive=1\0"
    );
    */
    RETCODE retcode = SQLAllocEnv(&hEnv);
    if (ODBC_OK(retcode)) {
      retcode = SQLAllocConnect(hEnv, &hDBC); /* Connection handle */
      if (ODBC_OK(retcode))
        return;
      hDBC = 0;
      SQLFreeEnv(hEnv);
    }
    hEnv = 0;
  }
  ~ODBCConnection()
  {
    if (hDBC) SQLFreeConnect(hDBC);
    if (hEnv) SQLFreeEnv(hEnv);
  }
  virtual int OpenDSN(const char* username, const char* password, const char* DSN)
  {
    // Set login timeout to 5 seconds
    RETCODE retcode = SQLSetConnectOption(hDBC, SQL_LOGIN_TIMEOUT, 5);
    if (!ODBC_OK(retcode))
      return ODBC_Error(hEnv,hDBC,0);
    // Connect to data source		
    retcode = SQLConnect(hDBC, 
      (UCHAR FAR *)DSN, SQL_NTS,
      (UCHAR FAR *)username, SQL_NTS,
      (UCHAR FAR *)password, SQL_NTS
    );   	

/*
    UCHAR szOut[512]; SWORD cbOut;
    RETCODE retcode = SQLDriverConnect(
      hDBC, GetDesktopWindow(),
      (UCHAR*)"", 15,
      szOut, sizeof(szOut), &cbOut,
      SQL_DRIVER_COMPLETE
    );
*/
    if (ODBC_OK(retcode))
      return 1;
    else
      //return 0;
      return ODBC_Error(hEnv,hDBC,0);

  }
  virtual int OpenPath(const char* username, const char* password, const char* DSN, const char* path)
  {
    // Set login timeout to 5 seconds
    RETCODE retcode = SQLSetConnectOption(hDBC, SQL_LOGIN_TIMEOUT, 5);
    if (!ODBC_OK(retcode))
      return ODBC_Error(hEnv,hDBC,0);

    char connect[1024];
//    "DRIVER={%s};DSN='';READONLY=FALSE;CREATE_DB=\"%s\";DBQ=%s;UID=%s;PWD=%s",
    const int connlen = wsprintf(connect, 
      "DSN=%s;"
      "DBQ=%s;"
      "FIL=%s;"
      "UID=%s;PWD=%s;",
      DSN,
      path, 
      path, 
      username, 
      password
    );
    char complet[1024];
//    for (char* s=connect; *s++; ) {
//      if (*s==';') *s++ = 0;
//    }
//    *s = 0;
    SWORD complen = sizeof(complet)-1;
    // Connect to data source
    retcode = SQLDriverConnect(hDBC,
      GetDesktopWindow(), // no dialog
      (SQLCHAR *)connect, connlen,
      (SQLCHAR *)complet, complen, &complen,
      SQL_DRIVER_NOPROMPT
    );
    //ODBC_TRACE_ANY_ERROR(retcode);
    if (ODBC_OK(retcode))
      return 1;
    else
      return 0;
  }
  virtual int Close()
  {
    return ODBC_OK(SQLDisconnect(hDBC));
  }
  virtual Cursor* GetCursor()
  {
    ODBCCursor* c = new ODBCCursor(hEnv,hDBC);
    if (c && c->IsOK())
      return c;
    else
      return NULL;
  }
private:
  SQLHENV    hEnv;
  SQLHDBC    hDBC;
};


class ODBCDataTypes : public DataTypes {
  virtual int operator()(int&)                const { return SQL_INTEGER; }
  virtual int operator()(unsigned int&)       const { return SQL_INTEGER; }
  virtual int operator()(long&)               const { return SQL_INTEGER; }
  virtual int operator()(unsigned long&)      const { return SQL_INTEGER; }
  virtual int operator()(float&)              const { return SQL_FLOAT;   }
  virtual int operator()(double&)             const { return SQL_DOUBLE;  }
  virtual int operator()(char&)               const { return SQL_TINYINT; }
  virtual int operator()(char*)               const { return SQL_CHAR;    }
  virtual int operator()(TIMESTAMP_STRUCT&)	  const	{ return SQL_C_TIMESTAMP; }
};

Connection* GetODBCConnection() 
{ 
  static Connection* conn = new ODBCConnection(); 
  return conn;
}

const DataTypes& GetDataTypes()
{
  static const ODBCDataTypes types;
  return types;
}


static HANDLE hGate = CreateMutex(NULL, FALSE, NULL);

Cursor* GetCursor(class Broker*)
{
	if (WAIT_OBJECT_0==WaitForSingleObject(hGate, INFINITE)) 
		return GetODBCConnection()->GetCursor();
	else
		return 0;
}

void ReleaseCursor(Cursor* c)
{
  delete c;
	ReleaseMutex(hGate);
}

int GetError(Cursor* c, char* buffer, int bufsiz)
{
  return c->GetErrorText(buffer, bufsiz);
}
