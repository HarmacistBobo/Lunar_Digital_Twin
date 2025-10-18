#include <iostream>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <filesystem>
#include <vector>
#include "../scratch_helpers/lunarTransmissionSim.cc"
#include "../scratch_helpers/lunarNodeMapGenerator.cc"
#include "../scratch_helpers/lunar_dt_CI.cc"
#include "../scratch_helpers/optimalPathFinder.cc"
#include "../scratch_helpers/LDT_shared.h"

using namespace std;
namespace fs = std::filesystem;

// Function definitions
void displayMenu();
void startSimulation();
void startLunarCISimulation();
void startOptimalPathFinder();
void startMappingSoftware();
void browseConfigurationFile();
extern void simulateTransmission(double distance, double freqMHz, double txPowerdBm, std::string rate);
extern void generateNodeMapXML(const std::vector<NodeConfig>& nodes, const std::string& outputPath);
extern int runLunarDtCI(int argc, char* argv[]);

int main() {
    char userInput = '\0';

    while (true) {
        displayMenu();
        cout << "Enter choice: ";
        cin >> userInput;
        userInput = static_cast<char>(tolower(userInput));

        switch (userInput) {
            case 's':
                cout << "\n[INFO] Starting Simulation...\n";
                startSimulation();
                break;
				
			case 'c':
				cout << "\n[INFO] Launching Lunar CI LTE simulation...\n";
				startLunarCISimulation();
				break;

            case 'p':
                cout << "\n[INFO] Starting Optimal Path Finder...\n";
                startOptimalPathFinder();
                break;
				
			case 'd':
                cout << "\n[INFO] Starting Mapping Software...\n";
                startMappingSoftware();
                break;

            case 'b':
                cout << "\n[INFO] Opening Configuration File Browser...\n";
                browseConfigurationFile();
                break;

            case 'q':
                cout << "\n[INFO] Exiting program.\n";
                return 0;

            default:
                cout << "\n[ERROR] Invalid choice. Please try again.\n";
                break;
        }

        cout << "\n------------------------------------------\n";
    }

    return 0;
}

// --- Function Definitions ---

void displayMenu() {
    cout << "\n=== Main Menu ===\n";
    cout << " [S] Start Simulation\n";
	cout << " [C] Run Lunar CI LTE Simulation\n";
    cout << " [P] Find Optimal Path\n";
	cout << " [D] Display Node Map\n";
    cout << " [B] Browse Configuration File\n";
    cout << " [Q] Quit\n";
}

