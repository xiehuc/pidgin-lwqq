/**
 * @file   swsqlite.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Wed May 30 10:36:42 2012
 * 
 * @brief  Simple wrapper of sqlite
 * 
 * 
 */

#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "swsqlite.h"

#define SET_ERRMSG(a, msg) if(a) *a = strdup(msg)


/** 
 * Open a sqlite3 database
 * 
 * @param filename DB filename
 * @param errmsg If open db failed, a error information will be stored in
 *               errmsg if you pass this parameterment.
 * 
 * @return A new SwsDB object, or NULL if something error happens
 */
SwsDB *sws_open_db(const char *filename, char **errmsg)
{
    int ret;
    sqlite3 *db = NULL;
    
    if (!filename) {
        if (errmsg) {
            SET_ERRMSG(errmsg, "Filename is null");
        }
        return NULL;
    }

    ret = sqlite3_open(filename, &db);
    if (ret != SQLITE_OK) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Open file: %s failed, errcode is %d, %s",
                 filename, ret,sqlite3_errmsg(db));
        SET_ERRMSG(errmsg, msg);
        /**
         * NB: should close db though we open this db failed,
         * or else a memory will leak
         */
        sqlite3_close(db);
        return NULL;
    }
    
    return db;
}

/** 
 * Close a sqlite3 database
 * 
 * @param db 
 * @param errmsg 
 */
void sws_close_db(SwsDB *db, char **errmsg)
{
    int ret;
    
    if (!db) {
        SET_ERRMSG(errmsg, "Filename is null");
        return ;
    }

    ret = sqlite3_close(db);
    if (ret != SQLITE_OK) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Close DB failed: %s", sqlite3_errmsg(db));
        SET_ERRMSG(errmsg, msg);
		return ;
	}
}

/** 
 * Excute a sql sentence
 * 
 * @param db
 * @param sql Sql sentence you want to excute
 * @param errmsg 
 * 
 * @return 0 if everything is ok, else return -1
 */
int sws_exec_sql(SwsDB *db, const char *sql, char **errmsg)
{
    char *err = NULL;
    int ret = 0;

    if (!db || !sql) {
        SET_ERRMSG(errmsg, "Some parameterment is null");
        return -1;
    }

    ret = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (ret != SQLITE_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "\"%s\" failed: %s", sql, err);
        SET_ERRMSG(errmsg, msg);
        sqlite3_free(err);
        return -1;
    }

    return 0;
}

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
int sws_query_start(SwsDB *db, const char *sql, SwsStmt **stmt, char **errmsg)
{
    int ret;
    
    if (!db || !sql || !stmt) {
        SET_ERRMSG(errmsg, "Some parameterment is null");
        return -1;
    }

    ret = sqlite3_prepare_v2(db, sql, -1, (sqlite3_stmt **)stmt, 0);
    if (ret != SQLITE_OK) {
        char msg[512];
        snprintf(msg, sizeof(msg), "\"%s\" failed: %s",
                 sql, sqlite3_errmsg(db));
        SET_ERRMSG(errmsg, msg);
		return -1;
	}

    return 0;
}
int sws_query_bind(SwsStmt *stmt,int index,SwsBindType type,...)
{
    va_list args;
    va_start(args,type);
    switch(type){
        case SWS_BIND_INT:
            sqlite3_bind_int(stmt,index,va_arg(args,int));
            break;
        case SWS_BIND_TEXT:
            {
                const char * text = va_arg(args,const char*);
                if(text == NULL)
                    sqlite3_bind_null(stmt, index);
                else 
                    sqlite3_bind_text(stmt,index,text,strlen(text),SQLITE_TRANSIENT);
            } break;
    }
    va_end(args);
    return 0;
}

int sws_query_reset(SwsStmt* stmt)
{
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
    return 0;
}

/** 
 * Query next result
 * 
 * @param stmt 
 * @param errmsg
 * 
 * @return 0 if query successfully, else return -1 and stored error
 *         information in errmsg if errmsg is not null.
 */
