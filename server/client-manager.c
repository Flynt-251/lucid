#include <ctype.h>
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

#define SQL_AUTH "SELECT pass FROM administrators WHERE email = ?"
#define SQL_SELECT_ALL_QUERY "SELECT * FROM clientlist"
#define SQL_SELECT_EDIT_QUERY "SELECT nickname, port, rdp FROM clientlist WHERE hostname = ?"
#define SQL_DELETE_CLIENT_QUERY "DELETE FROM clientlist WHERE hostname = ?"
#define SQL_UPDATE_CLIENT_QUERY "UPDATE clientlist SET nickname = ?, port = ?, rdp = ? WHERE hostname = ?"
#define SQL_INSERT_CLIENT_QUERY "INSERT INTO clientlist(hostname, nickname) VALUES (?, ?)"
// Sourced from https://stackoverflow.com/a/106223
#define REGEX_IP "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"
#define REGEX_DNS "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$"

static const char *HELP_STRING = "List of commands for Lucid Management Console:\n"
"exit            - Sign out of the management console\n"
"help            - Display this help page\n"
"ls              - List all clients\n"
"rm [hostname]   - Delete the provided hostname\n"
"edit [hostname] - Change client's details\n"
"add             - Add a new client\n";

struct lucidClient {
    const char* hostname;
    const char* nickname;
    int port;
    int rdp;
};

// Modification to ensure all strings are correctly terminated
void strNcpy(char* dest, const char* src, size_t limit) {
    strncpy(dest, src, limit);
    dest[strlen(src)] = '\0';
}

void printLucidClient(const struct lucidClient* client) {
    printf("Lucid Client: %s:%d (RDP %d), Alias %s",
        client->hostname, client->port, client->rdp, client->nickname
    );
}

int verifyUser(const char* username, const char* password) {
    char *sqlAuthCheck = malloc(strlen(SQL_AUTH) + 1);
    strcpy(sqlAuthCheck, SQL_AUTH);
    
    MYSQL *sql = mysql_init(NULL);
    if (sql == NULL) {
        printf("Failed to initialise MySQL connector.\n");
        return -1;
    }
    if (mysql_real_connect(sql, "localhost", "root", "root", "lucid", 1, NULL, 0) == NULL) {
        printf("Failed to connect to MySQL database. (%s)\n", mysql_error(sql));
        mysql_close(sql);
        return -2;
    }
    
    MYSQL_STMT *stmt = mysql_stmt_init(sql);
    if (!stmt) {
        printf("Failed to initialise SQL statement.\n");
        return -3;
    }
    if (mysql_stmt_prepare(stmt, sqlAuthCheck, strlen(sqlAuthCheck))) {
        printf("Failed to Prepare SQL statement.\n");
        printf("%s\n%s\n", sqlAuthCheck, mysql_error(sql));
        return -4;
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
        return -5;
    }
    
    strncpy(sqlUsername, username, 254);
    usernameLength = strlen(sqlUsername);
    
    if (mysql_stmt_execute(stmt)) {
        printf("Failed to bind execute SQL statement.\n");
        return -6;
    }
    
    MYSQL_BIND resultBind[1];
    char passHash[254];
    unsigned long passHashLength;
    memset(resultBind, 0, sizeof(resultBind));
    resultBind[0].buffer_type = MYSQL_TYPE_STRING;
    resultBind[0].buffer = (char *) passHash;
    resultBind[0].buffer_length = 254;
    resultBind[0].is_null = 0;
    resultBind[0].length = &passHashLength;

    if (mysql_stmt_bind_result(stmt, resultBind)) {
        printf("Failed to bind results table.\n\t%s\n", mysql_stmt_error(stmt));
        return -7;
    }
    if (mysql_stmt_store_result(stmt)) {
        printf("Failed to store results table.\n\t%s\n", mysql_stmt_error(stmt));
        return -8;
    }
    if (mysql_stmt_fetch(stmt)) {
        printf("No results for this authentication query.\n");
        return 0;
    }

    int out = verify(passHash, password);
    mysql_close(sql);
    free(sqlAuthCheck);
    return out;
}

int printClientList() {
    MYSQL *con = mysql_init(NULL);
    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to initialise with Error code %i", mysql_errno(con));
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -1;
    }
    if (mysql_real_connect(con, "localhost", "root", "root", "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to connect to Lucid Database with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -2;
    }
    
    if (mysql_query(con, SQL_SELECT_ALL_QUERY)) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL could not carry out SELECT query with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return -3;
    }
    MYSQL_RES *res = mysql_store_result(con);

    
    MYSQL_ROW row;
    printf("Hostname\tNickname\tPort\tRDP \n");
    printf("--------\t--------\t----\t----\n");
    while ( (row = mysql_fetch_row(res)) ) {
        printf("%s\t%s\t%s\t%s\n",
            row[0], row[1], row[2], row[3]
        );
    }

    if (res) {
        mysql_free_result(res);
        res = NULL;
    }

    mysql_close(con);
    return 0;
}

