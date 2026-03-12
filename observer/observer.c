#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <termios.h>
#include <curl/curl.h>

#define USER_AGENT "libcurl-agent/1.0"
#define FREERDP_COMMAND "sdl-freerdp /u:lucid /p:lucid /v:%s /port:%d /size:1920x1080"

struct LucidClient {
    char hostname[255];
    char nickname[33];
    int rdpPort;
    int sshPort;
};

struct Mem {
    char *mem;
    size_t size;
};

size_t writeMem(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t fullSize = size * nmemb;
    struct Mem *mem = (struct Mem *) userp;

    char *ptr = realloc(mem->mem, mem->size + fullSize + 1);
    if (!ptr) {
        fprintf(stderr, "Ran out of memory!\n");
        return 0;
    }

    mem->mem = ptr;
    memcpy(&(mem->mem[mem->size]), contents, fullSize);
    mem->size += fullSize;
    mem->mem[mem->size] = 0;

    return fullSize;
}

void getClientList(char *dest) {
    CURL *curl;
    CURLcode res;
    struct Mem chunk;
    char email[255];
    char passwd[255];
    char basicAuth[512];

    chunk.mem = malloc(1);
    chunk.size=0;

    printf("Email: ");
    fgets(email, sizeof(email), stdin);
    email[strcspn(email, "\n")] = 0;
    printf("Password: ");
    fgets(passwd, sizeof(passwd), stdin);
    passwd[strcspn(passwd, "\n")] = 0;
    sprintf(basicAuth, "%s:%s", email, passwd);
    basicAuth[strlen(basicAuth)] = '\0';

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://localhost:8888/lucid-api");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMem);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_USERPWD, basicAuth);
        // Disable Certificate Checks
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST | CURLSSLOPT_NO_REVOKE);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "Could not perform request: %s\n", curl_easy_strerror(res));
        } else {
            // printf("%lu bytes retrieved.\n", (unsigned long) chunk.size);
            // printf("%s\n", chunk.mem);
            strcpy(dest, chunk.mem);
        }

        curl_easy_cleanup(curl);
        free(chunk.mem);
    }

    curl_global_cleanup();
}

void connectRDP(struct LucidClient client) {
    char command[321];
    sprintf(command, FREERDP_COMMAND, client.hostname, client.rdpPort);
    printf("%s\n", command);
    system(command);
}

int main() {
    char JSON[4096];
    char cur[100];
    struct LucidClient Conns[256];
    int numConns = 0;
    int validSel;
    int selection = 257;

    getClientList(JSON);
    json_object *root = json_tokener_parse(JSON);
    json_object *clients = json_object_object_get(root, "clients");
    int n = json_object_array_length(clients);
    for (int i=0; i<n; i++) {
        json_object *temp = json_object_array_get_idx(clients, i);
        json_object *json_hostname = json_object_object_get(temp, "hostname");
        json_object *json_nickname = json_object_object_get(temp, "nickname");
        json_object *json_port = json_object_object_get(temp, "port");
        json_object *json_rdp = json_object_object_get(temp, "rdp");
        strncpy(Conns[i].hostname, json_object_get_string(json_hostname), 254);
        strncpy(Conns[i].nickname, json_object_get_string(json_nickname), 32);
        Conns[i].sshPort = json_object_get_int(json_port);
        Conns[i].rdpPort = json_object_get_int(json_rdp);
        numConns++;
    }
    for (int i=0; i<numConns; i++) {
        printf("%d: %s (%s) SSH: %d, RDP: %d\n", i, Conns[i].hostname, Conns[i].nickname, Conns[i].sshPort, Conns[i].rdpPort);
    }

    while (selection >= numConns) {
        printf("Enter a client to connect to: ");
        fgets(cur, sizeof(cur), stdin);
        if (0 == strcmp("\n", cur)) continue;
        cur[strcspn(cur, "\n")] = 0;
        validSel = 1;
        for (int i=0; i<strlen(cur); i++)
            if (!isdigit(cur[i])) validSel = 0;
        if (!validSel) continue;
        selection = atoi(cur);
    }

    connectRDP(Conns[selection]);

    return 0;
}