void startSimulation() {
    string configDir = "./scratch/config";
    vector<fs::path> configFiles;

    cout << "\n=== Simulation Configuration Selector ===\n";

    if (!fs::exists(configDir) || !fs::is_directory(configDir)) {
        cerr << "[ERROR] Directory '" << configDir << "' not found.\n";
        return;
    }

    int index = 1;
    for (const auto &entry : fs::directory_iterator(configDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            cout << "  [" << index++ << "] " << entry.path().filename().string() << '\n';
            configFiles.push_back(entry.path());
        }
    }

    if (configFiles.empty()) {
        cerr << "[ERROR] No configuration files found.\n";
        return;
    }

    int choice;
    cout << "\nSelect a file number to simulate: ";
    if (!(cin >> choice) || choice < 1 || choice > (int)configFiles.size()) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cerr << "[ERROR] Invalid choice.\n";
        return;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string filename = configFiles[choice - 1].string();
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "[ERROR] Could not open " << filename << endl;
        return;
    }

    cout << "\n[INFO] Reading configuration: " << filename << endl;

    // -----------------------------
    // Step 1: Parse node definitions
    // -----------------------------
    struct NodeConfig {
        string name;
        string type;
        double x, y, z;
        double freqMHz;
        double txPowerBm;
        string txRate;
        string rxRate;
        vector<string> links;
    };

    vector<NodeConfig> nodes;
    NodeConfig current;
    string line;

    auto trim = [](string &s) {
        s.erase(0, s.find_first_not_of(" \t\r\n\""));
        s.erase(s.find_last_not_of(" \t\r\n\"") + 1);
    };

    while (getline(file, line)) {
        if (line.find("NODECONFIGHEADER") != string::npos) {
            if (!current.name.empty()) nodes.push_back(current);
            current = NodeConfig();
            continue;
        }

        if (line.find("Name:") != string::npos)
            current.name = line.substr(line.find(':') + 1);
        else if (line.find("Type:") != string::npos)
            current.type = line.substr(line.find(':') + 1);
        else if (line.find("Location:") != string::npos) {
            string locStr = line.substr(line.find(':') + 1);
            replace(locStr.begin(), locStr.end(), ',', ' ');
            stringstream ss(locStr);
            ss >> current.x >> current.y >> current.z;
        } else if (line.find("Transmission Frequency:") != string::npos)
            current.freqMHz = stod(line.substr(line.find(':') + 1));
        else if (line.find("Transmission Power:") != string::npos)
            current.txPowerBm = stod(line.substr(line.find(':') + 1));
        else if (line.find("Transmission Data Rate:") != string::npos)
            current.txRate = line.substr(line.find(':') + 1);
        else if (line.find("Receiver Data Rate:") != string::npos)
            current.rxRate = line.substr(line.find(':') + 1);
        else if (line.find("Linked Nodes:") != string::npos) {
            string links = line.substr(line.find(':') + 1);
            trim(links);
            replace(links.begin(), links.end(), ',', ' ');
            stringstream ss(links);
            string name;
            while (ss >> name) {
                trim(name);
                if (!name.empty())
                    current.links.push_back(name);
            }
        }
    }
    if (!current.name.empty()) nodes.push_back(current);
    file.close();

    // Clean whitespace/quotes
    for (auto &n : nodes) {
        trim(n.name);
        trim(n.type);
        trim(n.txRate);
        trim(n.rxRate);
    }

    cout << "\n[INFO] Parsed " << nodes.size() << " nodes successfully.\n";

    // -----------------------------
    // Step 2: Simulate each link
    // -----------------------------
    auto findNode = [&](const string &name) -> NodeConfig* {
        for (auto &n : nodes) if (n.name == name) return &n;
        return nullptr;
    };

    for (auto &tx : nodes) {
        for (auto &targetName : tx.links) {
            NodeConfig *rx = findNode(targetName);
            if (!rx) {
                cerr << "[WARNING] Target node '" << targetName << "' not found.\n";
                continue;
            }

            double dx = tx.x - rx->x;
            double dy = tx.y - rx->y;
            double dz = tx.z - rx->z;
            double distance = sqrt(dx*dx + dy*dy + dz*dz);

            string effectiveRate = (tx.txRate < rx->rxRate) ? tx.txRate : rx->rxRate;

            cout << "\n[SIM] " << tx.name << " → " << rx->name
                 << " | Distance: " << distance << " m"
                 << " | Freq: " << tx.freqMHz << " MHz"
                 << " | Power: " << tx.txPowerBm << " dBm"
                 << " | Rate: " << effectiveRate << endl;

            simulateTransmission(distance, tx.freqMHz, tx.txPowerBm, effectiveRate);
        }
    }

    cout << "\n[INFO] All transmissions complete.\n";
}

void startLunarCISimulation() {
    string configDir = "./scratch/config/LTE_config";
    vector<fs::path> configFiles;

    cout << "\n=== Lunar CI Simulation Selector ===\n";
    if (!fs::exists(configDir) || !fs::is_directory(configDir)) {
        cerr << "[ERROR] Directory '" << configDir << "' not found.\n";
        return;
    }

    int index = 1;
    for (const auto &entry : fs::directory_iterator(configDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".conf") {
            cout << "  [" << index++ << "] " << entry.path().filename().string() << '\n';
            configFiles.push_back(entry.path());
        }
    }

    if (configFiles.empty()) {
        cerr << "[ERROR] No .conf files found.\n";
        return;
    }

    int choice;
    cout << "\nSelect a file number to run: ";
    if (!(cin >> choice) || choice < 1 || choice > (int)configFiles.size()) {
        cerr << "[ERROR] Invalid choice.\n";
        return;
    }

    string filename = configFiles[choice - 1].string();
    cout << "\n[INFO] Running CI simulation with: " << filename << endl;

    std::string confArg = "--conf=" + filename;
    const char *argv[] = {"lunar_dt_CI", confArg.c_str()};
    runLunarDtCI(2, const_cast<char**>(argv));
}

