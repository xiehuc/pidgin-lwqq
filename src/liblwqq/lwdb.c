/**
 * @file   lwdb.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Tue May 29 15:24:42 2012
 * 
 * @brief  LWQQ Database API
 * We use Sqlite3 in this project.
 *
 * Description: There are has two types database in LWQQ, one is
 * global, other one depends on user. e.g. if we have two qq user(A and B),
 * then we should have three database, lwqq.sqlie (for global),
 * a.sqlite(for user A), b.sqlite(for user B).
 * 
 * 
 */

#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>

#include "smemory.h"
#include "logger.h"
#include "swsqlite.h"
#include "lwdb.h"

#define DB_PATH "/tmp/lwqq"

static LwqqErrorCode lwdb_globaldb_add_new_user(
    struct LwdbGlobalDB *db, const char *qqnumber);
static LwdbGlobalUserEntry *lwdb_globaldb_query_user_info(
    struct LwdbGlobalDB *db, const char *qqnumber);
static LwqqErrorCode lwdb_globaldb_update_user_info(
    struct LwdbGlobalDB *db, const char *qqnumber,
    const char *key, const char *value);

static LwqqBuddy *lwdb_userdb_query_buddy_info(
    struct LwdbUserDB *db, const char *qqnumber);
static LwqqErrorCode lwdb_userdb_update_buddy_info(
    struct LwdbUserDB *db, LwqqBuddy *buddy);

static char *database_path;
static char *global_database_name;

///when save friend info need update version
//to clean old error
#define LWDB_VERSION 1001
#define VAL(v) #v
#define STR(v) VAL(v)

static const char *create_global_db_sql =
    "create table if not exists configs("
    "    id integer primary key asc autoincrement,"
    "    family default '',"
    "    key default '',"
    "    value default '');"
    "create table if not exists users("
    "    qqnumber primary key,"
    "    db_name default '',"
    "    password default '',"
    "    status default 'offline',"
    "    rempwd default '1');";

static const char *create_user_db_sql =
    "create table if not exists buddies("
    "    qqnumber primary key not null,"
    "    face default '',"
    "    occupation default '',"
    "    phone default '',"
    "    allow default '',"
    "    college default '',"
    "    reg_time default '',"
    "    constel default '',"
    "    blood default '',"
    "    homepage default '',"
    "    stat int default 0,"
    "    country default '',"
    "    city default '',"
    "    personal default '',"
    "    nick default '',"
    "    shengxiao default '',"
    "    email default '',"
    "    province default '',"
    "    gender default '',"
    "    mobile default '',"
    "    vip_info default '',"
    "    markname default '',"
    "    flag default '',"
    "    cate_index int default 0,"
    "    last_modify timestamp default 0);"
    
    "create table if not exists categories("
    "    name primary key,"
    "    cg_index default '',"
    "    sort default '');"

    "create table if not exists groups("
    "    account primary key not null,"
    "    name default '',"
    "    markname default '',"
    "    face default '',"
    "    memo default '',"
    "    class default '',"
    "    fingermemo default '',"
    "    createtime default '',"
    "    level default '',"
    "    owner default '',"
    "    flag default '',"
    "    mask int default 0);"

    "create table if not exists pairs("
    "   key primary key not null,"
    "   value default '');"
    ;

static const char* init_user_db_sql = 
    "insert into pairs (key,value) values ('version','"STR(LWDB_VERSION)"');";


static const char* buddy_query_sql = 
    "SELECT face,occupation,phone,allow,college,reg_time,constel,"
    "blood,homepage,stat,country,city,personal,nick,shengxiao,"
    "email,province,gender,mobile,vip_info,markname,flag,"
    "cate_index,qqnumber FROM buddies ;";

static const char* group_query_sql = 
    "SELECT account,name,markname FROM groups ;";


/** 
 * LWDB initialization
 * 
 */
void lwdb_global_init()
{
    char buf[256];
    char *home;

    home = getenv("HOME");
    if (!home) {
        lwqq_log(LOG_ERROR, "Cant get $HOME, exit\n");
        exit(1);
    }
    
    snprintf(buf, sizeof(buf), "%s/.config/lwqq", home);
    database_path = s_strdup(buf);
    if (access(database_path, F_OK)) {
        /* Create a new config directory if we dont have */
        mkdir(database_path, 0777);
    }
    
    snprintf(buf, sizeof(buf), "%s/lwqq.db", database_path);
    global_database_name = s_strdup(buf);
}

