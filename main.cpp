#include "general.h"
#include <functional>
#include <sys/resource.h>

//#define _CRT_SECURE_NO_WARNINGS
//#pragma comment(lib, "version.lib")
//#pragma comment(lib, "libversion.a")
//#pragma comment(linker, "/stack:200000000")
//#pragma GCC optimize("Ofast")
//#pragma GCC target("sse,sse2,sse3,ssse3,sse4,popcnt,abm,mmx,avx,tune=native")
//#pragma GCC optimization ("O3")
//#pragma GCC optimization ("unroll-loops")


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
    IloEnv env;
    IloModel model;
    IloFloatVarArray x;

    explicit MaxCliqueSolver(const GRAPH &graph) {
        this->graph = graph;
        this->number_vertices = graph.number_vertices;
        random_int = uniform_int_distribution<int>(0, number_vertices - 1);
        model = IloModel(env);
        x = IloFloatVarArray(env, number_vertices);
        create_problem(find_sets());
    }

    void find_set(int deep, set<int> cur_set, vector<int> &used, set<set<int>> &sets, map<EDGE, bool> &pairs) {

        bool found_large = false;

        auto i = used.begin();
        while (i != used.end() && (int) cur_set.size() < largest_independent_set_size) {
            bool independent = true;
            for (auto &j: cur_set)
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
            for (auto &i: cur_set)
                for (auto &j: cur_set)
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
            for (int _ = 0; _ < 1; ++_) {
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

        for (auto &i: pairs)
            sets.insert(set<int>({i.first.first, i.first.second}));

        cout << "Found independent sets (+ size 2): " << sets.size() << "\n";
        return sets;
    }

    double solve() {
        IloCplex cplex(model);
        cplex.exportModel("model.lp");
        if (cplex.solve()) {
            auto xx = x;
            sort(bounds.begin(), bounds.end(), [&cplex, &xx](int a, int b) {
                return cplex.getValue(xx[a]) < cplex.getValue(xx[b]);
            });
            return cplex.getObjValue();
        }
        return -1;
    }


    void create_problem(set<set<int>> sets) {

        std::stringstream name;

        for (auto i = 0; i < number_vertices; ++i) {
            name << "x_" << i;
            x[i] = IloFloatVar(env, 0., 1., name.str().c_str());
            name.str("");
        }

        IloRangeArray independent_sets(env);

        IloExpr expr(env);

        for (const auto &set: sets) {
            for (auto &i: set)
                expr += x[i];
            independent_sets.add(IloRange(env, 0., expr, 1.));
            expr.clear();
        }

        for (auto i = 0; i < number_vertices; ++i)
            expr += x[i];

        model.add(independent_sets);

        IloObjective obj(env, expr, IloObjective::Maximize);

        model.add(obj);

        expr.end();

    }

    MAX_CLIQUE BnB() {
        bounds.reserve(number_vertices);
        for (int i = 0; i < number_vertices; i++)
            bounds.push_back(i);

        vector<int> v(number_vertices, 0);
        for (int i = 0; i < number_vertices; i++)
            for (int j = 0; j < number_vertices; j++)
                v[i] += graph.graph_matrix[i][j];

        sort(bounds.begin(), bounds.end(), [&v](int a, int b) {
            return v[a] < v[b];
        });
        BnB_();
        return gbAns;
    }

    MAX_CLIQUE gbAns;
    int globalAns = -1;
    vector<int> solution;
    vector<int> bounds;
    int v = -1, value = 0;

    void BnB_() {

        if ((int) bounds.size() + (int) solution.size() <= globalAns)
            return;

        bool b = value == 1;
        cout << v << " " << globalAns << " gb\n";
        IloRange constraint;
        IloExpr expr(env);

        if (v != -1) {
            if (b)
                solution.push_back(v);

            expr = x[v];
            constraint = IloRange(env, value, expr, value);
            model.add(constraint);

            double sol = solve();

            cout << "sols " << sol << " " << solution.size() << "\n";

            if (floor(sol) == solution.size()) {
                if (globalAns < sol) {
                    globalAns = floor(sol);
                    gbAns = MAX_CLIQUE{(int) floor(sol), solution};
                }
            }

            if (floor(sol) <= globalAns) {
                expr.end();
                model.remove(constraint);
                if (b)
                    solution.pop_back();
                return;
            }
        }

        if (bounds.empty()) {
            globalAns = solution.size();
            gbAns = MAX_CLIQUE{(int) solution.size(), solution};
            model.remove(constraint);
            solution[v] = -1;
            return;
        }

        int vertex = bounds.back();
        bounds.pop_back();

        bool add = true;
        for (int i: solution)
            if (graph.graph_matrix[vertex][i] == 0)
                add = false;

        if (add) {
            v = vertex, value = 1;
            BnB_();
        }

        v = vertex, value = 0;
        BnB_();

        bounds.push_back(vertex);

        if (v != -1) {
            model.remove(constraint);
            if (b)
                solution.pop_back();
        }
    }

};

int main() {
//    {
//        const rlim_t kStackSize = 1024 * 1024 * 1024;   // min stack size = 16 MB
//        struct rlimit rl;
//        int result;
//
//        result = getrlimit(RLIMIT_STACK, &rl);
//        if (result == 0) {
//            if (rl.rlim_cur < kStackSize) {
//                rl.rlim_cur = kStackSize;
//                result = setrlimit(RLIMIT_STACK, &rl);
//                if (result != 0) {
//                    cout << "setrlimit returned result = " <<  result;
//                }
//            }
//        }
//
//    }
    auto info = read_info();

    ofstream result("../result.csv");

    for (auto &graph_case : info) {

        cout << "Filename: " << graph_case.first << "\n";
        auto graph = read_graph(graph_case.first);

        MaxCliqueSolver solver = MaxCliqueSolver(graph);

        auto begin = chrono::system_clock::now();
        MAX_CLIQUE ans = solver.BnB();
        auto time = getDiff(begin);

        cout << time << " ";

//        assert(ans.vertices.size() == ans.size);

        if (ans.size == graph_case.second) {
            result << graph_case.first << ",";
            result << time << "," << ans.size << "," << graph_case.second << "\n";
            cout << "Accepted\n";
        } else {
            cout << ans.size << " " << graph_case.second << " : Error\n";
            throw;
        }

    }

    result.close();

    return 0;
}
