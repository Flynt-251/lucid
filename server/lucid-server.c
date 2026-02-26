#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <json-c/json_object.h>
#include <json-c/json.h>
#include <mysql.h>
#include <time.h>
#include <iso646.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mariadb_com.h"
#include "neon2.h"

#define PORT 8888
#define REALM "Lucid-Remote-Management"
#define JSON_RESOURCE_LOCATION "/lucid-api"
#define JSON_NOT_FOUND_RESPONSE "{\"clients\":[],\"status\":1,\"description\":\"Invalid URL\"}"
#define JSON_INTERNAL_ERROR_RESPONSE "{\"clients\":[],\"status\":2,\"description\":\"Internal Server Error\"}"
#define SQL_AUTH_A "SELECT pass FROM "
#define SQL_AUTH_B " WHERE email = ?"
#define SQL_SELECT_QUERY "SELECT * FROM clientlist"
#define MANAGEMENT_RESOURCE_LOCATION "/lucid-management"
#define MANAGEMENT_INTERNAL_ERROR_RESPONSE "<!DOCTYPE html><head>Lucid - 500</head><body><h1>500 - Internal Server Error</h1></body>"
#define SERVERKEYFILE "server.key"
#define SERVERCERTFILE "server.pem"


const unsigned int AUTH_FLAG_OBSERVER = 0;
const unsigned int AUTH_FLAG_ADMIN = 1;
char *login;

// Admin Login: flynt@twofiveone.xyz, pass: flynt

static size_t get_file_size(const char *filename) {
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp) {
        long size;
        if ((fseek(fp, 0, SEEK_END) != 0) || ((size = ftell(fp)) == -1)) {
            size = 0;
        }
        fclose(fp);
        return (size_t) size;
    }
    return 0;
}

static char* load_file(const char *filename) {
    FILE *fp;
    char *buffer;
    size_t size;
    size = get_file_size(filename);

    if (size == 0)
        return NULL;
    fp = fopen(filename, "rb");
    if (!fp)
        return NULL;

    buffer = malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    buffer[size] = '\0';
    if (fread(buffer, 1, size,fp) != size) {
        free(buffer);
        return NULL;
    }
    fclose(fp);
    return buffer;
}

char* dynamicStrcat(char* str1, char* str2) {
    int len1 = strlen(str1);
    int len2 = strlen(str2);
    printf("Attempting to concatenate \"%s\" (%d) and \"%s\" (%d), will be %d bytes.\n", str1, len1, str2, len2, len1+len2+1);
    char* out = (char *) malloc(len1 + len2 + 1);
    strcpy(stpcpy(out, str1), str2);
    out[len1+len2] = '\0';
    printf("Result: %s\n", out);
    return out;
}

const char* getClientList() {
    json_object *root = json_object_new_object();
    json_object_object_add(root, "timestamp", json_object_new_int64((unsigned long) time(NULL)));
    json_object *clientList = json_object_new_array();

    MYSQL *con = mysql_init(NULL);
    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        json_object_object_add(root, "status", json_object_new_int(-1));
        json_object_object_add(root, "description", json_object_new_string("Failed to fetch client list."));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to initialise with Error code %i", mysql_errno(con));
        json_object_object_add(root, "debug", json_object_new_string(errMsg));
        return json_object_to_json_string_ext(root, 0);
    }
    if (mysql_real_connect(con, "localhost", "root", "root", "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        json_object_object_add(root, "status", json_object_new_int(-2));
        json_object_object_add(root, "description", json_object_new_string("Failed to fetch client list."));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to connect to Lucid Database with Error code %i", mysql_errno(con));
        json_object_object_add(root, "debug", json_object_new_string(errMsg));
        mysql_close(con);
        return json_object_to_json_string_ext(root, 0);
    }
    
    if (mysql_query(con, SQL_SELECT_QUERY)) {
        fprintf(stderr, "%s\n", mysql_error(con));
        json_object_object_add(root, "status", json_object_new_int(-2));
        json_object_object_add(root, "description", json_object_new_string("Failed to fetch client list."));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL could not carry out SELECT query with Error code %i", mysql_errno(con));
        json_object_object_add(root, "debug", json_object_new_string(errMsg));
        mysql_close(con);
        return json_object_to_json_string_ext(root, 0);
    }
    MYSQL_RES *res = mysql_store_result(con);

    
    MYSQL_ROW row;
    while ( (row = mysql_fetch_row(res)) ) {
        json_object *client = json_object_new_object();
        json_object_object_add(client, "hostname", json_object_new_string(row[0]));
        json_object_object_add(client, "nickname", json_object_new_string(row[1]));
        json_object_object_add(client, "port", json_object_new_int(atoi(row[2])));
        json_object_object_add(client, "rdp", json_object_new_int(atoi(row[3])));
        json_object_array_add(clientList, client);
    }
    
    if (res) {
        mysql_free_result(res);
        res = NULL;
    }

    json_object_object_add(root, "clients", clientList);
    json_object_object_add(root, "status", json_object_new_int(0));
    json_object_object_add(root, "description", json_object_new_string("Client List generated normally."));
    json_object_object_add(root, "debug", json_object_new_string("No debug information."));

    mysql_close(con);
    return json_object_to_json_string_ext(root, 0);
}

