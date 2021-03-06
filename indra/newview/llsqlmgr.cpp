#include "llviewerprecompiledheaders.h"
#include "llsqlmgr.h"
#include <stdio.h>
#include "sqlite3.h"


LLSqlMgr::LLSqlMgr() 
{
	
}
LLSqlMgr::~LLSqlMgr()
{
	
}


char LLSqlMgr::init(std::string db_path) {
   LL_INFOS() << "Init Genesis DB :" << db_path << LL_ENDL;
   char *zErrMsg = 0;
   char *sql;
   int rc;

   rc = sqlite3_open(db_path.c_str(), &db);
   if( rc ) {
    return rc;
   }

    /* Create table contacts set */
   sql = "CREATE TABLE IF NOT EXISTS CONTACTS_SET("  \
      "ID TEXT PRIMARY KEY     NOT NULL," \
      "ALIAS           TEXT    ," \
      "R               REAL    NOT NULL," \
      "G               REAL    NOT NULL," \
      "B               REAL    NOT NULL," \
      "A               REAL    NOT NULL);";
    rc = sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);  
    if( rc ) {
        LL_WARNS() << "Can't initialise Genesis Contacts set table " << zErrMsg << LL_ENDL;
        return rc;
    }

    rc = sqlite3_exec(db, "INSERT INTO CONTACTS_SET VALUES( 'Contact set 1',NULL,1.0,0.0,0.0,1.0)", NULL, NULL, &zErrMsg);
    rc = sqlite3_exec(db, "INSERT INTO CONTACTS_SET VALUES( 'Contact set 2',NULL,0.0,1.0,0.0,1.0)", NULL, NULL, &zErrMsg);
    rc = sqlite3_exec(db, "INSERT INTO CONTACTS_SET VALUES( 'Contact set 3',NULL,0.0,0.0,1.0,1.0)", NULL, NULL, &zErrMsg);
    
   /* Create SQL statement */
   sql = "CREATE TABLE IF NOT EXISTS CONTACT_SET_AVATARS("  \
      "CONTACT_SET_ID  TEXT NOT NULL," \
      "AVATAR_NAME     TEXT NOT NULL," \
      "AVATAR_ID       TEXT PRIMARY KEY     NOT NULL);";
    rc = sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);  
    if( rc ) {
        LL_WARNS() << "Can't initialise Genesis Tagged avatars table " << zErrMsg << LL_ENDL;
        return rc;
    }
}
void LLSqlMgr::close() {

   char *zErrMsg = 0;
   int rc;

   sqlite3_close(db);
}

sqlite3 *LLSqlMgr::getDB() {
    return db;
}