struct lucidClient* getOneClient(const char* hostname) {
    MYSQL *sql = mysql_init(NULL);
    if (sql == NULL) {
        printf("Failed to initialise MySQL connector.\n");
        return NULL;
    }
    if (mysql_real_connect(sql, "localhost", "root", "root", "lucid", 1, NULL, 0) == NULL) {
        printf("Failed to connect to MySQL database. (%s)\n", mysql_error(sql));
        mysql_close(sql);
        return NULL;
    }
    
    MYSQL_STMT *stmt = mysql_stmt_init(sql);
    if (!stmt) {
        printf("Failed to initialise SQL statement.\n");
        return NULL;
    }
    if (mysql_stmt_prepare(stmt, SQL_SELECT_EDIT_QUERY, strlen(SQL_SELECT_EDIT_QUERY))) {
        printf("Failed to Prepare SQL statement.\n");
        printf("%s\n", mysql_error(sql));
        return NULL;
    }
    
    char sqlHostname[253];
    unsigned long hostnameLength;
    MYSQL_BIND bind[1];
    memset(bind, 1, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *) sqlHostname;
    bind[0].buffer_length = 253;
    bind[0].is_null = 0;
    bind[0].length = &hostnameLength;
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        printf("Failed to bind parameter(s) SQL statement.\n");
        return NULL;
    }
    
    strncpy(sqlHostname, hostname, 253);
    hostnameLength = strlen(sqlHostname);
    
    if (mysql_stmt_execute(stmt)) {
        printf("Failed to bind execute SQL statement.\n");
        return NULL;
    }
    
    MYSQL_BIND resultBind[3];
    char nickname[253];
    unsigned long nicknameLength;
    int port;
    unsigned long portLength;
    int rdpPort;
    unsigned long rdpLength;
    memset(resultBind, 0, sizeof(resultBind));

    resultBind[0].buffer_type = MYSQL_TYPE_STRING;
    resultBind[0].buffer = (char *) nickname;
    resultBind[0].buffer_length = 253;
    resultBind[0].is_null = 0;
    resultBind[0].length = &nicknameLength;
    
    resultBind[1].buffer_type = MYSQL_TYPE_SHORT;
    resultBind[1].buffer = (char *) &port;
    resultBind[1].is_null = 0;
    resultBind[1].length = &portLength;
    
    resultBind[2].buffer_type = MYSQL_TYPE_SHORT;
    resultBind[2].buffer = (char *) &rdpPort;
    resultBind[2].is_null = 0;
    resultBind[2].length = &rdpLength;

    if (mysql_stmt_bind_result(stmt, resultBind)) {
        printf("Failed to bind results table.\n\t%s\n", mysql_stmt_error(stmt));
        return NULL;
    }
    if (mysql_stmt_store_result(stmt)) {
        printf("Failed to store results table.\n\t%s\n", mysql_stmt_error(stmt));
        return NULL;
    }
    if (mysql_stmt_fetch(stmt) == MYSQL_NO_DATA) {
        printf("No results for this hostname.\n");
        printf("%s\n", nickname);
        return NULL;
    }

    struct lucidClient* out = &(struct lucidClient) {
        .hostname = hostname,
        .nickname = nickname,
        .port = port,
        .rdp = rdpPort
    };

    mysql_close(sql);
    return out;
}

int modifyClient(const char* hostname, const char* nickname, int port, int rdp) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", "root", "root", // hostname, username, password
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
    if (mysql_stmt_prepare(stmt, SQL_UPDATE_CLIENT_QUERY, strlen(SQL_UPDATE_CLIENT_QUERY))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlNickname[32];
    unsigned long nicknameLen;
    unsigned long portLen;
    unsigned long rdpLen;
    char sqlHostname[253];
    unsigned long hostnameLen;
    MYSQL_BIND bind[4];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlNickname;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &nicknameLen;
    
    bind[1].buffer_type= MYSQL_TYPE_SHORT;
    bind[1].buffer= (char *) &port;
    bind[1].is_null= 0;
    bind[1].length= &portLen;
    
    bind[2].buffer_type= MYSQL_TYPE_SHORT;
    bind[2].buffer= (char *) &rdp;
    bind[2].is_null= 0;
    bind[2].length= &rdpLen;

    bind[3].buffer_type= MYSQL_TYPE_STRING;
    bind[3].buffer= (char *) sqlHostname;
    bind[3].buffer_length = 254;
    bind[3].is_null= 0;
    bind[3].length= &hostnameLen;

    if (mysql_stmt_bind_param(stmt, bind))
    {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    printf("Copying Nickname...\n");
    strncpy(sqlNickname, nickname, 32);
    printf("Setting Nickname length...\n");
    nicknameLen = strlen(sqlNickname);
    printf("Copying Hostname...\n");
    strncpy(sqlHostname, hostname, 253);
    hostnameLen = strlen(sqlHostname);
    printf("All data copied.\n");

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }
    printf("Affected rows: %llu\n", mysql_affected_rows(con));

    mysql_close(con);
    return 0;
}

