#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <json-c/json.h>
#include <json-c/json_object.h>
#include <termios.h>
#include <curl/curl.h>
#include <libssh/libssh.h>

#define HOSTNAME "/etc/lucid-observer/server"
#define USER_AGENT "libcurl-agent/1.0"
#define MACRO_SHUTDOWN "sudo shutdown now"
#define MACRO_RESTART "sudo shutdown -r now"
#define MACRO_LOGOUT "qdbus org.kde.Shutdown /Shutdown logout"

// Global flag to ensure client list was properly fetched
int listFetched = 0;

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

// ---------------------------- CURL MEMORY MANAGEMENT ----------------------------

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

// ---------------------------- FILE HANDLING ----------------------------

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

// ---------------------------- SSH COMMANDS ----------------------------

int verifyKnownHost(ssh_session ssh) {
    enum ssh_known_hosts_e state;
    unsigned char *hash = NULL;
    ssh_key serverPublicKey = NULL;
    size_t hlen;
    char buf[10];
    char *p = NULL;
    int cmp;
    int rc;

    rc = ssh_get_server_publickey(ssh, &serverPublicKey);
    if (rc < 0)
        return -1;

    rc = ssh_get_publickey_hash(serverPublicKey, SSH_PUBLICKEY_HASH_SHA256, &hash, &hlen);
    ssh_key_free(serverPublicKey);
    if (rc < 0)
        return -1;
    
    state = ssh_session_is_known_server(ssh);
    switch (state) {
        // Have connected to this client before
        case SSH_KNOWN_HOSTS_OK:
            break;
            
        // Hostname is familiar, but its key has changed (could indicate compromisation)
        case SSH_KNOWN_HOSTS_CHANGED:
            fprintf(stderr, "Host key has changed to:\n");
            ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
            fprintf(stderr, "Connection stopped.\n");
            ssh_clean_pubkey_hash(&hash);
            return -1;
        
        // No .ssh/known_hosts file, will create automatically
        case SSH_KNOWN_HOSTS_NOT_FOUND:
            fprintf(stderr, "Couldn't find a known host file.\nContinue to automatically create it.\n");

        // New client. Prompt user to accept it.
        case SSH_KNOWN_HOSTS_UNKNOWN:
            fprintf(stderr, "This is an unknown server. Do you wish to accept the host key?");
            ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
            ssh_clean_pubkey_hash(&hash);
            p = fgets(buf, sizeof(buf), stdin);
            if (p == NULL) {
                return -1;
            }
            cmp = strncasecmp(buf, "yes", 3);
            if (cmp != 0) {
                return -1;
            }
            break;
    }
    ssh_clean_pubkey_hash(&hash);
    return 0;
}

int sshPerformCommand(ssh_session session, const char *command) {
    ssh_channel channel = NULL;
    int rc;
    char buffer[256];
    int nbytes;
    
    // Create and configure SSH channel
    channel = ssh_channel_new(session);
    if (channel == NULL)
        return SSH_ERROR;
    
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        return rc;
    }
    
    rc = ssh_channel_request_exec(channel, command);
    if (rc != SSH_OK) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return rc;
    }
    
    // Fetch output of the given SSH command. This may be useful for getting the result of a command.
    nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    while (nbytes > 0) {
        if (write(1, buffer, nbytes) != (unsigned int) nbytes) {
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return SSH_ERROR;
        }
        nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    }
    buffer[nbytes] = '\0';
    printf("%s\n", buffer);
    
    if (nbytes < 0) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return SSH_ERROR;
    }
    
    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    
    return SSH_OK;
}

// Start SSH Session, then pass over to sshPerformCommand to execute provided instruction
void connectSSH(struct LucidClient client, const char *command) {
    ssh_session ssh = NULL;
    int rc;
    
    ssh = ssh_new();
    if (ssh == NULL)
        exit(-1);

    ssh_options_set(ssh, SSH_OPTIONS_HOST, client.hostname);
    ssh_options_set(ssh, SSH_OPTIONS_PORT, &client.sshPort);

    rc = ssh_connect(ssh);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error connecting to SSH server: %s\n", ssh_get_error(ssh));
        return;
    }

    // Determine client authenticity
    if (verifyKnownHost(ssh) < 0) {
        ssh_disconnect(ssh);
        ssh_free(ssh);
        return;
    }

    // Open the SSH channel...
    rc = ssh_userauth_password(ssh, "lucid", "lucid");
    if (rc != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Authentication Failure: %s\n", ssh_get_error(ssh));
        ssh_disconnect(ssh);
        ssh_free(ssh);
        return;
    }

    // And pass this connection to the command handler.
    sshPerformCommand(ssh, command);

    // Then clean everything up.
    ssh_disconnect(ssh);
    ssh_free(ssh);
}

// ---------------------------- RDP CONNECTION ----------------------------

void connectRDP(struct LucidClient client) {
    char host[259];
    char port[12];
    
    sprintf(host, "/v:%s", client.hostname);
    sprintf(port, "/port:%d", client.rdpPort);
    execlp("/usr/bin/sdl-freerdp", "sdl-freerdp", "/u:lucid", "/p:lucid", host, port, "/size:1920x1080", NULL);
    perror("Could not start sdl-freerdp.\n");
    return;
}

