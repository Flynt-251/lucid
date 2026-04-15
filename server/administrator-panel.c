#include <sys/types.h>
#include <sys/select.h>
#include <mysql.h>
#include <iso646.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "mariadb_com.h"
#include "neon2.h"

#define MSG_ROOT_NOTICE "NOTE: You are logged in with elevated privileges.\n"

#define SQL_USER "lucid"
#define SQL_PASS "hyperplane-kumquat-platypus"
#define SQL_AUTH "SELECT pass, root FROM administrators WHERE email = ?"
#define SQL_SELECT_ALL_OBS_QUERY "SELECT email FROM observers"
#define SQL_SELECT_ALL_ADMIN_QUERY "SELECT email, root FROM administrators"
#define SQL_INSERT_OBS_QUERY "INSERT INTO observers(email, pass) VALUES (?, ?)"
#define SQL_INSERT_ADMIN_QUERY "INSERT INTO administrators(email, pass) VALUES (?, ?)"
#define SQL_DELETE_OBS_QUERY "DELETE FROM observers WHERE email = ?"
#define SQL_DELETE_ADMIN_QUERY "DELETE FROM administrators WHERE email = ?"
#define SQL_PASSWD_RESET_OBS "UPDATE observers SET pass = ? WHERE email = ?"
#define SQL_ROOT_ENABLE "UPDATE administrators SET root = TRUE WHERE email = ?"
#define SQL_ROOT_DISABLE "UPDATE administrators SET root = FALSE WHERE email = ?"
#define SQL_PASSWD_CHANGE "UPDATE administrators SET pass = ? WHERE email= ?"

static const char *HELP_STRING_FULL = "List of commands for Lucid Administration Panel:\n"
"exit             - Sign out of the administration panel\n"
"help             - Display this help page\n"
"ls obs           - List all observer accounts\n"
"ls admin         - List all administrator accounts\n"
"newobs [email]   - Add a new observer account with the provided email\n"
"newadmin [email] - Add a new administrator account with the provided email\n"
"rm obs [email]   - Delete the provided observer account\n"
"rm admin [email] - Delete the provided administrator account\n"
"reset [email]    - Initiate password reset for the provided observer account\n"
"mkroot [email]   - Elevate the given administrator account to root privileges\n"
"rmroot [email]   - Remove root privileges from the given administrator account\n"
"passwd           - Change your account's password\n";

static const char *HELP_STRING_LIMITED = "List of commands for Lucid Administration Panel:\n"
"exit              - Sign out of the administration panel\n"
"help              - Display this help page\n"
"ls obs            - List all observer accounts\n"
"newobs [email]    - Add a new observer account with the provided email\n"
"rm obs [email]    - Delete the provided observer account\n"
"reset obs [email] - Initiate password reset for the provided observer account\n"
"passwd            - Change this administrator account's password\n";

// Modification to ensure all strings are correctly terminated
void strNcpy(char* dest, const char* src, size_t limit) {
    strncpy(dest, src, limit);
    dest[strlen(src)] = '\0';
}

int verifyUser(const char* username, const char* password) {
    char *sqlAuthCheck = malloc(strlen(SQL_AUTH) + 1);
    strcpy(sqlAuthCheck, SQL_AUTH);
    
    MYSQL *sql = mysql_init(NULL);
    if (sql == NULL) {
        printf("Failed to initialise MySQL connector.\n");
        return -3;
    }
    if (mysql_real_connect(sql, "localhost", SQL_USER, SQL_PASS, "lucid", 1, NULL, 0) == NULL) {
        printf("Failed to connect to MySQL database. (%s)\n", mysql_error(sql));
        mysql_close(sql);
        return -4;
    }
    
    MYSQL_STMT *stmt = mysql_stmt_init(sql);
    if (!stmt) {
        printf("Failed to initialise SQL statement.\n");
        return -5;
    }
    if (mysql_stmt_prepare(stmt, sqlAuthCheck, strlen(sqlAuthCheck))) {
        printf("Failed to Prepare SQL statement.\n");
        printf("%s\n%s\n", sqlAuthCheck, mysql_error(sql));
        return -6;
    }
    
    char sqlUsername[255];
    unsigned long usernameLength;
    MYSQL_BIND bind[1];
    memset(bind, 1, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *) username;
    bind[0].buffer_length = 254;
    bind[0].is_null = 0;
    bind[0].length = &usernameLength;
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        printf("Failed to bind parameter(s) SQL statement.\n");
        return -7;
    }
    
    strNcpy(sqlUsername, username, 254);
    usernameLength = strlen(sqlUsername);
    
    if (mysql_stmt_execute(stmt)) {
        printf("Failed to bind execute SQL statement.\n");
        return -8;
    }
    
    MYSQL_BIND resultBind[2];
    char passHash[254];
    unsigned long passHashLength;
    int rootFlag = 0;
    unsigned long rootFlagLength;
    memset(resultBind, 0, sizeof(resultBind));
    resultBind[0].buffer_type = MYSQL_TYPE_STRING;
    resultBind[0].buffer = (char *) passHash;
    resultBind[0].buffer_length = 254;
    resultBind[0].is_null = 0;
    resultBind[0].length = &passHashLength;
    resultBind[1].buffer_type = MYSQL_TYPE_SHORT;
    resultBind[1].buffer = (char *) &rootFlag;
    resultBind[1].is_null = 0;
    resultBind[1].length = &rootFlagLength;

    if (mysql_stmt_bind_result(stmt, resultBind)) {
        printf("Failed to bind results table.\n\t%s\n", mysql_stmt_error(stmt));
        return -9;
    }
    if (mysql_stmt_store_result(stmt)) {
        printf("Failed to store results table.\n\t%s\n", mysql_stmt_error(stmt));
        return -10;
    }
    if (mysql_stmt_fetch(stmt)) {
        printf("No results for this authentication query.\n");
        return -2;
    }

    int out = verify(passHash, password);
    mysql_close(sql);
    free(sqlAuthCheck);

    if (out == 0) return -1;

    return (short int) rootFlag;
}