void startOptimalPathFinder() {
    string configDir = "./scratch/config";
    vector<fs::path> configFiles;

    cout << "\n=== Optimal Path Finder ===\n";

    if (!fs::exists(configDir) || !fs::is_directory(configDir)) {
        cerr << "[ERROR] Directory '" << configDir << "' not found.\n";
        return;
    }

    int index = 1;
    for (const auto &entry : fs::directory_iterator(configDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            cout << "  [" << index++ << "] " << entry.path().filename().string() << '\n';
            configFiles.push_back(entry.path());
        }
    }

    if (configFiles.empty()) {
        cerr << "[ERROR] No configuration files found.\n";
        return;
    }

    int choice;
    cout << "\nSelect a file number: ";
    if (!(cin >> choice) || choice < 1 || choice > (int)configFiles.size()) {
        cerr << "[ERROR] Invalid choice.\n";
        return;
    }

    string filename = configFiles[choice - 1].string();
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "[ERROR] Could not open " << filename << endl;
        return;
    }

    unordered_map<string, NodePosition> nodes;
    unordered_map<string, vector<string>> adjacency;

    string line, currentNode;
    while (getline(file, line)) {
        if (line.find("Name:") != string::npos) {
            currentNode = line.substr(line.find(':') + 1);
            currentNode.erase(remove(currentNode.begin(), currentNode.end(), '\"'), currentNode.end());
            currentNode.erase(remove_if(currentNode.begin(), currentNode.end(), ::isspace), currentNode.end());
        } else if (line.find("Location:") != string::npos && !currentNode.empty()) {
            string locStr = line.substr(line.find(':') + 1);
            replace(locStr.begin(), locStr.end(), ',', ' ');
            stringstream ss(locStr);
            double x, y, z; ss >> x >> y >> z;
            nodes[currentNode] = {x, y, z};
        } else if (line.find("Linked Nodes:") != string::npos && !currentNode.empty()) {
            string links = line.substr(line.find(':') + 1);
            replace(links.begin(), links.end(), ',', ' ');
            stringstream ss(links);
            string node;
            while (ss >> node) {
                node.erase(remove(node.begin(), node.end(), '\"'), node.end());
                adjacency[currentNode].push_back(node);
            }
        }
    }
    file.close();

    cout << "\nAvailable Nodes:\n";
    for (auto &n : nodes) cout << "  - " << n.first << endl;

    string start, goal;
    cout << "\nEnter starting node name: ";
    cin >> start;
    cout << "Enter destination node name: ";
    cin >> goal;

    auto path = findOptimalPath(nodes, adjacency, start, goal);
    if (path.empty()) {
        cout << "\n[ERROR] No valid path found between " << start << " and " << goal << ".\n";
        return;
    }

    cout << "\n[RESULT] Optimal Path:\n  ";
    for (size_t i = 0; i < path.size(); ++i) {
        cout << path[i];
        if (i < path.size() - 1) cout << " -> ";
    }

    double totalDist = 0.0;
    for (size_t i = 1; i < path.size(); ++i)
        totalDist += sqrt(pow(nodes[path[i-1]].x - nodes[path[i]].x, 2) +
                          pow(nodes[path[i-1]].y - nodes[path[i]].y, 2) +
                          pow(nodes[path[i-1]].z - nodes[path[i]].z, 2));

    cout << "\nTotal distance: " << totalDist << " m\n";
}