// ---------------------------- CONNECTION TO SERVER ----------------------------

// Perform CURL HTTPS request to get JSON client list
void getClientList(char *dest) {
    CURL *curl;
    CURLcode res;
    struct Mem chunk;
    char email[255];
    char passwd[255];
    char basicAuth[512];
    char* server;

    chunk.mem = malloc(1);
    chunk.size=0;

    server = load_file(HOSTNAME);
    if (server == NULL) {
        printf("Cannot read server hostname file.\n");
        return;
    }
    server[strcspn(server, "\n")] = 0;

    // Get HTTP Basic Authentication details
    printf("Email: ");
    fgets(email, sizeof(email), stdin);
    email[strcspn(email, "\n")] = 0;
    printf("Password: ");
    fgets(passwd, sizeof(passwd), stdin);
    passwd[strcspn(passwd, "\n")] = 0;
    sprintf(basicAuth, "%s:%s", email, passwd);
    basicAuth[strlen(basicAuth)] = '\0';

    // Set up HTTPS request
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, server);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMem);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_USERPWD, basicAuth);

        // Disable Certificate Checks to permit self-signed certificates
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // !!! THIS IS NOT RECOMMENDED AND IS INSECURE! !!!
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST | CURLSSLOPT_NO_REVOKE);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

        // Perform request and place the response data into the dest pointer
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "Could not perform request: %s\n", curl_easy_strerror(res));
        } else {
            listFetched = 1;
            strcpy(dest, chunk.mem);
        }

        curl_easy_cleanup(curl);
        free(chunk.mem);
    }

    curl_global_cleanup();
}

// ---------------------------- MAIN ----------------------------

int main() {
    char JSON[4096] = "";
    char cur[100] = "";
    struct LucidClient Conns[256];
    int numConns = 0;
    int validSel;
    int selection = 257;
    int pid;
    char logName[42] = "";
    int log;
    int sshCmd;

    // Get and Parse client list
    getClientList(JSON);
    if (listFetched == 0) return -1;
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

    // Main observer navigation
    while (1) {
        selection = 257;

        // First, have the user pick a client to connect to.
        while (selection >= numConns) {
            printf("Enter a client to connect to: ");
            fgets(cur, sizeof(cur), stdin);

            // Prompt again on empty response, or allow user to exit.
            if (0 == strcmp("\n", cur)) continue;
            cur[strcspn(cur, "\n")] = 0;
            if (0 == strcmp("exit", cur)) break;

            // Ensure input is an integer, otherwise prompt again.
            validSel = 1;
            for (int i=0; i<strlen(cur); i++)
                if (!isdigit(cur[i])) validSel = 0;
            if (!validSel) continue;
            selection = atoi(cur);
        }

        // User enters "exit" - observer will terminate.
        if (selection >= numConns) {
            printf("Goodbye.\n");
            break;
        }
    
        // Client has been selected, prompt user on whether to connect over RDP or SSH.
        while (0 != strcmp("rdp", cur) && 0 != strcmp("ssh", cur)) {
            printf("Connect via (rdp) or [ssh]? ");
            fgets(cur, sizeof(cur), stdin);
            cur[strcspn(cur, "\n")] = 0;
        }

        // If RDP, fork the observer with a child process that handles the RDP connection.
        // ... Or default to SSH otherwise.
        if (0 == strcmp("rdp", cur)) {
            sprintf(logName, "sdl-freerdp-out-%lu.log", (unsigned long) time(NULL));
            log = open(logName, O_CREAT|O_WRONLY|O_TRUNC, 0644);
            if (!log) {
                fprintf(stderr, "Couldn't open log file for RDP Connection.\n");
                continue;
            }
            pid = fork();
            if (pid < 0) printf("Failed to open RDP Connection.\n");
            else if (pid == 0) {
                dup2(log, 1);
                close(log);
                connectRDP(Conns[selection]);
                return 0;
            }    
        } else {
            sshCmd = 10;
            printf("\nSSH Command options\n");
            printf("1: Shutdown\n");
            printf("2: Restart\n");
            printf("3: Logout (KDE Session)\n");

            // Determine command option
            while (sshCmd >= 4) {
                printf("Select a command to send (or enter 0 followed by a custom command): ");
                fgets(cur, sizeof(cur), stdin);
                if (0 == strcmp("\n", cur)) continue;
                cur[strcspn(cur, "\n")] = 0;
                validSel = 1;
                for (int i=0; i<strlen(cur); i++)
                    if (!isdigit(cur[i])) validSel = 0;
                if (!validSel) continue;
                sshCmd = atoi(cur);
            }
            switch (sshCmd) {
                case 0:
                    printf("> ");
                    fgets(cur, sizeof(cur), stdin);
                    cur[strcspn(cur, "\n")] = 0;
                    connectSSH(Conns[selection], cur);
                    break;
                case 1:
                    connectSSH(Conns[selection], MACRO_SHUTDOWN);
                    break;
                case 2:
                    connectSSH(Conns[selection], MACRO_RESTART);
                    break;
                case 3:
                    connectSSH(Conns[selection], MACRO_LOGOUT);
                    break;
            }
        }
    }

    return 0;
}