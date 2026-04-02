#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <fstream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <sstream> // Needed for string splitting

struct Service {
    std::string name;
    std::vector<std::string> args; // Holds the command AND all arguments
    pid_t current_pid = -1;
};

std::vector<Service> services;

// Helper function to split a line by spaces
std::vector<std::string> split_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to read services from a text file
void load_services(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[fiRC] Could not open config file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        std::vector<std::string> tokens = split_line(line);
        if (tokens.size() < 2) continue; // Need at least a name and a command

        Service s;
        s.name = tokens[0];
        
        // Everything after the name is the command and its arguments
        for (size_t i = 1; i < tokens.size(); ++i) {
            s.args.push_back(tokens[i]);
        }
        
        services.push_back(s);
    }
}

void start_service(Service& service) {
    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "[fiRC] Failed to fork for " << service.name << std::endl;
    } else if (pid == 0) {
        // Child process
        std::cout << "[Child] Starting service: " << service.name << std::endl;

        // Convert the vector of strings into a char* array for execv
        std::vector<char*> c_args;
        for (const auto& arg : service.args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(NULL); // Execv requires a NULL at the end

        execv(c_args[0], c_args.data());

        // If execv fails
        std::cerr << "[Child] Failed to start " << service.args[0] << std::endl;
        exit(1);
    } else {
        // Parent process
        service.current_pid = pid;
        std::cout << "[fiRC] " << service.name << " running with PID: " << pid << std::endl;
    }
}

// Fixed the cut-off here!
void handle_shutdown(int sig) {
    std::cout << "\n[fiRC] Shutdown signal received (" << sig << "). Killing services..." << std::endl;
    for (auto& s : services) {
        if (s.current_pid > 0) {
            std::cout << "[fiRC] Sending SIGTERM to " << s.name << " (PID " << s.current_pid << ")" << std::endl;
            kill(s.current_pid, SIGTERM);
        }
    }
    std::cout << "[fiRC] All services stopped. Goodbye." << std::endl;
    exit(0);
}

int main() {
    signal(SIGINT, handle_shutdown);

    std::cout << "--- fiRC (Θos Init) Starting ---" << std::endl;

    load_services("services.conf");

    if (services.empty()) {
        std::cout << "[fiRC] No services loaded. Exiting." << std::endl;
        return 0;
    }

    // Initial start
    for (auto& s : services) {
        start_service(s);
    }

    while (true) {
        int status;
        pid_t dead_child = waitpid(-1, &status, WNOHANG);

        if (dead_child > 0) {
            std::cout << "[fiRC] Process " << dead_child << " died. Checking registry..." << std::endl;
            
            for (auto& s : services) {
                if (s.current_pid == dead_child) {
                    std::cout << "[fiRC] Service '" << s.name << "' stopped. Restarting now..." << std::endl;
                    start_service(s); 
                }
            }
        }

        usleep(500000); 
    }

    return 0;
}