// Sourced from https://stackoverflow.com/a/15768317
void randomStr(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

int printObserverList() {
    MYSQL *con = mysql_init(NULL);
    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to initialise with Error code %i", mysql_errno(con));
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -1;
    }
    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to connect to Lucid Database with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -2;
    }
    
    if (mysql_query(con, SQL_SELECT_ALL_OBS_QUERY)) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL could not carry out SELECT query with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -3;
    }
    MYSQL_RES *res = mysql_store_result(con);

    
    MYSQL_ROW row;
    while ( (row = mysql_fetch_row(res)) ) {
        printf("%s\n", row[0]);
    }

    if (res) {
        mysql_free_result(res);
        res = NULL;
    }

    mysql_close(con);
    return 0;
}

int printAdminList() {
    MYSQL *con = mysql_init(NULL);
    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to initialise with Error code %i", mysql_errno(con));
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -1;
    }
    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to connect to Lucid Database with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -2;
    }
    
    if (mysql_query(con, SQL_SELECT_ALL_ADMIN_QUERY)) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL could not carry out SELECT query with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -3;
    }
    MYSQL_RES *res = mysql_store_result(con);

    
    MYSQL_ROW row;
    while ( (row = mysql_fetch_row(res)) ) {
        if (atoi(row[1]) == 1) printf("[*] ");
        printf("%s\n", row[0]);
    }

    if (res) {
        mysql_free_result(res);
        res = NULL;
    }

    mysql_close(con);
    return 0;
}

int newObserver(const char* email) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_INSERT_OBS_QUERY, strlen(SQL_INSERT_OBS_QUERY))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlEmail[253];
    unsigned long emailLen;
    char passHash[253];
    unsigned long passHashLength;
    MYSQL_BIND bind[2];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlEmail;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &emailLen;

    bind[1].buffer_type= MYSQL_TYPE_STRING;
    bind[1].buffer= (char *) passHash;
    bind[1].buffer_length = 254;
    bind[1].is_null= 0;
    bind[1].length= &passHashLength;

    if (mysql_stmt_bind_param(stmt, bind)) {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, strlen(email));

    emailLen = strlen(sqlEmail);
    printf("%lu %lu %d\n", strlen(email), strlen(sqlEmail), (int)(strlen(sqlEmail) - strlen(email)));
    char passwd[17];
    randomStr(passwd, 16);
    printf("The password for this account is %s. This will not be shown again.\n", passwd);
    strcpy(passHash, hash(passwd));
    passHashLength = strlen(passHash);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }

    mysql_close(con);
    return 0;
}

int newAdministrator(const char* email) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_INSERT_ADMIN_QUERY, strlen(SQL_INSERT_ADMIN_QUERY))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlEmail[253];
    unsigned long emailLen;
    char passHash[253];
    unsigned long passHashLength;
    MYSQL_BIND bind[2];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlEmail;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &emailLen;

    bind[1].buffer_type= MYSQL_TYPE_STRING;
    bind[1].buffer= (char *) passHash;
    bind[1].buffer_length = 254;
    bind[1].is_null= 0;
    bind[1].length= &passHashLength;

    if (mysql_stmt_bind_param(stmt, bind)) {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, strlen(email));
    emailLen = strlen(sqlEmail);
    char passwd[17];
    randomStr(passwd, 16);
    printf("The password for this account is %s. This will not be shown again.\n", passwd);
    strcpy(passHash, hash(passwd));
    passHashLength = strlen(passHash);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }

    mysql_close(con);
    return 0;
}