int newClient(const char* hostname, const char* nickname) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", "root", "root", // hostname, username, password
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
    if (mysql_stmt_prepare(stmt, SQL_INSERT_CLIENT_QUERY, strlen(SQL_INSERT_CLIENT_QUERY))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlHostname[253];
    unsigned long hostnameLen;
    char sqlNickname[253];
    unsigned long nicknameLen;
    MYSQL_BIND bind[2];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlHostname;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &hostnameLen;

    bind[1].buffer_type= MYSQL_TYPE_STRING;
    bind[1].buffer= (char *) sqlNickname;
    bind[1].buffer_length = 254;
    bind[1].is_null= 0;
    bind[1].length= &nicknameLen;

    if (mysql_stmt_bind_param(stmt, bind)) {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strNcpy(sqlHostname, hostname, strlen(hostname));
    hostnameLen = strlen(sqlHostname);
    strNcpy(sqlNickname, nickname, strlen(nickname));
    nicknameLen = strlen(sqlNickname);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }

    mysql_close(con);
    return 0;
}

int deleteClient(const char* hostname) {
    MYSQL *con = mysql_init(NULL);

    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        return -1;
    }

    if (mysql_real_connect(con, "localhost", "root", "root", // hostname, username, password
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
    if (mysql_stmt_prepare(stmt, SQL_DELETE_CLIENT_QUERY, strlen(SQL_DELETE_CLIENT_QUERY))) {
        fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -4;
    }

    char sqlHostname[253];
    unsigned long hostnameLen;
    MYSQL_BIND bind[1];
    memset(bind, 1, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (char *) sqlHostname;
    bind[0].buffer_length = 254;
    bind[0].is_null= 0;
    bind[0].length= &hostnameLen;

    if (mysql_stmt_bind_param(stmt, bind))
    {
        fprintf(stderr, " mysql_stmt_bind_param() failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -5;
    }

    strncpy(sqlHostname, hostname, 253);
    hostnameLen = strlen(sqlHostname);

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
        fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
        return -6;
    }
    printf("Affected rows: %llu\n", mysql_affected_rows(con));

    mysql_close(con);
    return 0;
}

int main() {
    char email[254];
    char passw[254];

    printf("Email> ");
    fgets(email, sizeof(email), stdin);
    email[strcspn(email, "\n")] = 0;
    printf("Password> ");
    fgets(passw, sizeof(passw), stdin);
    passw[strcspn(passw, "\n")] = 0;
    
    int auth = verifyUser(email, passw);
    if (auth < 0) {
        printf("Internal Server Error.\n");
        return auth;
    } else if (auth == 0) {
        printf("Email or password is incorrect, try again.\n");
        return 0;
    }
    
    printf("\nSuccessfully Logged in.\n\n");
    char cur[355];
    char param[255];
    struct lucidClient *client;
    while (0 != strcmp("exit", cur)) {
        printf("lm> ");
        fgets(cur, sizeof(cur), stdin);
        cur[strcspn(cur, "\n")] = 0;

        if (0 == strcmp("\n", cur))
            continue;

        if (0 == strcmp("help", cur)) {
            printf("%s", HELP_STRING);
        } else if (0 == strcmp("ls", cur)) { // Print full list of clients
            printClientList();
        } else if (0 == strncmp("edit ", cur, 5)) { // Edit a single client
            strcpy(param, cur+5);
            client = getOneClient(param);
            if (client == NULL) {
                printf("Invalid Hostname.\n");
                continue;
            }
            printf("Found client for %s (alias \"%s\"), accessible at %d and %d over RDP.\nEditing Client...\n\n",
                param, client->nickname, client->port, client->rdp
            );
            char newNickname[32];
            int newPort = -1;
            int newRdp = -1;
            do {
                printf("Nickname (max. 32 characters): ");
                fgets(cur, sizeof(cur), stdin);
                cur[strcspn(cur, "\n")] = 0;
            } while (strlen(cur) > 32);
            if (0 == strcmp("", cur)) {
                printf("Retaining original nickname.\n");
                strcpy(newNickname, client->hostname);
            } else {
                strcpy(newNickname, cur);
            }
            do {
                printf("Port for Lucid Control [0-35535]: ");
                fgets(cur, sizeof(cur), stdin);
                if (0 == strcmp("\n", cur)) {
                    printf("Retaining original lucid port.\n");
                    newPort = client->port;
                    continue;
                }
                cur[strcspn(cur, "\n")] = 0;
                for (int i=0; i<strlen(cur); i++) {
                    if (!isdigit(cur[i]))
                        continue;
                }
                newPort = atoi(cur);
            } while (newPort > 65535 || newPort < 0);
            do {
                printf("Port for RDP remote access [0-35535]: ");
                fgets(cur, sizeof(cur), stdin);
                if (0 == strcmp("\n", cur)) {
                    printf("Retaining original RDP port.\n");
                    newRdp = client->rdp;
                    continue;
                }
                cur[strcspn(cur, "\n")] = 0;
                for (int i=0; i<strlen(cur); i++) {
                    if (!isdigit(cur[i]))
                        continue;
                }
                newRdp = atoi(cur);
            } while (newRdp > 65535 || newRdp < 0);
            printf("Got: {%s, %d, %d}\nSaving new data to client buffer...\n", newNickname, newPort, newRdp);
            client->nickname = newNickname;
            client->port = newPort;
            client->rdp = newRdp;
            printLucidClient(client);
            printf("Exporting changes...\n");
            if (modifyClient(param, newNickname, newPort, newRdp) == 0) {
                printf("Updated client %s - Alias: %s, Port: %d, RDP Port: %d\n",
                    param, newNickname, newPort, newRdp
                );
            } else {
                printf("Failed to update client.\n");
            }
        } else if (0 == strcmp("add", cur)) { // Add a client
            char hostname[253];
            char nickname[32];
            regex_t hostnameComp;
            regex_t ipComp;
            if (regcomp(&hostnameComp, REGEX_DNS, 0) || regcomp(&ipComp, REGEX_IP, 0)) {
                printf("Failed to initialise at least one regular expression.\n");
                continue;
            }
            do {
                printf("Hostname (must be a valid domain or IPv4 address): ");
                fgets(cur, sizeof(cur), stdin);
                cur[strcspn(cur, "\n")] = 0;
            } while (
                (0 != regexec(&hostnameComp, cur, 0, NULL, 0)) &&
                (0 == regexec(&ipComp, cur, 0, NULL, 0))
            );
            strcpy(hostname, cur);
            do {
                printf("Nickname (max. 32 characters): ");
                fgets(cur, sizeof(cur), stdin);
                cur[strcspn(cur, "\n")] = 0;
            } while (strlen(cur) > 32 || strlen(cur) <= 0);
            strcpy(nickname, cur);
            printf("Attempting to create client %s - Alias: %s, SSH Port: 22, RDP Port: 3389\n",
                hostname, nickname
            );
            if (newClient(hostname, nickname) == 0) {
                printf("Added client %s - Alias: %s\n",
                    hostname, nickname
                );
            } else {
                printf("Failed to create client.\n");
            }
        } else if (0 == strncmp("rm ", cur, 3)) { // Delete a client
            strcpy(param, cur+3);
            client = getOneClient(param);
            if (client == NULL) {
                printf("Invalid Hostname.\n");
                continue;
            }
            printf("Found client to remove: %s (alias \"%s\"), accessible at %d and %d over RDP.\nEditing Client...\n\n",
                param, client->nickname, client->port, client->rdp
            );
            printf("\nThis client will be removed from the lucid system and can no longer be tracked.\n");
            do {
                printf("Is this ok? [y/N] ");
                fgets(cur, sizeof(cur), stdin);
                if (0 == strcmp("\n", cur)) {
                    printf("Operation cancelled.\n");
                    break;
                }
                cur[strcspn(cur, "\n")] = 0;
                if ((0 == strcmp("n", cur)) || (0 == strcmp("N", cur))) {
                    printf("Operation Cancelled.\n");
                    break;
                }
            } while ((0 != strcmp("y", cur)) && (0 != strcmp("Y", cur)));
            if ((0 != strcmp("y", cur)) && (0 != strcmp("Y", cur))) {
                continue;
            }
            if (deleteClient(client->hostname) == 0) {
                printf("Client deleted.\n");
            } else {
                printf("Failed to delete client.\n");
            }
        }
    }
    printf("Goodbye.\n");
}