void lwdb_global_free()
{
}

/** 
 * LWDB finalize
 * 
 */
void lwdb_finalize()
{
    s_free(database_path);
    s_free(global_database_name);
}

/** 
 * Create database for lwqq
 * 
 * @param filename 
 * @param db_type Type of database you want to create. 0 means
 *        global database, 1 means user database
 * 
 * @return 0 if everything is ok, else return -1
 */
static int lwdb_create_db(const char *filename, int db_type)
{
    int ret;
    char *errmsg = NULL;
    
    if (!filename) {
        return -1;
    }

    if (access(filename, F_OK) == 0) {
        lwqq_log(LOG_WARNING, "Find a file whose name is same as file "
                 "we want to create, delete it.\n");
        unlink(filename);
    }else{
        //create parent dir
	char* dir = s_strdup(filename);
	char* end = strrchr(dir,'/');
	if(end){
	   *end = '\0';
	   mkdir(dir,0755);
	}
	s_free(dir);
    }
    if (db_type == 0) {
        ret = sws_exec_sql_directly(filename, create_global_db_sql, &errmsg);
    } else if (db_type == 1) {
        ret = sws_exec_sql_directly(filename, create_user_db_sql, &errmsg);
    } else {
        ret = -1;
    }
    if(ret >= 0) {
        chmod(filename,0666);
    }
    if(db_type ==1){
        ret = sws_exec_sql_directly(filename, init_user_db_sql,&errmsg);
    }else ret = -1;

    if (errmsg) {
        lwqq_log(LOG_ERROR, "%s\n", errmsg);
        s_free(errmsg);
    }
    return ret;
}

/** 
 * Check whether db is valid
 * 
 * @param filename The db name
 * @param type 0 means db is a global db, 1 means db is a user db
 * 
 * @return 1 if db is valid, else return 0
 */
static int db_is_valid(const char *filename, int type)
{
    int ret;
    SwsStmt *stmt = NULL;
    SwsDB *db = NULL;
    char *sql;

    /* Check whether file exists */
    if (!filename || access(filename, F_OK)) {
        goto invalid;
    }
    
    /* Open DB */
    db = sws_open_db(filename, NULL);
    if (!db) {
        goto invalid;
    }
    
    /* Query DB */
    if (type == 0) {
        sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='configs';";
    } else {
        sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='pairs';";
    }
    ret = sws_query_start(db, sql, &stmt, NULL);
    if (ret) {
        goto invalid;
    }
    if (sws_query_next(stmt, NULL)) {
        goto invalid;
    }
    sws_query_end(stmt, NULL);

    if(type == 1){
        sws_query_start(db, "SELECT value FROM pairs WHERE key='version' AND value='"STR(LWDB_VERSION)"';",&stmt,NULL);
        if(sws_query_next(stmt,NULL)) {
            lwqq_puts("userdb version not matched,recreate it");
            goto invalid;
        }
        sws_query_end(stmt,NULL);
    }

    /* Close DB */
    sws_close_db(db, NULL);

    /* OK, it is a valid db */
    return 1;
    
invalid:
    sws_close_db(db, NULL);
    return 0;
}

/** 
 * Create a global DB object
 * 
 * @return A new global DB object, or NULL if somethins wrong, and store
 * error code in err
 */

LwdbGlobalDB *lwdb_globaldb_new(void)
{
    LwdbGlobalDB *db = NULL;
    int ret;
    char sql[256];
    SwsStmt *stmt = NULL;
    
    if (!db_is_valid(global_database_name, 0)) {
        ret = lwdb_create_db(global_database_name, 0);
        if (ret) {
            goto failed;
        }
    }

    db = s_malloc0(sizeof(*db));
    db->db = sws_open_db(global_database_name, NULL);
    if (!db->db) {
        goto failed;
    }
    db->add_new_user = lwdb_globaldb_add_new_user;
    db->query_user_info = lwdb_globaldb_query_user_info;
    db->update_user_info = lwdb_globaldb_update_user_info;

    snprintf(sql, sizeof(sql), "SELECT qqnumber,db_name,password,"
             "status,rempwd FROM users;");
    lwqq_log(LOG_DEBUG, "%s\n", sql);
    ret = sws_query_start(db->db, sql, &stmt, NULL);
    if (ret) {
        lwqq_log(LOG_ERROR, "Failed to %s\n", sql);
        goto failed;
    }

    while (!sws_query_next(stmt, NULL)) {
        LwdbGlobalUserEntry *e = s_malloc0(sizeof(*e));
        char buf[256] = {0};
#define LWDB_GLOBALDB_NEW_MACRO(i, member) {                    \
            sws_query_column(stmt, i, buf, sizeof(buf), NULL);  \
            e->member = s_strdup(buf);                          \
        }
        LWDB_GLOBALDB_NEW_MACRO(0, qqnumber);
        LWDB_GLOBALDB_NEW_MACRO(1, db_name);
        LWDB_GLOBALDB_NEW_MACRO(2, password);
        LWDB_GLOBALDB_NEW_MACRO(3, status);
        LWDB_GLOBALDB_NEW_MACRO(4, rempwd);
#undef LWDB_GLOBALDB_NEW_MACRO
        lwqq_log(LOG_DEBUG, "qqnumber:%s, db_name:%s, password:%s, status:%s, "
                 "rempwd:%s\n", e->qqnumber, e->db_name, e->password, e->status);
        LIST_INSERT_HEAD(&db->head, e, entries);
    }
    sws_query_end(stmt, NULL);
    return db;