int deleteObserver(const char* email) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_DELETE_OBS_QUERY, strlen(SQL_DELETE_OBS_QUERY))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlEmail[253];
    unsigned long emailLen;
    MYSQL_BIND bind[1];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlEmail;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &emailLen;

    if (mysql_stmt_bind_param(stmt, bind))
    {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, 253);
    emailLen = strlen(sqlEmail);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }
    printf("Affected rows: %llu\n", mysql_affected_rows(con));

    mysql_close(con);
    return 0;
}

int deleteAdministrator(const char* email) {
    if (0 == strcmp("root@lucid.lucid", email)) {
        printf("Cannot delete root account.\n");
        return -10;
    }

    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_DELETE_ADMIN_QUERY, strlen(SQL_DELETE_ADMIN_QUERY))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlEmail[253];
    unsigned long emailLen;
    MYSQL_BIND bind[1];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlEmail;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &emailLen;

    if (mysql_stmt_bind_param(stmt, bind))
    {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, 253);
    emailLen = strlen(sqlEmail);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }
    printf("Affected rows: %llu\n", mysql_affected_rows(con));

    mysql_close(con);
    return 0;
}

int resetObserverPassword(const char* email) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_PASSWD_RESET_OBS, strlen(SQL_PASSWD_RESET_OBS))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char passHash[253];
    unsigned long passHashLength;
    char sqlEmail[253];
    unsigned long emailLen;
    MYSQL_BIND bind[2];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) passHash;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &passHashLength;

    bind[1].buffer_type= MYSQL_TYPE_STRING;
    bind[1].buffer= (char *) sqlEmail;
    bind[1].buffer_length = 254;
    bind[1].is_null= 0;
    bind[1].length= &emailLen;

    if (mysql_stmt_bind_param(stmt, bind)) {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, strlen(email));
    emailLen = strlen(sqlEmail);
    char passwd[17];
    randomStr(passwd, 16);
    printf("The new password for this account is %s. This will not be shown again.\n", passwd);
    strcpy(passHash, hash(passwd));
    passHashLength = strlen(passHash);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }

    mysql_close(con);
    return 0;
}

int changeAdminPassword(const char* email, char* passwd) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_PASSWD_CHANGE, strlen(SQL_PASSWD_CHANGE))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char passHash[253];
    unsigned long passHashLength;
    char sqlEmail[253];
    unsigned long emailLen;
    MYSQL_BIND bind[2];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) passHash;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &passHashLength;

    bind[1].buffer_type= MYSQL_TYPE_STRING;
    bind[1].buffer= (char *) sqlEmail;
    bind[1].buffer_length = 254;
    bind[1].is_null= 0;
    bind[1].length= &emailLen;

    if (mysql_stmt_bind_param(stmt, bind)) {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, strlen(email));
    emailLen = strlen(sqlEmail);
    strcpy(passHash, hash(passwd));
    passHashLength = strlen(passHash);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }

    mysql_close(con);
    return 0;
}

int enableRootPriv(const char* email) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_ROOT_ENABLE, strlen(SQL_ROOT_ENABLE))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlEmail[253];
    unsigned long emailLen;
    MYSQL_BIND bind[1];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlEmail;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &emailLen;

    if (mysql_stmt_bind_param(stmt, bind))
    {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, 253);
    emailLen = strlen(sqlEmail);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }

    int delta = mysql_affected_rows(con);
    if (delta == 0)
        printf("Root privileges are already enabled for this account.\n");
    else
        printf("Root privileges have been enabled.\n");

    mysql_close(con);
    return 0;
}

int disableRootPriv(const char* email) {
    if (0 == strcmp("root@lucid.lucid", email)) {
        printf("Root privileges cannot be disabled for the root account.\n");
        return -10;
    }

    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", SQL_USER, SQL_PASS, // hostname, username, password
            "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return -2;
    }  

    MYSQL_STMT *stmt = mysql_stmt_init(con);
    if (!stmt) {
        fprintf(stderr, " mysql_stmt_init(), out of memory\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, SQL_ROOT_DISABLE, strlen(SQL_ROOT_DISABLE))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlEmail[253];
    unsigned long emailLen;
    MYSQL_BIND bind[1];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlEmail;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &emailLen;

    if (mysql_stmt_bind_param(stmt, bind))
    {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlEmail, email, 253);
    emailLen = strlen(sqlEmail);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }

    int delta = mysql_affected_rows(con);
    if (delta == 0)
        printf("Root privileges are already disabled for this account.\n");
    else
        printf("Root privileges have been diabled.\n");

    mysql_close(con);
    return 0;
}

