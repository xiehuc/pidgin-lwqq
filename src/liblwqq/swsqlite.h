/**
 * @file   swsqlite.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Wed May 30 10:37:01 2012
 *
 * @brief  Simple wrapper of sqlite
 * 
 * 
 */

#ifndef SSQLITE_H
#define SSQLITE_H

typedef void SwsDB;
typedef void SwsStmt;

typedef enum {
    SWS_BIND_INT,
    SWS_BIND_TEXT
}SwsBindType;

typedef enum {
    SWS_OK,             /* there are no error or there are data */
    SWS_FAILED,         /* failed */
    //SWS_NEXT            /* when there are next line */
}SwsRetCode;
/** 
 * Open a sqlite3 database
 * 
 * @param filename DB filename
 * @param errmsg If open db failed, a error information will be stored in
 *               errmsg if you pass this parameterment.
 * 
 * @return A new SwsDB object, or NULL if something error happens
 */
SwsDB *sws_open_db(const char *filename, char **errmsg);

/** 
 * Close a sqlite3 database
 * 
 * @param db 
 * @param errmsg 
 */
void sws_close_db(SwsDB *db, char **errmsg);

/** 
 * Excute a sql sentence
 * 
 * @param db
 * @param sql Sql sentence you want to excute
 * @param errmsg 
 * 
 * @return 0 if everything is ok, else return -1
 */
int sws_exec_sql(SwsDB *db, const char *sql, char **errmsg);

/************************************************************************/
/**
 * Below is query API. If you want to query something from sqlite3 database.
 * Yuo just only call these API like this:
 * // open db;
 * SwsStmt *stmt = NULL;
 * char *sql = "SELECT key,value FROM config;"
 * sws_query_start(db, sql, &stmt, NULL);
 * while (!sws_query_next(stmt)) {
 *     char key[128], value[128];
 *     sws_query_column(stmt, 0, key, sizeof(key), NULL);
 *     sws_query_column(stmt, 1, value, sizeof(value), NULL);
 *     // Handle something
 * }
 * sws_query_end(stmt, NULL);
 *
 * Just so easy
 */

/** 
 * Start a query
 * 
 * @param db 
 * @param sql 
 * @param stmt 
 * @param errmsg 
 * 
 * @return 0 if start successfully, else return -1 and stored error
 *         information in errmsg if errmsg is not null.
 */
int sws_query_start(SwsDB *db, const char *sql, SwsStmt **stmt, char **errmsg);


/** index starts from 1 */
int sws_query_bind(SwsStmt *stmt,int index,SwsBindType type,...);

int sws_query_reset(SwsStmt* stmt);

/** 
 * Query next result
 * 
 * @param stmt 
 * @param errmsg
 * 
 * @return 0 if query successfully, else return -1 and stored error
 *         information in errmsg if errmsg is not null.
 */
SwsRetCode sws_query_next(SwsStmt *stmt, char **errmsg);

/** 
 * 
 * 
 * @param stmt 
 * @param clm_index Column number
 * @param buf Store query result
 * @param buflen Max length of buffer which store query result in
 * @param errmsg 
 * 
 * @return 0 if query successfully, else return -1 and stored error
 *         information in errmsg if errmsg is not null.
 */
int sws_query_column(SwsStmt *stmt, int clm_index, char *buf, int buflen,
                     char **errmsg);
/** 
 * 
 * 
 * @param stmt 
 * @param errmsg 
 * 
 * @return 0 if end successfully, else return -1 and stored error
 *         information in errmsg if errmsg is not null.
 */
int sws_query_end(SwsStmt *stmt, char **errmsg);

/** 
 * Excute a SQL directly, dont need to open DB and close DB
 * 
 * @param filename 
 * @param sql 
 * @param errmsg 
 * 
 * @return 0 if excute successfully, else return -1.
 */
int sws_exec_sql_directly(const char *filename, const char *sql, char **errmsg);

#endif
