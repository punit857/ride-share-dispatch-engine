#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <limits>
#include <algorithm>
#include <cmath>

using namespace std;

// 1. Define what a "Road" looks like to our engine
struct Edge {
    int destination;
    double travel_time; // This will hold our live_travel_time
};

// 2. Define the Graph (Adjacency List)
// Key: Source Node ID -> Value: List of connected edges
typedef unordered_map<int, vector<Edge>> Graph;

// 3. Function to load the Kaggle CSV into our C++ Graph
Graph loadGraphFromCSV(const string& filename) {
    Graph city_graph;
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << ". Check your file path!" << endl;
        return city_graph;
    }

    string line;
   
    getline(file, line); 

    // Read the file line by line
    while (getline(file, line)) {
        stringstream ss(line);
        string item;
        
        int node_a, node_b;
        double base_time, multiplier, live_travel_time;

        // Parse the comma-separated values
        getline(ss, item, ','); node_a = stoi(item);
        getline(ss, item, ','); node_b = stoi(item);
        getline(ss, item, ','); base_time = stod(item);
        getline(ss, item, ','); multiplier = stod(item);
        getline(ss, item, ','); live_travel_time = stod(item);

        // Add the road to our graph. 
        // We are building a "directed" graph for simplicity.
        city_graph[node_a].push_back({node_b, live_travel_time});
    }
    
    file.close();
    return city_graph;
}

struct RouteResult {
    double total_time;
    vector<int> path;
};

// 2. The core routing engine using Dijkstra's Algorithm
RouteResult calculateShortestPath(const Graph& graph, int start_node, int end_node) {
    // Min-heap priority queue: stores pairs of (current_time, current_node)
    // std::greater ensures the node with the LOWEST travel time is always at the top
    priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> pq;

    // Track the shortest time to reach each node
    unordered_map<int, double> shortest_times;
    // Track the path (which node did we just come from?)
    unordered_map<int, int> previous_node;

    // Start at the origin
    pq.push({0.0, start_node});
    shortest_times[start_node] = 0.0;

    while (!pq.empty()) {
        double current_time = pq.top().first;
        int current_node = pq.top().second;
        pq.pop();

        //  If we reached the destination, stop searching
        if (current_node == end_node) break;

        // If we already found a faster way to this node in the past, ignore this slower path
        if (current_time > shortest_times[current_node]) continue;

        // Check all roads leaving the current intersection
        auto it = graph.find(current_node);
        if (it != graph.end()) {
            for (const Edge& edge : it->second) {
                double new_time = current_time + edge.travel_time;

                // If this is the first time seeing the destination, OR we found a faster route
                if (shortest_times.find(edge.destination) == shortest_times.end() || 
                    new_time < shortest_times[edge.destination]) {
                    
                    shortest_times[edge.destination] = new_time;
                    previous_node[edge.destination] = current_node; 
                    pq.push({new_time, edge.destination});
                }
            }
        }
    }

    // 3. Reconstruct the final path 
    RouteResult result;
    result.total_time = shortest_times.count(end_node) ? shortest_times[end_node] : -1.0;
    
    if (result.total_time != -1.0) {
        int current = end_node;
        while (current != start_node) {
            result.path.push_back(current);
            current = previous_node[current];
        }
        result.path.push_back(start_node);
        reverse(result.path.begin(), result.path.end()); // Flip it so it goes Start -> End
    }

    return result;
}


struct Driver {
    int id;
    double x; // GPS Longitude / X coordinate
    double y; // GPS Latitude / Y coordinate
    int current_node; // Which intersection they are closest to
};

// *****************************************************************************************************************************
// 2. The Grid-Based Spatial Index
class SpatialIndex {
private:
    double cell_size;
    // Key: Grid ID (e.g., "3_4"), Value: List of drivers in that cell
    unordered_map<string, vector<Driver>> grid;

    // A private helper function to hash 2D coordinates into a 1D string key
    string getGridID(double x, double y) {
        // Floor the coordinates to find which grid square they belong to
        int grid_x = static_cast<int>(floor(x / cell_size));
        int grid_y = static_cast<int>(floor(y / cell_size));
        return to_string(grid_x) + "_" + to_string(grid_y);
    }

public:
    // Constructor: Define how big each grid cell is (e.g., 2.0 miles)
    SpatialIndex(double size) : cell_size(size) {}

    // O(1) Insertion
    void addDriver(const Driver& d) {
        string cell_id = getGridID(d.x, d.y);
        grid[cell_id].push_back(d);
    }

    // O(1) Lookup
    vector<Driver> getDriversNear(double x, double y) {
        string cell_id = getGridID(x, y);
        
        // If the cell exists, return the drivers inside it
        if (grid.find(cell_id) != grid.end()) {
            return grid[cell_id];
        }
        
        // Return an empty list if no drivers are in this cell
        return {}; 
    }
};

int main() {
    // 1. Initialize the Spatial Grid (Let's say each cell is 5.0 units wide)
    SpatialIndex driver_grid(5.0);

    // 2. Simulate 3 drivers scattered around the city logging into the app
    driver_grid.addDriver({101, 12.5, 14.2, 17}); // Driver 101 is near Node 17
    driver_grid.addDriver({102, 45.1, 88.9, 2});  // Driver 102 is far away
    driver_grid.addDriver({103, 11.1, 13.5, 9});  // Driver 103 is near Node 9

    // 3. A Rider opens the app at coordinates (X: 13.0, Y: 14.0) at Node 10
    double rider_x = 13.0;
    double rider_y = 14.0;
    int rider_node = 10;
    
    cout << "Rider requesting a car at coordinates (" << rider_x << ", " << rider_y << ").\n";
    cout << "Scanning local grid cell for drivers...\n";
    cout << "--------------------------------------------------\n";

    // 4. FAST O(1) LOOKUP: Find drivers only in the rider's specific grid cell
    vector<Driver> nearby_drivers = driver_grid.getDriversNear(rider_x, rider_y);

    if (nearby_drivers.empty()) {
        cout << "No drivers available in your area.\n";
        return 0;
    }

    cout << "Found " << nearby_drivers.size() << " driver(s) nearby!\n";
    
    // 5. Load the AI Traffic Graph
    Graph city_map = loadGraphFromCSV("graph_weights.csv");

    // 6. Find the absolute fastest driver using our AI-Weighted Dijkstra's Algorithm
    int best_driver_id = -1;
    double lowest_eta = numeric_limits<double>::max();
    vector<int> best_path;

    for (const Driver& d : nearby_drivers) {
        // Calculate path from Driver to Rider
        RouteResult route = calculateShortestPath(city_map, d.current_node, rider_node);
        
        if (route.total_time != -1.0 && route.total_time < lowest_eta) {
            lowest_eta = route.total_time;
            best_driver_id = d.id;
            best_path = route.path;
        }
    }

    // 7. Dispatch the Driver!
    cout << "\n*** DISPATCH DECISION ***\n";
    cout << "Dispatching Driver #" << best_driver_id << " to the Rider.\n";
    cout << "AI-Predicted ETA: " << lowest_eta << " minutes.\n";
    cout << "Optimal Routing Path: ";
    for (size_t i = 0; i < best_path.size(); ++i) {
        cout << best_path[i];
        if (i != best_path.size() - 1) cout << " -> ";
    }
    cout << "\n";

    return 0;
}