failed:
    sws_query_end(stmt, NULL);
    lwdb_globaldb_free(db);
    return NULL;
}

/** 
 * Free a LwdbGlobalDb object
 * 
 * @param db 
 */
void lwdb_globaldb_free(LwdbGlobalDB *db)
{
    LwdbGlobalUserEntry *e, *e_next;
    if (!db)
        return ;
    
    sws_close_db(db->db, NULL);

    LIST_FOREACH_SAFE(e, &db->head, entries, e_next) {
        LIST_REMOVE(e, entries);
        lwdb_globaldb_free_user_entry(e);
    }
    s_free(db);
}

/** 
 * Free LwdbGlobalUserEntry object
 * 
 * @param e 
 */
void lwdb_globaldb_free_user_entry(LwdbGlobalUserEntry *e)
{
    if (e) {
        s_free(e->qqnumber);
        s_free(e->db_name);
        s_free(e->password);
        s_free(e->status);
        s_free(e->rempwd);
        s_free(e);
    }
}

/** 
 * 
 * 
 * @param db 
 * @param qqnumber 
 * 
 * @return LWQQ_EC_OK on success, else return LWQQ_EC_DB_EXEC_FAIELD on failure
 */
static LwqqErrorCode lwdb_globaldb_add_new_user(
    struct LwdbGlobalDB *db, const char *qqnumber)
{
    char *errmsg = NULL;
    char sql[256];

    if (!qqnumber){
        return LWQQ_EC_NULL_POINTER;
    }
    
    snprintf(sql, sizeof(sql), "INSERT INTO users (qqnumber,db_name) "
             "VALUES('%s','%s/%s.db');", qqnumber, database_path, qqnumber);
    sws_exec_sql(db->db, sql, &errmsg);
    if (errmsg) {
        lwqq_log(LOG_ERROR, "Add new user error: %s\n", errmsg);
        s_free(errmsg);
        return LWQQ_EC_DB_EXEC_FAIELD;
    }

    return LWQQ_EC_OK;
}

static LwdbGlobalUserEntry *lwdb_globaldb_query_user_info(
    struct LwdbGlobalDB *db, const char *qqnumber)
{
    int ret;
    char sql[256];
    LwdbGlobalUserEntry *e = NULL;
    SwsStmt *stmt = NULL;

    if (!qqnumber) {
        return NULL;
    }

    snprintf(sql, sizeof(sql), "SELECT db_name,password,status,rempwd "
             "FROM users WHERE qqnumber='%s';", qqnumber);
    ret = sws_query_start(db->db, sql, &stmt, NULL);
    if (ret) {
        goto failed;
    }

    if (!sws_query_next(stmt, NULL)) {
        e = s_malloc0(sizeof(*e));
        char buf[256] = {0};
#define GET_MEMBER_VALUE(i, member) {                           \
            sws_query_column(stmt, i, buf, sizeof(buf), NULL);  \
            e->member = s_strdup(buf);                          \
        }
        e->qqnumber = s_strdup(qqnumber);
        GET_MEMBER_VALUE(0, db_name);
        GET_MEMBER_VALUE(1, password);
        GET_MEMBER_VALUE(2, status);
        GET_MEMBER_VALUE(3, rempwd);
#undef GET_MEMBER_VALUE
    }
    sws_query_end(stmt, NULL);

    return e;

failed:
    s_free(e);
    sws_query_end(stmt, NULL);
    return NULL;
}