int main() {
    char email[254] = "";
    char passw[254] = "";
    int auth = -1;

    printf("Email> ");
    fgets(email, sizeof(email), stdin);
    email[strcspn(email, "\n")] = 0;
    printf("Password> ");
    fgets(passw, sizeof(passw), stdin);
    passw[strcspn(passw, "\n")] = 0;
    
    auth = verifyUser(email, passw);
    if (auth < -2) {
        printf("Internal Server Error.\n");
        return auth;
    } else if (auth < 0) {
        printf("Email or password is incorrect, try again.\n");
        return 0;
    }
    
    printf("\nSuccessfully Logged in.\n");
    if (auth == 1) printf(MSG_ROOT_NOTICE);
    printf("\n");


    char cur[355] = "";
    char param[255] = "";
    while (0 != strcmp("exit", cur)) {
        auth == 1 ? printf("lucid-panel*> ") : printf("lucid-panel> ");
        fgets(cur, sizeof(cur), stdin);
        cur[strcspn(cur, "\n")] = 0;

        if (0 == strcmp("\n", cur))
            continue;

        if (0 == strcmp("help", cur)) {
            auth == 1 ? printf("%s", HELP_STRING_FULL) : printf("%s", HELP_STRING_LIMITED);
        } else if (0 == strcmp("ls obs", cur)) { // Print full list of observers
            printObserverList();
        } else if (0 == strcmp("ls admin", cur)) { // Print full list of administrators (root only)
            if (auth == 1) {
                printAdminList();
            } else {
                printf("Permission Denied.\n");
            }
        } else if (0 == strncmp("newobs ", cur, 7)) { // Create a new observer with the given email
            strcpy(param, cur+7);
            if (strlen(param) == 0) continue;
            if (newObserver(param) == 0)
                printf("New observer account created for %s\n", param);
            else
                printf("Failed to create observer account\n");
        } else if (0 == strncmp("newadmin ", cur, 9)) { // Create a new admin with the given email (root only)
            if (auth == 0) {
                printf("Permission Denied.\n");
                continue;
            }
            strcpy(param, cur+9);
            if (strlen(param) == 0) continue;
            if (newAdministrator(param) == 0)
                printf("New administrator account created for %s\n", param);
            else
                printf("Failed to create administrator account\n");
        } else if (0 == strncmp("rm obs ", cur, 7)) { // Remove an observer
            strcpy(param, cur+7);
            if (deleteObserver(param) == 0)
                printf("Observer account deleted.\n");
            else
                printf("Failed to delete observer account.\n");
        } else if (0 == strncmp("rm admin ", cur, 9)) { // Remove an admin (root only)
            if (auth == 0) {
                printf("Permission Denied.\n");
                continue;
            }
            strcpy(param, cur+9);
            if (deleteAdministrator(param) == 0)
                printf("Administrator account deleted.\n");
            else
                printf("Failed to delete administrator account.\n");
        } else if (0 == strncmp("reset ", cur, 6)) { // Reset an observer's password
            strcpy(param, cur+6);
            if (resetObserverPassword(param) == 0)
                printf("Password reset successful\n");
            else
                printf("Failed to reset password\n");
        } else if (0 == strncmp("mkroot ", cur, 7)) { // Give an administrator root privileges (root only)
            if (auth == 0) {
                printf("Permission Denied.\n");
                continue;
            }
            strcpy(param, cur+7);
            if (enableRootPriv(param) == 0)
                printf("Root privileges enabled.\n");
            else
                printf("Could not enable root privileges for this account.\n");
        } else if (0 == strncmp("rmroot ", cur, 7)) { // Revoke an administrator's root privileges (root only)
            if (auth == 0) {
                printf("Permission Denied.\n");
                continue;
            }
            strcpy(param, cur+7);
            if (disableRootPriv(param) == 0)
                printf("Root privileges disabled.\n");
            else
                printf("Could not disable root privileges for this account.\n");
        } else if (0 == strcmp("passwd", cur)) { // Change own password
            printf("Enter new password: ");
            fgets(cur, sizeof(cur), stdin);
            cur[strcspn(cur, "\n")] = 0;
            strcpy(param, cur);
            printf("Enter password again: ");
            fgets(cur, sizeof(cur), stdin);
            cur[strcspn(cur, "\n")] = 0;
            if (0 != strcmp(param, cur)) {
                printf("Passwords do not match, please try again.\n");
                continue;
            }
            if (changeAdminPassword(email, param) == 0)
                printf("Password updated successfully.\n");
            else
                printf("Unable to change password.\n");
        }
    }
    printf("Goodbye.\n");
}