void startMappingSoftware() {
    string configDir = "./scratch/config";
    vector<fs::path> configFiles;

    cout << "\n=== Node Map Configuration Selector ===\n";
    if (!fs::exists(configDir) || !fs::is_directory(configDir)) {
        cerr << "[ERROR] Directory '" << configDir << "' not found.\n";
        return;
    }

    int index = 1;
    for (const auto &entry : fs::directory_iterator(configDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            cout << "  [" << index++ << "] " << entry.path().filename().string() << '\n';
            configFiles.push_back(entry.path());
        }
    }
    if (configFiles.empty()) { cerr << "[ERROR] No configuration files found.\n"; return; }

    int choice;
    cout << "\nSelect a file number to map: ";
    if (!(cin >> choice) || choice < 1 || choice > (int)configFiles.size()) {
        cerr << "[ERROR] Invalid choice.\n"; return;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string filename = configFiles[choice - 1].string();
    ifstream file(filename);
    if (!file.is_open()) { cerr << "[ERROR] Could not open " << filename << endl; return; }

    cout << "\n[INFO] Reading configuration: " << filename << endl;

    vector<NodeConfig> nodes;
    NodeConfig current;
    string line;

    auto trim = [](string &s){ s.erase(0, s.find_first_not_of(" \t\r\n\""));
                               s.erase(s.find_last_not_of(" \t\r\n\"") + 1); };

    while (getline(file, line)) {
        if (line.find("NODECONFIGHEADER") != string::npos) {
            if (!current.name.empty()) nodes.push_back(current);
            current = NodeConfig();
            continue;
        }
        if (line.find("Name:") != string::npos) current.name = line.substr(line.find(':')+1);
        else if (line.find("Type:") != string::npos) current.type = line.substr(line.find(':')+1);
        else if (line.find("Location:") != string::npos) {
            string loc = line.substr(line.find(':')+1);
            replace(loc.begin(), loc.end(), ',', ' ');
            stringstream ss(loc); ss >> current.x >> current.y >> current.z;
        }
    }
    if (!current.name.empty()) nodes.push_back(current);
    file.close();
    for (auto &n : nodes){ trim(n.name); trim(n.type); }

    cout << "[INFO] Parsed " << nodes.size() << " nodes. Generating map...\n";
    generateNodeMapXML(nodes, "./scratch/output/lunar_node_map.xml");
}


void browseConfigurationFile() {
    string configDir = "./scratch/config";
    vector<fs::path> configFiles;

    cout << "\n=== Configuration File Browser ===\n";

    // Check if config directory exists
    if (!fs::exists(configDir) || !fs::is_directory(configDir)) {
        cerr << "[ERROR] Directory '" << configDir << "' not found.\n";
        return;
    }

    // Collect all .txt files
    cout << "Available configuration files:\n";
    int index = 1;
    for (const auto& entry : fs::directory_iterator(configDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            cout << "  [" << index++ << "] " << entry.path().filename().string() << '\n';
            configFiles.push_back(entry.path());
        }
    }

    if (configFiles.empty()) {
        cout << "[INFO] No configuration files found in '" << configDir << "'.\n";
        return;
    }

    // Ask user to select one
    int choice;
    cout << "\nSelect a file number to open: ";
	if (!(cin >> choice)) {
		cin.clear();                     // clear failbit
		cin.ignore(numeric_limits<streamsize>::max(), '\n');  // discard invalid input
		cerr << "[ERROR] Invalid input. Please enter a number.\n";
		return;
	}
	cin.ignore(numeric_limits<streamsize>::max(), '\n'); // consume leftover newline

    string filename = configFiles[choice - 1].string();
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "[ERROR] Could not open file: " << filename << endl;
        return;
    }

    cout << "\n=== Displaying " << fs::path(filename).filename().string() << " ===\n";

    string line;
    int sectionCount = 0;
    while (getline(file, line)) {
        if (line.find("NODECONFIGHEADER") != string::npos) {
            sectionCount++;
            cout << "\n--------------------------------------\n";
            cout << " Node Configuration #" << sectionCount << "\n";
            cout << "--------------------------------------\n";
            continue;
        }

        if (line.empty()) continue;

        size_t colonPos = line.find(':');
        if (colonPos != string::npos) {
            string key = line.substr(0, colonPos);
            string value = line.substr(colonPos + 1);

			auto trim = [](std::string &s) {
				// Remove leading whitespace and quotes
				s.erase(0, s.find_first_not_of(" \t\r\n\""));
				// Remove trailing whitespace, quotes, and carriage returns
				s.erase(s.find_last_not_of(" \t\r\n\"") + 1);
			};

			trim(key);
			trim(value);


            cout << left << setw(25) << key << ": " << value << '\n';
        }
    }

    if (sectionCount == 0)
        cout << "\n[WARNING] No node configuration sections found.\n";
    else
        cout << "\n[INFO] Displayed " << sectionCount << " node configurations.\n";

    file.close();
}