static LwqqErrorCode lwdb_globaldb_update_user_info(
    struct LwdbGlobalDB *db, const char *qqnumber,
    const char *key, const char *value)
{
    char sql[256];
    char *err = NULL;
    
    if (!qqnumber || !key || !value) {
        return LWQQ_EC_NULL_POINTER;
    }

    snprintf(sql, sizeof(sql), "UPDATE users SET %s='%s' WHERE qqnumber='%s';",
             key, value, qqnumber);
    if (!sws_exec_sql(db->db, sql, &err)) {
        lwqq_log(LOG_DEBUG, "%s successfully\n", sql);
        return LWQQ_EC_DB_EXEC_FAIELD;
    } else {
        lwqq_log(LOG_ERROR, "Failed to %s: %s\n", sql, err);
        s_free(err);
    }
    
    return LWQQ_EC_OK;
}

LwdbUserDB *lwdb_userdb_new(const char *qqnumber,const char* dir)
{
    LwdbUserDB *udb = NULL;
    int ret;
    char db_name[64];
    
    if (!qqnumber) {
        return NULL;
    }

    if(dir == NULL){
        char* home = getenv("HOME");
        snprintf(db_name,sizeof(db_name),"%s/.config/lwqq/%s.db",home,qqnumber);
    }else{
        snprintf(db_name,sizeof(db_name),"%s/%s.db",dir,qqnumber);
    }

    /* If there is no db named "db_name", create it */
    if (!db_is_valid(db_name, 1)) {
        lwqq_log(LOG_WARNING, "db doesnt exist, create it\n");
        ret = lwdb_create_db(db_name, 1);
        if (ret) {
            goto failed;
        }
    }

    udb = s_malloc0(sizeof(*udb));
    udb->db = sws_open_db(db_name, NULL);
    if (!udb->db) {
        goto failed;
    }
    udb->query_buddy_info = lwdb_userdb_query_buddy_info;
    udb->update_buddy_info = lwdb_userdb_update_buddy_info;

    //sws_exec_sql(udb->db, "BEGIN;", NULL);

    return udb;

failed:
    lwdb_userdb_free(udb);
    return NULL;
}

void lwdb_userdb_free(LwdbUserDB *db)
{
    if (db) {
        //sws_exec_sql(db->db,"COMMIT;",NULL);
        sws_close_db(db->db, NULL);
        s_free(db);
    }
}

static LwqqBuddy* read_buddy_from_stmt(SwsStmt* stmt)
{
    LwqqBuddy* buddy;
    buddy = s_malloc0(sizeof(*buddy));
    char buf[256] = {0};
#define GET_BUDDY_MEMBER_VALUE(i, member) {                     \
            sws_query_column(stmt, i, buf, sizeof(buf), NULL);      \
            buddy->member = s_strdup(buf);                          \
        }
#define GET_BUDDY_MEMBER_INT(i,member) {                        \
    sws_query_column(stmt, i, buf, sizeof(buf), NULL);          \
    buddy->member = atoi(buf);                                  \
}
        GET_BUDDY_MEMBER_VALUE(0, face);
        GET_BUDDY_MEMBER_VALUE(1, occupation);
        GET_BUDDY_MEMBER_VALUE(2, phone);
        GET_BUDDY_MEMBER_VALUE(3, allow);
        GET_BUDDY_MEMBER_VALUE(4, college);
        GET_BUDDY_MEMBER_VALUE(5, reg_time);
        GET_BUDDY_MEMBER_VALUE(6, constel);
        GET_BUDDY_MEMBER_VALUE(7, blood);
        GET_BUDDY_MEMBER_VALUE(8, homepage);
        GET_BUDDY_MEMBER_INT(9, stat);
        //GET_BUDDY_MEMBER_VALUE(9, stat);
        GET_BUDDY_MEMBER_VALUE(10, country);
        GET_BUDDY_MEMBER_VALUE(11, city);
        GET_BUDDY_MEMBER_VALUE(12, personal);
        GET_BUDDY_MEMBER_VALUE(13, nick);
        GET_BUDDY_MEMBER_VALUE(14, shengxiao);
        GET_BUDDY_MEMBER_VALUE(15, email);
        GET_BUDDY_MEMBER_VALUE(16, province); 
        GET_BUDDY_MEMBER_VALUE(17, gender);
        GET_BUDDY_MEMBER_VALUE(18, mobile);
        GET_BUDDY_MEMBER_VALUE(19, vip_info);
        GET_BUDDY_MEMBER_VALUE(20, markname);
        GET_BUDDY_MEMBER_VALUE(21, flag);
        GET_BUDDY_MEMBER_INT(22, cate_index);
        GET_BUDDY_MEMBER_VALUE(23, qqnumber);
