/**
 * @file   lwdb.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Tue May 29 15:24:42 2012
 * 
 * @brief  LWQQ Datebase API
 * We use Sqlite3 in this project
 * 
 * 
 */

#ifndef LWDB_H
#define LWDB_H

#include "type.h"
#include "swsqlite.h"

#define LWDB_CACHE_LEN 15

/************************************************************************/
/* Initialization and final API */
/** 
 * LWDB initialization
 * 
 */
void lwdb_global_init();
void lwdb_global_free();

/** 
 * LWDB final
 * 
 */
void lwdb_finalize();

/* Initialization and final API end */

/************************************************************************/
/* LwdbGlobalDB API */
typedef struct LwdbGlobalUserEntry {
    char *qqnumber;
    char *db_name;
    char *password;
    char *status;
    char *rempwd;
    LIST_ENTRY(LwdbGlobalUserEntry) entries;
} LwdbGlobalUserEntry;

typedef struct LwdbGlobalDB {
    SwsDB *db;                  /**< Pointer sqlite3 db */
    LwqqErrorCode (*add_new_user)(struct LwdbGlobalDB *db, const char *qqnumber);
    LwdbGlobalUserEntry * (*query_user_info)(struct LwdbGlobalDB *db,
                                             const char *qqnumber);
    LIST_HEAD(, LwdbGlobalUserEntry) head; /**< QQ friends */
    LwqqErrorCode (*update_user_info)(struct LwdbGlobalDB *db, const char *qqnumber,
                                      const char *key, const char *value);
} LwdbGlobalDB;

/** 
 * Create a global DB object
 * 
 * @return A new global DB object, or NULL if somethins wrong, and store
 * error code in err
 */
LwdbGlobalDB *lwdb_globaldb_new(void);

/** 
 * Free a LwdbGlobalDb object
 * 
 * @param db 
 */
void lwdb_globaldb_free(LwdbGlobalDB *db);

/** 
 * Free LwdbGlobalUserEntry object
 * 
 * @param e 
 */
void lwdb_globaldb_free_user_entry(LwdbGlobalUserEntry *e);

/* LwdbGlobalDB API end */

/************************************************************************/
/* LwdbUserDB API */


typedef struct LwdbUserDB {
    SwsDB *db;
    struct{
        SwsStmt* stmt;
        char* sql;
    }cache[LWDB_CACHE_LEN];
    LwqqBuddy * (*query_buddy_info)(struct LwdbUserDB *db, const char *qqnumber);
    LwqqErrorCode (*update_buddy_info)(struct LwdbUserDB *db, LwqqBuddy *buddy);
} LwdbUserDB;

/** 
 * Create a user DB object
 * 
 * @param qqnumber The qq number
 * 
 * @return A new user DB object, or NULL if somethins wrong, and store
 * error code in err
 */
LwdbUserDB *lwdb_userdb_new(const char *qqnumber,const char* dir,int flags);

/** 
 * Free a LwdbUserDB object
 * 
 * @param db 
 */
void lwdb_userdb_free(LwdbUserDB *db);
/**
 * maybe it is better recognisation
 */
#define lwdb_userdb_close(db) (lwdb_userdb_free(db))

LwqqErrorCode lwdb_userdb_update_buddy_info(LwdbUserDB* db,LwqqBuddy* buddy);
LwqqErrorCode lwdb_userdb_update_group_info(LwdbUserDB* db,LwqqGroup* group);
LwqqErrorCode lwdb_userdb_insert_buddy_info(LwdbUserDB* db,LwqqBuddy* buddy);
LwqqErrorCode lwdb_userdb_insert_group_info(LwdbUserDB* db,LwqqGroup* group);

LwqqErrorCode lwdb_userdb_query_buddy(LwdbUserDB* db,LwqqBuddy* buddy);
LwqqErrorCode lwdb_userdb_query_group(LwdbUserDB* db,LwqqGroup* group);

void lwdb_userdb_write_to_client(LwdbUserDB* from,LwqqClient* to);
void lwdb_userdb_read_from_client(LwqqClient* from,LwdbUserDB* to);
void lwdb_userdb_begin(LwdbUserDB* db);
void lwdb_userdb_commit(LwdbUserDB* db);
void lwdb_userdb_query_qqnumbers(LwqqClient* lc,LwdbUserDB* db);
void lwdb_userdb_flush_buddies(LwdbUserDB* db,int last,int day);
void lwdb_userdb_flush_groups(LwdbUserDB* db,int last,int day);
const char*  lwdb_userdb_read(LwdbUserDB* db,const char* key);
int lwdb_userdb_write(LwdbUserDB* db,const char* key,const char* value);
/* LwdbUserDB API end */

/************************************************************************/

#endif
