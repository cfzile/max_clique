#include "general.h"
#include <functional>

struct MAX_CLIQUE {
    int size = -1;
    vector<int> vertices;
};

class MaxCliqueSolver {

public:
    GRAPH graph;
    int number_vertices;
    int largest_independent_set_size = 1000;
    int min_independent_set_size = 3;
    uniform_int_distribution<int> random_int;

    explicit MaxCliqueSolver(const GRAPH &graph) {
        this->graph = graph;
        this->number_vertices = graph.number_vertices;
        random_int = uniform_int_distribution<int>(0, number_vertices - 1);

    }

    void find_set(int deep, set<int> cur_set, vector<int> &used, set<set<int>> &sets, map<EDGE, bool> &pairs) {

        bool found_large = false;

        auto i = used.begin();
        while (i != used.end() && (int) cur_set.size() < largest_independent_set_size) {
            bool independent = true;
            for (auto &j : cur_set)
                if (graph.graph_matrix[min(*i, j)][max(*i, j)]) {
                    independent = false;
                    break;
                }
            if (independent) {
                int val = *i;
                cur_set.insert(val);
                used.erase(i);
                find_set(deep + 1, cur_set, used, sets, pairs);
                found_large = true;
                cur_set.erase(val);
                i = used.begin();
            } else
                i++;
        }

        if (deep >= min_independent_set_size && (deep == largest_independent_set_size || !found_large)) {
            for (auto &i : cur_set)
                for (auto &j : cur_set)
                    if (pairs.find({min(i, j), max(i, j)}) != pairs.end())
                        pairs.erase({min(i, j), max(i, j)});
            sets.insert(cur_set);
        }
    }

    set<set<int>> find_sets() {
        map<EDGE, bool> pairs;
        for (int i = 0; i < number_vertices; ++i) {
            for (int j = i + 1; j < number_vertices; ++j) {
                if (graph.graph_matrix[i][j])
                    continue;
                pairs[{i, j}] = true;
            }
        }

        set<set<int>> sets;

        cout << "No edges: " << pairs.size() << "\n";

        for (int i = 0; i < number_vertices; ++i) {
            for (int _ = 0; _ < 1; ++_){
                vector<int> used;
                for (int j = 0; j < number_vertices; ++j) {
                    int add = random_int(rng) % number_vertices;
                    if (i != add)
                        used.push_back(add);
                }
                find_set(0, set<int>({i}), used, sets, pairs);
            }
        }

        cout << "Independent sets size 2: " << pairs.size() << "\n";

        for (auto &i : pairs)
            sets.insert(set<int>({i.first.first, i.first.second}));

        cout << "Found independent sets (+ size 2): " << sets.size() << "\n";
        return sets;
    }

    MAX_CLIQUE solve() {
        IloEnv env;
        IloModel model(env);

        std::stringstream name;
        IloBoolVarArray x(env, number_vertices);

        for (auto i = 0; i < number_vertices; ++i) {
            name << "x_" << i;
            x[i] = IloBoolVar(env, 0, 1, name.str().c_str());
            name.str("");
        }

        IloRangeArray independent_sets(env);

        IloExpr expr(env);

        set<set<int>> sets = find_sets();
        for (const auto &set : sets) {
            for (auto &i : set)
                expr += x[i];
            independent_sets.add(IloRange(env, 0, expr, 1));
            expr.clear();
        }

        model.add(independent_sets);

        for (auto i = 0; i < number_vertices; ++i)
            expr += x[i];

        IloObjective obj(env, expr, IloObjective::Maximize);

        model.add(obj);

        expr.end();

        IloCplex cplex(model);
        cplex.exportModel("model.lp");

        bool solved = false;

        try {
            solved = cplex.solve();
        } catch (const IloException &e) {
            std::cerr << "\n\nCPLEX Raised an exception:\n";
            std::cerr << e << "\n";
            env.end();
            throw;
        }

        if (solved) {
            vector<int> vertices;
            for (int i = 0; i < number_vertices; i++) {
                if (cplex.getValue(x[i]) >= 1 - EPS)
                    vertices.push_back(i + 1);
            }
            return MAX_CLIQUE{(int) cplex.getObjValue(), vertices};
        }

        env.end();

        return MAX_CLIQUE{};
    }
};

int main() {

    auto info = read_info();
    ofstream result("../result.csv");

    for (auto &graph_case : info) {

        cout << "Filename: " << graph_case.first << "\n";
        auto graph = read_graph(graph_case.first);

        MaxCliqueSolver solver = MaxCliqueSolver(graph);

        auto begin = chrono::system_clock::now();
        MAX_CLIQUE ans = solver.solve();
        auto time = getDiff(begin);

        cout << time << " ";

        assert(ans.vertices.size() == ans.size);

        if (ans.size == graph_case.second) {
            result << graph_case.first << ",";
            result << time << "," << ans.size << "," << graph_case.second << "\n";
            cout << "Accepted\n";
        } else {
            cout << ans.size << " " << graph_case.second << " : Error\n";
        }

    }

    result.close();

    return 0;
}
