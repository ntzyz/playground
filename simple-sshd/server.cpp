#include <iostream>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <unistd.h>

#include <libssh/libssh.h>
#include <libssh/server.h>

#define KEYS_FOLDER "/home/ntzyz/Document/ssh-hello/ssh/"

using namespace std;

class Server {
private:
    ssh_session session;
    ssh_bind sshbind;
    ssh_message message;
    ssh_channel chan = 0;
    string port;
    string addr;
    void setup() {
        sshbind = ssh_bind_new();
        session = ssh_new();

        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_DSAKEY, KEYS_FOLDER "ssh_host_dsa_key");
        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, KEYS_FOLDER "ssh_host_rsa_key");
        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, addr.c_str());
        ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, port.c_str());
    }

public: 
    function<bool (string, string&)> handler;
    string PS1 = "[NSH]$ ";

    Server(string _addr, string _port) {
        addr = _addr;
        port = _port;
        setup();
    };
    
    Server(const char *_addr, const char * _port) {
        addr = string(_addr);
        port = string(_port);
        setup();
    };

    bool listen() {
        if (ssh_bind_listen(sshbind) < 0) {
            cerr << "Bind error: " << ssh_get_error(sshbind) << endl;
            return false;
        }
        
        cout << "Server started on " << addr << ":" << port << endl;

        for (;;) {
            if (ssh_bind_accept(sshbind, session) == SSH_ERROR) {
                cerr << "Accept error: " << ssh_get_error(sshbind) << endl;
                continue;
            }

            pid_t pid = fork();
            if (pid == 0) {
                cout << "Starting child process ..." << endl;
                process();
                cout << "Stoping child process ..." << endl;
                exit(0);
            }
        }
    }

    bool login() {
        if (ssh_handle_key_exchange(session)) {
            cerr << "ssh_handle_key_exchange: " << ssh_get_error(session) << endl;
            return false;
        }

        for(int count = 0; count != 3; count++) {
            message = ssh_message_get(session);
            if (!message)
                break;
            if (ssh_message_type(message) != SSH_REQUEST_AUTH) {
                ssh_message_reply_default(message);
            }
            else if (ssh_message_subtype(message) == SSH_AUTH_METHOD_PASSWORD) {
                cout << "User " << ssh_message_auth_user(message) << " wants to auth with pass " << ssh_message_auth_password(message) << endl;
                // Just let it pass ...
                ssh_message_auth_reply_success(message, 0);
                return true;
            }
            else {
                ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);
                ssh_message_reply_default(message);
            }
            ssh_message_free(message);
        }
        return false;
    }

    void process() {
        int i;
        const size_t buff_size = 16384;
        char buf[buff_size];
        memset(buf, 0, buff_size);

        if (!login()) {
            cout << "Remote side login failed." << endl;
            ssh_disconnect(session);
            ssh_finalize();
            goto quit;
        }
        if (!getChannel()) {
            cout << "getChannel failed." << endl;
            ssh_disconnect(session);
            ssh_finalize();
            goto quit;
        }
        if (!getShell()) {
            cout << "getShell failed." << endl;
            ssh_disconnect(session);
            ssh_finalize();
            goto quit;
        }

        do {
            ssh_channel_write(chan, PS1.c_str(), PS1.length());

            i = ssh_channel_read(chan, buf, 2048, 0);
            buf[strlen(buf) - 1] = '\0';
            string request = buf, response;
            bool eof = handler(request, response);

            ssh_channel_write(chan, response.c_str(), response.length());
            ssh_channel_write(chan, "\n", 1);
            if (eof)
                break;
        } while (i > 0);

    quit:
        if (chan)
            ssh_channel_close(chan);
        if (session)
            ssh_disconnect(session);
        ssh_finalize();
    }

    bool getShell() {
        for (int count = 0; count != 3; count++) {
            message = ssh_message_get(session);
            if (message == NULL) {
                return false;
            }
            else if(ssh_message_type(message) == SSH_REQUEST_CHANNEL && ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_SHELL) {
                ssh_message_channel_request_reply_success(message);
                ssh_message_free(message);
                return true;
            }
            else {
                ssh_message_reply_default(message);
                ssh_message_free(message);
            }
        }
        return false;
    }

    bool getChannel() {
        for (int count = 0; count != 3; count++) {
            message = ssh_message_get(session);
            if(message) {
                if (ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN && ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
                    chan = ssh_message_channel_request_open_reply_accept(message);
                    return true;
                }
                else {
                    ssh_message_reply_default(message);
                }
                ssh_message_free(message);
            }
        }
        return false;
    }
};

int main() {
    Server server("0.0.0.0", "2022");

    server.handler = [](string request, string& response) {
        if (request == string("ping")) {
            response = string("Pong!");
        }
        else if (request == string("exit")) {
            response = string("exiting...");
            return true;
        }
        else {
            response = "ECHO: ";
            response += request;
        }
        return false;
    };

    return server.listen();
}
