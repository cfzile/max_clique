#include <bits/stdc++.h>
#include <random>
#include <functional>

#ifndef IL_STD
#define IL_STD
#endif

#include <ilcplex/ilocplex.h>

#define EPS 1e-6

ILOSTLBEGIN

using namespace std;

std::random_device rd;
std::mt19937 rng(rd());

typedef pair<int, int> EDGE;
typedef vector<vector<int>> GRAPH_MATRIX;
typedef vector<EDGE> EDGES;

struct GRAPH {
    GRAPH_MATRIX graph_matrix;
    EDGES edges;
    int number_vertices;
    vector<vector<int>> num_edges;
};

GRAPH read_graph(const string &filename) {
    ifstream file("../DIMACS_subset_ascii/" + filename);

    string str;
    int number_vertices = 0, number_edges = 0;
    while (getline(file, str)) {
        if (str[0] == 'p') {
            str = str.substr(str.find(' ') + 1);
            str = str.substr(str.find(' ') + 1);
            int space1 = str.find(' ');
            number_vertices = stoi(str.substr(0, space1));
            str = str.substr(space1 + 1);
            number_edges = stoi(str);
            break;
        }
    }

    GRAPH_MATRIX graph_matrix(number_vertices, vector<int>(number_vertices, 0));
    vector<vector<int>> num_edges(number_vertices);
    EDGES edges;
    for (int i = 0; i < number_edges; i++) {
        char c;
        int a, b;
        file >> c >> a >> b;
        a--;
        b--;
        num_edges[a].push_back(b);
        num_edges[b].push_back(a);
        graph_matrix[a][b] = graph_matrix[b][a] = 1;
        edges.push_back({a, b});
    }

    for (auto &i: num_edges)
        sort(i.begin(), i.end(), [num_edges](int a, int b) {
            return num_edges[a].size() > num_edges[b].size();
        });

    file.close();

    return GRAPH{graph_matrix, edges, number_vertices, num_edges};
}

vector<pair<string, int>> read_info() {
    ifstream info("../info.csv");

    string str;
    getline(info, str);
    getline(info, str);

    vector<pair<string, int>> result_info;

    while (getline(info, str)) {
        if (str == "//")
            break;
        vector<string> cols;
        string cur_string;
        for (int i = 0; i <= (int) str.size(); i++) {
            if (i == (int) str.size() || str[i] == ',') {
                cols.push_back(cur_string);
                cur_string = "";
            } else
                cur_string += str[i];
        }
        result_info.emplace_back(cols[0] + ".clq", stoi(cols[1]));
    }
    return result_info;
}

double getDiff(chrono::_V2::system_clock::time_point time) {
    return (double) chrono::duration_cast<std::chrono::milliseconds>(chrono::system_clock::now() - time).count();
}