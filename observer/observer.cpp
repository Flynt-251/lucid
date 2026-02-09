#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <iostream>
#include <fstream>    // File Handling
#include <filesystem> // For checking if a file exists
#include <string>
#include <vector>
#include <unistd.h>
#include <SFML/Network.hpp>
#include <httplib.h>
#include <json-c/json_object.h>
#include <json-c/json.h>
#include <openssl/x509.h>
#include <httplib.h>
#include <termios.h>


using namespace std;

class RDPserver {
    string ipAddr;
    string user;
    string pass;
    bool isActive;

    public:
    RDPserver(string ip, string username, string password) {
        ipAddr = ip;
        user = username;
        pass = password;
        isActive = false;
    }

    void connectWayland() {
        string cmd = "sdl-freerdp /u:" + user + " /p:" + pass + " /v:" + ipAddr + " /size:1920x1080";
        int status = system(cmd.c_str());
        if (status == -1)
            cout << "Could not start connection.";
    }

    string getUserpath() {
        return user + "@" + ipAddr;
    }

    void refreshConnection() {
        // sf::TcpSocket socket;
        // socket.setBlocking(false);
        sf::Socket::Status status = sf::TcpSocket().connect(ipAddr, 3389, sf::seconds(2.f));
        if (status != sf::Socket::Done) {
            cout << "Connection to " << ipAddr << " timed out.\n";
            return;
        }
        isActive = true;
    }

    bool getActiveStatus() {
        return isActive;
    }

    string toString() {
        return user + "@" + ipAddr + "\t" + (isActive ? "Online" : "Offline");
    }
};

std::string getClientList() {
    std::string email;
    std::string passwd;

    std::cout << "Enter email: ";
    std::cin >> email;

    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::cout << "Enter Password: ";
    std::cin >> passwd;
    // std::cout << "\n" << passwd << "\n";
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    std::cout << "Getting Client List...\n";
    httplib::Client cli("https://localhost:8888");
    cli.enable_server_certificate_verification(false); // Not recommended!
    
    cli.set_basic_auth(email, passwd);

    if (auto reply = cli.Get("/lucid-api")) {
        return reply->body;
    } else {
        std::cout << "Failed to get reply: " << reply.error() << "\n";
        if (auto result = cli.get_openssl_verify_result()) {
            std::cout << X509_verify_cert_error_string(result) << "\n";
        }
        return "";
    }
}

int main() {
    std::vector<RDPserver> connections;

    string JSON = getClientList();
    json_object *root = json_tokener_parse(JSON.c_str());
    json_object *clients = json_object_object_get(root, "clients");
    int n = json_object_array_length(clients);
    for (int i=0; i<n; i++) {
        json_object *temp = json_object_array_get_idx(clients, i);
        json_object *json_hostname = json_object_object_get(temp, "hostname");
        // json_object *json_nickname = json_object_object_get(temp, "nickname");
        // json_object *json_port = json_object_object_get(temp, "port");
        // json_object *json_rdp = json_object_object_get(temp, "rdp");
        connections.push_back(RDPserver(
            json_object_get_string(json_hostname),
            "flynt",
            "flynt"
        ));
    }

    int selection = 9999;
    cout << "Checking all connections...\n";
    bool canConnect = false;
    for (int i=0; i<connections.size(); i++) {
        connections[i].refreshConnection();
    }
    for (int i=0; i<connections.size(); i++) {
        cout << i+1 << "\t" << connections.at(i).toString() << "\n";
    }

    while (selection >= connections.size() || !canConnect) {
        cout << "Select a server to connect to: ";
        cin >> selection;
        if (connections.size() < selection) continue;
        if (selection == 0) return 0;
        else if (!connections[selection-1].getActiveStatus()) {
            cout << "That server is not active. Please select another.\n";
        } else {
            canConnect = true;
        }
    }
    cout << "Connecting to " << connections[selection].getUserpath() << "...\n";
    connections[selection - 1].connectWayland();
    return 0;
}