#undef GET_BUDDY_MEMBER_VALUE
#undef GET_BUDDY_MEMBER_INT
        return buddy;
}

static LwqqGroup* read_group_from_stmt(SwsStmt* stmt)
{
    LwqqGroup* group = s_malloc0(sizeof(*group));
    char buf[256] = {0};
#define GET_GROUP_MEMBER_VALUE(i, member) {                     \
            sws_query_column(stmt, i, buf, sizeof(buf), NULL);      \
            group->member = s_strdup(buf);                          \
        }
#define GET_GROUP_MEMBER_INT(i,member) {                        \
            sws_query_column(stmt, i, buf, sizeof(buf), NULL);          \
            group->member = atoi(buf);                                  \
        }
    GET_GROUP_MEMBER_VALUE(0,account);
    GET_GROUP_MEMBER_VALUE(1,name);
    GET_GROUP_MEMBER_VALUE(2,markname);
#undef GET_GROUP_MEMBER_VALUE
#undef GET_GROUP_MEMBER_INT
    return group;
}
/** 
 * Query buddy's information
 * 
 * @param db 
 * @param qqnumber The key we used to query info from DB
 * 
 * @return A LwqqBuddy object on success, or NULL on failure
 */
static LwqqBuddy *lwdb_userdb_query_buddy_info(
    struct LwdbUserDB *db, const char *qqnumber)
{
    int ret;
    char sql[256];
    LwqqBuddy *buddy = NULL;
    SwsStmt *stmt = NULL;

    if (!qqnumber) {
        return NULL;
    }

    snprintf(sql, sizeof(sql),
             "SELECT face,occupation,phone,allow,college,reg_time,constel,"
             "blood,homepage,stat,country,city,personal,nick,shengxiao,"
             "email,province,gender,mobile,vip_info,markname,flag,"
             "cate_index,qqnumber FROM buddies WHERE qqnumber='%s';", qqnumber);
    ret = sws_query_start(db->db, sql, &stmt, NULL);
    if (ret) {
        goto failed;
    }

    if (!sws_query_next(stmt, NULL)) {
        buddy = read_buddy_from_stmt(stmt);
    }
    sws_query_end(stmt, NULL);

    return buddy;

failed:
    lwqq_buddy_free(buddy);
    sws_query_end(stmt, NULL);
    return NULL;
}
LwqqErrorCode lwdb_userdb_insert_buddy_info(LwdbUserDB* db,LwqqBuddy* buddy)
{
    if(!db || !buddy ) return -1;
    if(!buddy->qqnumber) return -1;
    static SwsStmt* stmt = NULL, *stmt2;

    sws_query_start(db->db, "INSERT INTO buddies ("
            "qqnumber,nick,markname) VALUES (?,?,?);", &stmt, NULL);
    sws_query_start(db->db, "UPDATE buddies SET "
            "nick=? , markname=? WHERE qqnumber=?;",&stmt2,NULL);

    sws_query_reset(stmt);
    sws_query_bind(stmt,1,SWS_BIND_TEXT,buddy->qqnumber);
    sws_query_bind(stmt,2,SWS_BIND_TEXT,buddy->nick);
    sws_query_bind(stmt,3,SWS_BIND_TEXT,buddy->markname);
    if(sws_query_next(stmt,NULL) == SWS_OK) goto done;
    sws_query_reset(stmt2);
    sws_query_bind(stmt2,1,SWS_BIND_TEXT,buddy->nick);
    sws_query_bind(stmt2,2,SWS_BIND_TEXT,buddy->markname);
    sws_query_bind(stmt2,3,SWS_BIND_TEXT,buddy->qqnumber);
    sws_query_next(stmt2,NULL);
done:
    sws_query_end(stmt, NULL);
    sws_query_end(stmt2, NULL);
    return 0;
}
LwqqErrorCode lwdb_userdb_insert_group_info(LwdbUserDB* db,LwqqGroup* group)
{
    if(!db || !group) return -1;
    if(! group->account) return -1;
    SwsStmt* stmt = NULL, *stmt2;

    sws_query_start(db->db,"INSERT INTO groups ("
            "account,name,markname) VALUES (?,?,?);",&stmt,NULL);
    sws_query_start(db->db,"UPDATE groups SET "
            "name=? , markname=? WHERE account=?;",&stmt2,NULL);

    sws_query_reset(stmt);
    sws_query_bind(stmt,1,SWS_BIND_TEXT,group->account);
    sws_query_bind(stmt,2,SWS_BIND_TEXT,group->name);
    sws_query_bind(stmt,3,SWS_BIND_TEXT,group->markname);
    if(sws_query_next(stmt,NULL) == SWS_OK) return 0;
    sws_query_reset(stmt2);
    sws_query_bind(stmt2,1,SWS_BIND_TEXT,group->name);
    sws_query_bind(stmt2,2,SWS_BIND_TEXT,group->markname);
    sws_query_bind(stmt2,3,SWS_BIND_TEXT,group->account);
    sws_query_next(stmt2,NULL);

    sws_query_end(stmt, NULL);
    sws_query_end(stmt2, NULL);
    return 0;
}
static LwqqErrorCode lwdb_userdb_update_buddy_info(
    struct LwdbUserDB *db, LwqqBuddy *buddy)
{
    char sql[4096] = {0};
    int sqllen = 0;
    
