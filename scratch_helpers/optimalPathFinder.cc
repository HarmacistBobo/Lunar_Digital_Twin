#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <queue>
#include <cmath>
#include <limits>
#include <algorithm>

using namespace std;

// Struct to hold node data
struct NodePosition {
    double x, y, z;
};

// Function to calculate Euclidean distance between nodes
static double distanceBetween(const NodePosition& a, const NodePosition& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    double dz = a.z - b.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

// Dijkstra’s algorithm to find shortest path between two nodes
vector<string> findOptimalPath(
    const unordered_map<string, NodePosition>& nodes,
    const unordered_map<string, vector<string>>& adjacency,
    const string& start, const string& goal)
{
    unordered_map<string, double> dist;
    unordered_map<string, string> prev;

    for (auto& n : nodes)
        dist[n.first] = numeric_limits<double>::infinity();

    dist[start] = 0.0;

    auto cmp = [&](const string& a, const string& b) {
        return dist[a] > dist[b];
    };

    priority_queue<string, vector<string>, decltype(cmp)> pq(cmp);
    pq.push(start);

    while (!pq.empty()) {
        string u = pq.top();
        pq.pop();

        if (u == goal) break;
        if (adjacency.find(u) == adjacency.end()) continue;

        for (const string& v : adjacency.at(u)) {
            if (nodes.find(v) == nodes.end()) continue;
            double alt = dist[u] + distanceBetween(nodes.at(u), nodes.at(v));
            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
                pq.push(v);
            }
        }
    }

    vector<string> path;
    if (dist[goal] == numeric_limits<double>::infinity()) return path; // no path

    for (string at = goal; !at.empty(); at = prev.count(at) ? prev[at] : "")
        path.push_back(at);

    reverse(path.begin(), path.end());
    return path;
}