const char* getClientTable() {
    printf("Initialising output variable for getClientTable...\n");
    char *out = malloc(100 * sizeof(char));
    memset(out, '\0', 100 * sizeof(char));

    MYSQL *con = mysql_init(NULL);
    if (con == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to initialise with Error code %i", mysql_errno(con));
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return dynamicStrcat(out, errMsg);
    }
    if (mysql_real_connect(con, "localhost", "root", "root", "lucid", 1, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL Failed to connect to Lucid Database with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return dynamicStrcat(out, errMsg);
    }
    
    if (mysql_query(con, SQL_SELECT_QUERY)) {
        fprintf(stderr, "%s\n", mysql_error(con));
        char errMsg[255];
        snprintf(errMsg, sizeof(errMsg), "MySQL could not carry out SELECT query with Error code %i", mysql_errno(con));
        mysql_close(con);
        sprintf(errMsg, "Internal Server Error: %s", errMsg);
        return dynamicStrcat(out, errMsg);
    }
    MYSQL_RES *res = mysql_store_result(con);

    
    MYSQL_ROW row;
    while ( (row = mysql_fetch_row(res)) ) {
        char *outputRow = malloc(200 * sizeof(char));
        sprintf(outputRow,
            "<tr>\n<td>%s</td>\n<td>%s</td>\n<td>%s</td>\n<td>%s</td>\n</tr>\n",
            row[0], row[1], row[2], row[3]
        );
        out = dynamicStrcat(out, outputRow);
        printf("Added a row to the management document.\n");
        memset(outputRow, '\0', 200 * sizeof(char));
    }
    printf("Got all rows!");

    if (res) {
        mysql_free_result(res);
        res = NULL;
    }

    mysql_close(con);
    return out;
}

int verifyUser(struct MHD_Connection *conn, unsigned int flag) {
    struct MHD_BasicAuthInfo *auth_info;
    auth_info = MHD_basic_auth_get_username_password3(conn);
    if (auth_info == NULL)
        return 0;

    char *table;
    if (flag == AUTH_FLAG_OBSERVER)
        table = "observers";
    else if (flag == AUTH_FLAG_ADMIN)
        table = "administrators";
    else
        return -10;

    char *sqlAuthCheck = malloc(strlen(SQL_AUTH_A) + strlen(SQL_AUTH_B) + strlen(table) + 1);
    strcpy(sqlAuthCheck, SQL_AUTH_A);
    strcat(sqlAuthCheck, table);
    strcat(sqlAuthCheck, SQL_AUTH_B);

    char *username = auth_info->username;
    char *password = auth_info->password;
    // printf("Attempting to copy login data...\n");
    // strcpy(username, auth_info->username);
    // strcpy(password, auth_info->password);
    
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

static enum MHD_Result handleSimpleGet(struct MHD_Connection *conn, const char *filename) {
    char *file = load_file(filename);
    struct MHD_Response *response = MHD_create_response_from_buffer_static(strlen(file), file);
    enum MHD_Result ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

static enum MHD_Result getClientManagementPanel(struct MHD_Connection *conn) {
    const char* htmlResp1 = load_file("management-index.html.part1");
    const char* htmlResp2 = load_file("management-index.html.part2");
    const char* table = getClientTable();
    char* htmlResp = malloc(strlen(htmlResp1) + strlen(htmlResp2) + strlen(table) + 1);
    printf("%s", table);
    sprintf(htmlResp, "%s%s%s", htmlResp1, table, htmlResp2);
    printf("%s", htmlResp);
    struct MHD_Response *response = MHD_create_response_from_buffer_static(strlen(htmlResp), htmlResp);
    enum MHD_Result ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

static enum MHD_Result respondToRequest(void *cls, struct MHD_Connection *conn,
 const char *url, const char *method,
 const char *version, const char *upload_data,
 size_t *upload_data_size, void **reqCls) {
    if (strcmp(method, "GET") != 0) // Only accept GET requests
        return MHD_NO;
    if (*reqCls == NULL) { // If this is not a callback, tell client to proceed
        *reqCls = conn;
        return MHD_YES;
    }
    struct MHD_Response *response;
    enum MHD_Result ret;

    if ((strcmp(JSON_RESOURCE_LOCATION, url) != 0)
     && (strcmp(MANAGEMENT_RESOURCE_LOCATION, url) != 0)) { // 404 if incorrect resource is requested.
        response = MHD_create_response_from_buffer_static(strlen(JSON_NOT_FOUND_RESPONSE), JSON_NOT_FOUND_RESPONSE);
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }

    if (strcmp(JSON_RESOURCE_LOCATION, url) == 0) {
        int auth = verifyUser(conn, AUTH_FLAG_OBSERVER);
        if (auth < 0) {
            response = MHD_create_response_from_buffer_static(strlen(JSON_INTERNAL_ERROR_RESPONSE), JSON_INTERNAL_ERROR_RESPONSE);
            MHD_add_response_header(response, "Content-Type", "application/json");
            ret = MHD_queue_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            return ret;
        }
        if (auth == 0) {
            response = MHD_create_response_empty(MHD_RF_NONE);
            if (!response)
                return MHD_NO;
            ret = MHD_queue_basic_auth_required_response3(conn, REALM, MHD_YES, response);
            MHD_destroy_response(response);
            return ret;
        }
    
        const char *clientList = getClientList();
        response = MHD_create_response_from_buffer_static(strlen(clientList), clientList);
        if (!response)
            return MHD_NO;
        MHD_add_response_header(response, "Content-Type", "application/json");
        ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    int auth = verifyUser(conn, AUTH_FLAG_ADMIN);
    if (auth < 0) {
        response = MHD_create_response_from_buffer_static(strlen(MANAGEMENT_INTERNAL_ERROR_RESPONSE), MANAGEMENT_INTERNAL_ERROR_RESPONSE);
        ret = MHD_queue_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }
    if (auth == 0) {
        response = MHD_create_response_empty(MHD_RF_NONE);
        if (!response)
            return MHD_NO;
        ret = MHD_queue_basic_auth_required_response3(conn, REALM, MHD_YES, response);
        MHD_destroy_response(response);
        return ret;
    }

    return getClientManagementPanel(conn);
}

int main() {
    struct MHD_Daemon *daemon;
    char *keyPem;
    char *certPem;
    keyPem = load_file(SERVERKEYFILE);
    certPem = load_file(SERVERCERTFILE);

    if ((keyPem == NULL) || (certPem == NULL)) {
        printf("The key or certificate file could not be read.\n");
        if (keyPem != NULL) free(keyPem);
        if (certPem != NULL) free(certPem);
        return 1;
    }

    int flags = MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_TLS;
    daemon = MHD_start_daemon(flags, PORT, NULL,
     NULL, &respondToRequest, NULL,
     MHD_OPTION_HTTPS_MEM_KEY, keyPem,
     MHD_OPTION_HTTPS_MEM_CERT, certPem
    );
    if (daemon == NULL) {
        printf("%s\n", certPem);
        free(keyPem);
        free(certPem);
        return 0;
    }
    printf("HTTPS Server started, press Enter to terminate.\n");
    (void) getchar();
    MHD_stop_daemon(daemon);
    free(keyPem);
    free(certPem);
    return 0;
}