    if (!buddy || !buddy->qqnumber) {
        return LWQQ_EC_NULL_POINTER;
    }

    snprintf(sql, sizeof(sql), "UPDATE buddies SET qqnumber='%s'", buddy->qqnumber);
    sqllen = strlen(sql);

#define UBI_CONSTRUCT_SQL(member) {                                     \
    if (buddy->member) {                                            \
        snprintf(sql + sqllen, sizeof(sql) - sqllen, ",%s='%s'", #member, buddy->member); \
        sqllen = strlen(sql);                                       \
        if (sqllen > (sizeof(sql) - 128)) {                         \
            return LWQQ_EC_ERROR;                                   \
        }                                                           \
    }                                                               \
}
#define UBI_CONSTRUCT_SQL_INT(member) {                                 \
    if(buddy->member) {                                                 \
        snprintf(sql+sqllen,sizeof(sql) - sqllen,",%s=%d",#member,buddy->member); \
        sqllen = strlen(sql);                                           \
        if(sqllen > (sizeof(sql) - 128 )) return LWQQ_EC_ERROR;         \
    }                                                                   \
}
    UBI_CONSTRUCT_SQL(face);
    UBI_CONSTRUCT_SQL(phone);
    UBI_CONSTRUCT_SQL(allow);
    UBI_CONSTRUCT_SQL(college);
    UBI_CONSTRUCT_SQL(reg_time);
    UBI_CONSTRUCT_SQL(constel);
    UBI_CONSTRUCT_SQL(blood);
    UBI_CONSTRUCT_SQL(homepage);
    UBI_CONSTRUCT_SQL_INT(stat);
    UBI_CONSTRUCT_SQL(country);
    UBI_CONSTRUCT_SQL(city);
    UBI_CONSTRUCT_SQL(personal);
    UBI_CONSTRUCT_SQL(nick);
    UBI_CONSTRUCT_SQL(shengxiao);
    UBI_CONSTRUCT_SQL(email);
    UBI_CONSTRUCT_SQL(province);
    UBI_CONSTRUCT_SQL(gender);
    UBI_CONSTRUCT_SQL(mobile);
    UBI_CONSTRUCT_SQL(vip_info);
    UBI_CONSTRUCT_SQL(markname);
    UBI_CONSTRUCT_SQL(flag);
    UBI_CONSTRUCT_SQL_INT(cate_index);
    UBI_CONSTRUCT_SQL_INT(client_type);
#undef UBI_CONSTRUCT_SQL
#undef UBI_CONSTRUCT_SQL_INT
    snprintf(sql + sqllen, sizeof(sql) - sqllen, " WHERE qqnumber='%s';", buddy->qqnumber);
    if (!sws_exec_sql(db->db, sql, NULL)) {
        return LWQQ_EC_DB_EXEC_FAIELD;
    }