SwsRetCode sws_query_next(SwsStmt *stmt, char **errmsg)
{
    int ret;
    
    if (!stmt) {
        SET_ERRMSG(errmsg, "Some parameterment is null");
        return SWS_FAILED;
    }

    ret = sqlite3_step(stmt);

    if(ret == SQLITE_ROW) return SWS_OK;
    else if (ret == SQLITE_DONE) return SWS_FAILED;
    else if (ret == SQLITE_OK) return SWS_OK;
    else return SWS_FAILED;
}

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
                     char **errmsg)
{
    const unsigned char *result;
    
    if (!stmt || clm_index <0 || !buf || buflen <= 0) {
        SET_ERRMSG(errmsg, "Some parameterment is null");
        return -1;
    }

    result = sqlite3_column_text(stmt, clm_index);
    if (!result) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Query %d column failed", clm_index);
        SET_ERRMSG(errmsg, msg);
        return -1;
    }

    snprintf(buf, buflen, "%s", result);
    return 0;
}

/** 
 * 
 * 
 * @param stmt 
 * @param errmsg 
 * 
 * @return 0 if end successfully, else return -1 and stored error
 *         information in errmsg if errmsg is not null.
 */
int sws_query_end(SwsStmt *stmt, char **errmsg)
{
    int ret;
    
    if (!stmt) {
        SET_ERRMSG(errmsg, "Some parameterment is null");
        return -1;
    }
    
    ret = sqlite3_finalize(stmt);

    if (ret != SQLITE_OK) {
        char msg[64];
        snprintf(msg, sizeof(msg), "End query failed, errcode: %d", ret);
        SET_ERRMSG(errmsg, msg);
        return -1;
    }
    
    return 0;
}

/** 
 * Excute a SQL directly, dont need to open DB and close DB
 * 
 * @param filename 
 * @param sql 
 * @param errmsg 
 * 
 * @return 0 if excute successfully, else return -1.
 */
int sws_exec_sql_directly(const char *filename, const char *sql, char **errmsg)
{
    int ret;
    SwsDB *db = NULL;
    
    if (!filename || !sql) {
        SET_ERRMSG(errmsg, "Some parameterment is null");
        goto failed;
    }

    db = sws_open_db(filename, errmsg);
    if (!db) {
        perror("Error:");
        goto failed;
    }

    ret = sws_exec_sql(db, sql, errmsg);
    if (ret) {
        goto failed;
    }

    sws_close_db(db, NULL);

    return 0;

failed:
    if (db)
        sws_close_db(db, NULL);
    return -1;    
}

#if 0
/* gcc -o test swsqlite.c -Wall -lsqlite3 */
#include <stdlib.h>
int main(int argc, char *argv[])
{
    int ret;
    SwsStmt *stmt = NULL;

    /* Open DB */
    SwsDB *db = sws_open_db("/tmp/test_sws.db", NULL);
    if (!db) {
        return -1;
    }

    /* Excute SQL */
    ret = sws_exec_sql(db, "create table if not exists config("
                       "id integer primary key asc autoincrement,"
                       "family,key,value); "
                       "INSERT into config (family, key, value) "
                       "VALUES('sws', 'version', '0.0.1');"
                       "INSERT into config (family, key, value) "
                       "VALUES('sws', 'status', 'test');", NULL);
    if (ret) {
        goto failed;
    }

    /* Query */
    ret = sws_query_start(db, "SELECT key,value FROM config;", &stmt, NULL);
    if (ret) {
        goto failed;
    }
    while (!sws_query_next(stmt, NULL)) {
        char key[128], value[128];
        sws_query_column(stmt, 0, key, sizeof(key), NULL);
        sws_query_column(stmt, 1, value, sizeof(value), NULL);
        printf ("%s=%s\n", key, value);
    }
    sws_query_end(stmt, NULL);
    
    /* Close DB */
    sws_close_db(db, NULL);

    /* Excute SQL directly */
    sws_exec_sql_directly("/tmp/test_sws.db", "INSERT INTO configs "
                          "(family,key,value) VALUES('1', '2', '3');", NULL);
    
    return 0;

failed:
    sws_close_db(db, NULL);
    return -1;
}
#endif