    return LWQQ_EC_OK;
}
static LwqqBuddy* find_buddy_by_nick_and_mark(LwqqClient* lc,const char* nick,const char* mark)
{
    if(!lc || !nick ) return NULL;
    LwqqBuddy * buddy = NULL;
    if(mark){
        LIST_FOREACH(buddy,&lc->friends,entries){
            if(buddy->markname && !strcmp(buddy->markname,mark) &&
                    buddy->nick && !strcmp(buddy->nick,nick))
                return buddy;
            else if(buddy->nick && !strcmp(buddy->nick,nick) )
                return buddy;
        }
    }else{
        LIST_FOREACH(buddy,&lc->friends,entries){
            if(buddy->nick && !strcmp(buddy->nick,nick))
                return buddy;
        }
    }
    return NULL;
}
static LwqqGroup* find_group_by_account(LwqqClient* lc, const char* account)
{
    if(!lc || !account) return NULL;
    LwqqGroup* group = NULL;
    LIST_FOREACH(group,&lc->groups,entries){
        if(group->account && !strcmp(group->account,account) )
            return group;
    }
    return NULL;
}
static LwqqGroup* find_group_by_name_and_mark(LwqqClient* lc,const char* name,const char* mark)
{
    if(!lc || !name) return NULL;
    LwqqGroup* group = NULL;
    if(mark){
        LIST_FOREACH(group,&lc->groups,entries){
            if(group->markname && !strcmp(group->markname,mark) &&
                    group->name && !strcmp(group->name,name) )
                return group;
            else if(group->name && !strcmp(group->name,name) )
                return group;
        }
    }else{
        LIST_FOREACH(group,&lc->groups,entries){
            if(group->name && ! strcmp(group->name,name) )
                return group;
        }
    }
    return NULL;
}
static void buddy_merge(LwqqBuddy* into,LwqqBuddy* from)
{
#define MERGE_TEXT(t,f) if(f&&strcmp(f,"")){s_free(t);t = s_strdup(f);}
#define MERGE_INT(t,f) (t = f)
    MERGE_TEXT(into->qqnumber,from->qqnumber);
#undef MERGE_TEXT
#undef MERGE_INT
}
static void group_merge(LwqqGroup* into,LwqqGroup* from)
{
#define MERGE_TEXT(t,f) if(f){s_free(t);t = s_strdup(f);}
#define MERGE_INT(t,f) (t = f)
    MERGE_TEXT(into->account,from->account);
#undef MERGE_TEXT
#undef MERGE_INT
}
void lwdb_userdb_write_to_client(LwdbUserDB* from,LwqqClient* to)
{
    if(!from || !to) return;
    LwqqBuddy *buddy = NULL,*target = NULL;
    LwqqGroup *group = NULL,*gtarget = NULL;
    SwsStmt *stmt = NULL;
    LwqqClient* lc = to;

    sws_query_start(from->db, buddy_query_sql, &stmt, NULL);

    while (!sws_query_next(stmt, NULL)) {
        buddy = read_buddy_from_stmt(stmt);
        target = lc->find_buddy_by_qqnumber(lc, buddy->qqnumber);
        if(target == NULL)
            target = find_buddy_by_nick_and_mark(lc,buddy->nick,buddy->markname);
        if(target){
            buddy_merge(target,buddy);
            lwqq_buddy_free(buddy);
        }else{
            //LIST_INSERT_HEAD(&lc->friends,buddy,entries);
        }
    }
    sws_query_end(stmt, NULL);

    sws_query_start(from->db,group_query_sql,&stmt,NULL);
    while(!sws_query_next(stmt, NULL)){
        group = read_group_from_stmt(stmt);
        gtarget = find_group_by_account(lc,group->account);
        if(gtarget == NULL)
            gtarget = find_group_by_name_and_mark(lc,group->name,group->markname);
        if(gtarget){
            group_merge(gtarget,group);
         lwqq_group_free(group);
        }
    }
    sws_query_end(stmt,NULL);
}
void lwdb_userdb_query_qqnum(LwdbUserDB* db,LwqqBuddy* buddy)
{
}
void lwdb_userdb_query_qqnumbers(LwqqClient* lc,LwdbUserDB* db)
{
    if(!lc || !db) return;
    LwqqBuddy* buddy;
    LwqqGroup* group;
    char qqnumber[32];
    static SwsStmt* stmt1 = 0,*stmt2,*stmt3,*stmt4;
            
    sws_query_start(db->db, "SELECT qqnumber FROM buddies WHERE nick=? AND markname=?", &stmt1, NULL);
    sws_query_start(db->db, "SELECT qqnumber FROM buddies WHERE nick=?",&stmt2,NULL);
    sws_query_start(db->db, "SELECT account FROM groups WHERE name=? AND markname=?",&stmt3,NULL);
    sws_query_start(db->db, "SELECT account FROM groups WHERE name=?",&stmt4,NULL);

    LIST_FOREACH(buddy,&lc->friends,entries){
        if(buddy->markname){
            sws_query_reset(stmt1);
            sws_query_bind(stmt1, 1, SWS_BIND_TEXT,buddy->nick);
            sws_query_bind(stmt1, 2, SWS_BIND_TEXT,buddy->markname);
            //there are no result.so we ignore it.
            if(sws_query_next(stmt1, NULL)!=SWS_OK){
                lwqq_log(LOG_NOTICE,"userdb mismatch [ nick : %s mark : %s]\n",buddy->nick,buddy->markname);
                continue;
            }
            sws_query_column(stmt1, 0, qqnumber, sizeof(qqnumber), NULL);
            //there are no more result so we can ensure the qqnumber is only.
            if(sws_query_next(stmt1,NULL)==SWS_FAILED){
                buddy->qqnumber = s_strdup(qqnumber);
            }
        }else{
            sws_query_reset(stmt2);
            sws_query_bind(stmt2, 1, SWS_BIND_TEXT,buddy->nick);
            if(sws_query_next(stmt2,0)!=SWS_OK){
                lwqq_log(LOG_NOTICE,"userdb mismatch [ nick : %s ]\n",buddy->nick);
                continue;
            }
            sws_query_column(stmt2, 0, qqnumber, sizeof(qqnumber), NULL);
            if(sws_query_next(stmt2,NULL)==SWS_FAILED){
                buddy->qqnumber = s_strdup(qqnumber);
            }else
                lwqq_log(LOG_NOTICE,"userdb mismatch [ nick : %s ]\n",buddy->nick);
        }
    }
    LIST_FOREACH(group,&lc->groups,entries){
        if(group->markname){
            sws_query_reset(stmt3);
            sws_query_bind(stmt3, 1, SWS_BIND_TEXT,group->name);
            sws_query_bind(stmt3, 2, SWS_BIND_TEXT,group->markname);
            if(sws_query_next(stmt3,0)!=SWS_OK){
                lwqq_log(LOG_NOTICE,"userdb mismatch [ name : %s mark : %s ]\n",group->name,group->markname);
                continue;
            }
            sws_query_column(stmt3, 0, qqnumber,sizeof(qqnumber), NULL);
            if(sws_query_next(stmt3,0)==SWS_FAILED){
                group->account = s_strdup(qqnumber);
            }else
                lwqq_log(LOG_NOTICE,"userdb mismatch [ name : %s mark : %s ]\n",group->name,group->markname);
        }else{
            sws_query_reset(stmt4);
            sws_query_bind(stmt4,1,SWS_BIND_TEXT,group->name);
            if(sws_query_next(stmt4,0)!=SWS_OK){
                lwqq_log(LOG_NOTICE,"userdb mismatch [ name : %s ]\n",group->name);
                continue;
            }
            sws_query_column(stmt4,0,qqnumber,sizeof(qqnumber),NULL);
            if(sws_query_next(stmt4,0)==SWS_FAILED){
                group->account = s_strdup(qqnumber);
            }else
                lwqq_log(LOG_NOTICE,"userdb mismatch [ name : %s ]\n",group->name);
        }
    }

    sws_query_end(stmt1, NULL);
    sws_query_end(stmt2, NULL);
    sws_query_end(stmt3, NULL);
    sws_query_end(stmt4, NULL);
}
void lwdb_userdb_read_from_client(LwqqClient* from,LwdbUserDB* to)
{
    
    if(!from || !to ) return;
    sws_exec_sql(to->db, "BEGIN TRANSACTION;", NULL);
    LwqqBuddy* buddy;
    LIST_FOREACH(buddy,&from->friends,entries){
        if(buddy->qqnumber){
            lwdb_userdb_insert_buddy_info(to, buddy);
        }
    }
    sws_exec_sql(to->db, "COMMIT TRANSACTION;", NULL);
}

void lwdb_userdb_begin(LwdbUserDB* db,const char* transaction)
{
    if(!db) return;
    char sql[128];
    snprintf(sql,sizeof(sql),"BEGIN %s;",transaction?transaction:"");
    sws_exec_sql(db->db,sql,NULL);
}
void lwdb_userdb_commit(LwdbUserDB* db,const char* transaction)
{
    if(!db) return;
    char sql[128];
    snprintf(sql,sizeof(sql),"COMMIT %s;",transaction?transaction:"");
    sws_exec_sql(db->db,sql,NULL);
}
