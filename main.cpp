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

struct MAX_CLIQUE {
    int size = -1;
    vector<int> vertices;
};

class MaxCliqueSolver {

public:
    GRAPH graph;
    int number_vertices;
    int largest_independent_set_size;
    int min_independent_set_size = 3;
    uniform_int_distribution<int> random_int;
    IloEnv env;
    IloModel model;
    IloFloatVarArray x;
    MAX_CLIQUE solution;
    int global_answer = -1;
    vector<int> cur_solution;
    vector<int> candidates;
    int prev_vertex = -1, prev_vertex_value = 0;
    vector<int> max_clique;
    set<vector<int>> all_sets;

    int tl = -1;

    explicit MaxCliqueSolver(const GRAPH &graph, int min_independent_set_size = -1) {
        this->graph = graph;
        this->number_vertices = graph.number_vertices;

        largest_independent_set_size = number_vertices;
        vector<vector<int>> independent_sets;
        if (min_independent_set_size == -1) {
            int the_best = 3;
            for (this->min_independent_set_size = 3;
                 this->min_independent_set_size <= 10; this->min_independent_set_size += 2) {
                auto sets = find_independent_sets();
                if (sets.size() < independent_sets.size() || independent_sets.empty()) {
                    independent_sets = sets;
                    all_sets = set<vector<int>>(sets.begin(), sets.end());
                    the_best = this->min_independent_set_size;
                } else
                    break;
            }
            this->min_independent_set_size = the_best;
        } else {
            this->min_independent_set_size = min_independent_set_size;
            independent_sets = find_independent_sets();
        }
        cout << "Min independent set size: " << this->min_independent_set_size << "\n";
        cout << "Independent sets: " << independent_sets.size() << "\n";
        random_int = uniform_int_distribution<int>(0, number_vertices - 1);
        model = IloModel(env);
        x = IloFloatVarArray(env, number_vertices);
        create_problem(independent_sets);
    }


    void find_independent_set(vector<int> cur_set, vector<int> &unused, vector<vector<int>> &independent_sets,
                              map<EDGE, bool> &no_edge) {

        if (no_edge.empty())
            return;

        bool found_large = false;

        auto vertex = unused.begin();
        int deep = cur_set.size();

        while (vertex != unused.end() && (int) cur_set.size() < largest_independent_set_size) {
            bool add = true;
            for (auto &j: cur_set)
                if (graph.graph_matrix[j][*vertex] || *vertex == j) {
                    add = false;
                    break;
                }
            if (add) {
                int val = *vertex;
                cur_set.push_back(val);
                unused.erase(vertex);
                find_independent_set(cur_set, unused, independent_sets, no_edge);
                found_large = true;
                cur_set.pop_back();
                vertex = unused.begin();
            } else
                vertex++;
        }

        if (deep >= min_independent_set_size && (deep == largest_independent_set_size || !found_large)) {
            int c = 0;
            for (auto &i: cur_set)
                for (auto &j: cur_set)
                    if (no_edge.find({min(i, j), max(i, j)}) != no_edge.end()) {
                        no_edge.erase({min(i, j), max(i, j)});
                        c += 1;
                    }
            if (c != 0)
                independent_sets.push_back(cur_set);
        }
    }

    vector<vector<int>> find_independent_sets() {
        map<EDGE, bool> no_edge;
        for (int i = 0; i < number_vertices; ++i)
            for (int j = i + 1; j < number_vertices; ++j) {
                if (graph.graph_matrix[i][j])
                    continue;
                no_edge[{i, j}] = true;
            }

        cout << "No edges: " << no_edge.size() << "\n";

        vector<vector<int>> independent_sets;

        for (int vertex = 0; vertex < number_vertices; ++vertex)
            for (int attempt = 0; attempt < 10; ++attempt) {
                vector<int> unused;
                for (int j = 0; j < number_vertices; ++j) {
                    int vertex_to_add = random_int(rng) % number_vertices;
                    if (vertex != vertex_to_add)
                        unused.push_back(vertex_to_add);
                }
                find_independent_set(vector<int>({vertex}), unused, independent_sets, no_edge);
            }


        cout << "Independent sets size 2: " << no_edge.size() << "\n";

        for (auto &i: no_edge)
            independent_sets.push_back(vector<int>({i.first.first, i.first.second}));

        cout << "Found independent sets (+ size 2): " << independent_sets.size() << "\n";
        return independent_sets;
    }

    void create_problem(const vector<vector<int>> &sets) {

        for (auto i = 0; i < number_vertices; ++i)
            x[i] = IloFloatVar(env, 0., 1.);

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

    pair<double, vector<pair<double, int>>> cplex_solution;
    vector<double> cplex_solution_2;

    bool all_integers;

    double cplex_solve() {
        all_integers = false;

        if (candidates.empty())
            return (double) cur_solution.size();

        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());

        double ans = -1;

        if (cplex.solve()) {
            size_t large_zero = candidates.size() - 1;
            vector<pair<double, int>> xs;

            cplex_solution_2.resize(number_vertices);

            all_integers = true;
            for (int i = 0; i < number_vertices; ++i) {
                cplex_solution_2[i] = cplex.getValue(x[i]);
                if (!(abs(1. - cplex.getValue(x[i])) < EPS || abs(cplex.getValue(x[i])) < EPS)) {
                    all_integers = false;
                }
            }

            for (size_t i = 0; i < candidates.size(); ++i) {
                if (cplex.getValue(x[candidates[i]]) > cplex.getValue(x[candidates[large_zero]])) {
                    large_zero = i;
                }
                xs.push_back({cplex.getValue(x[candidates[i]]), candidates[i]});
            }

            if (!candidates.empty() && large_zero != candidates.size() - 1)
                swap(candidates[candidates.size() - 1], candidates[large_zero]);

            ans = cplex.getObjValue();
            cplex_solution = {ans, xs};
        } else
            all_integers = false;

        cplex.end();
        return ans;
    }

    void find_clique(vector<int> &cur_clique) {
        if (cur_clique.size() > max_clique.size())
            max_clique = cur_clique;
        for (int attempt = 0; attempt < number_vertices; ++attempt) {
            auto vertex = random_int(rng) % number_vertices;
            bool add = true;
            for (auto &j: cur_clique)
                if (graph.graph_matrix[vertex][j] == 0 || vertex == j)
                    add = false;
            if (add) {
                cur_clique.push_back(vertex);
                find_clique(cur_clique);
                cur_clique.pop_back();
                break;
            }
        }
    }

    chrono::time_point<chrono::system_clock, chrono::duration<long, ratio<1, 1000000000>>> start;

    MAX_CLIQUE
    solve(int tl, chrono::time_point<chrono::system_clock, chrono::duration<long, ratio<1, 1000000000>>> start) {
        this->start = start;
        this->tl = tl;

        candidates.reserve(number_vertices);

        for (int vertex = 0; vertex < number_vertices; ++vertex)
            for (int attempt = 0; attempt < 5 * number_vertices; ++attempt) {
                vector<int> cur;
                cur.push_back(vertex);
                find_clique(cur);
            }

        cout << "Initial max clique size: " << max_clique.size() << "\n";

        set<int> max_clique_set(max_clique.begin(), max_clique.end());

        for (int i = 0; i < number_vertices; i++) {
            if (max_clique_set.find(i) == max_clique_set.end())
                candidates.push_back(i);
        }

        for (auto &i: max_clique_set)
            candidates.push_back(i);

        solution = {(int) max_clique_set.size(), max_clique};
        global_answer = (int) max_clique_set.size();

        BnC();

        return solution;
    }

    set<pair<double, vector<int>>> cur_sets;

    void find_constraint(vector<int> &cur_constraint, double sum, vector<pair<double, int>> &free) {

        if (!cur_sets.empty())
            return;

        while (!free.empty()) {
            auto i = *free.begin();
            bool add = true;
            for (auto j: cur_constraint)
                if (i.second == j || graph.graph_matrix[i.second][j] == 1) {
                    add = false;
                    break;
                }
            free.erase(free.begin());
            if (add) {
                cur_constraint.push_back(i.second);
                find_constraint(cur_constraint, sum + i.first, free);
                cur_constraint.pop_back();
                return;
            }
        }

        if (sum > 1. + EPS && (int) cur_constraint.size() >= 5) {
            sort(cur_constraint.begin(), cur_constraint.end());
            if (all_sets.find(cur_constraint) != all_sets.end())
                return;
            all_sets.insert(cur_constraint);
            cur_sets.insert({sum, cur_constraint});
        }
    }

    void separation() {
        cur_sets.clear();
        auto S = vector<int>();

        double init_f = cplex_solution.first;

        find_constraint(S, 0, cplex_solution.second);

        IloRange constraint;
        IloExpr expr(env);

        if (!cur_sets.empty()) {
            auto cur_constraint = cur_sets.rbegin()->second;
            expr.clear();
            for (auto i: cur_constraint)
                expr += x[i];

            constraint = IloRange(env, 0, expr, 1);
            model.add(constraint);

            double f = cplex_solve();
            if (f != init_f && !all_integers) {
                model.remove(constraint);
            }
        }
        constraint.end();
        expr.end();

    }

    bool TL = false;

    void BnC() {

        if (getDiff(this->start) > tl) {
            TL = true;
            return;
        }

        if ((int) candidates.size() + (int) cur_solution.size() <= global_answer)
            return;

        bool prev_vertex_in = (prev_vertex_value == 1);
        bool is_first_vertex = (prev_vertex == -1);

        IloRange constraint;

        if (!is_first_vertex) {
            if (prev_vertex_in)
                cur_solution.push_back(prev_vertex);
            constraint = IloRange(env, prev_vertex_value, x[prev_vertex], prev_vertex_value);
            model.add(constraint);
        }

        double sol = cplex_solve();

        if (all_integers) {
            if (cplex_solution.first + EPS > global_answer) {
                global_answer = (int)(cplex_solution.first + EPS);
                vector<int> cur_solution_;
                for (int i = 0; i < number_vertices; i++)
                    if (abs(cplex_solution_2[i] - 1) < EPS)
                        cur_solution_.push_back(i);
                if ((int) cur_solution_.size() != (int) (EPS + cplex_solution.first) || (int) cur_solution_.size() != global_answer)
                    throw;
                solution = MAX_CLIQUE{global_answer, cur_solution_};
            }
            if (prev_vertex_in)
                cur_solution.pop_back();
            if (!is_first_vertex)
                model.remove(constraint);
            return;
        }

        if (floor(sol + EPS) <= cur_solution.size() || floor(sol + EPS) <= global_answer) {
            if (global_answer < (int) cur_solution.size()) {
                global_answer = cur_solution.size();
                solution = MAX_CLIQUE{global_answer, cur_solution};
            }
            if (prev_vertex_in)
                cur_solution.pop_back();
            if (!is_first_vertex)
                model.remove(constraint);
            return;
        }

        if (candidates.empty()) {
            if ((int) cur_solution.size() > global_answer) {
                global_answer = cur_solution.size();
                solution = MAX_CLIQUE{(int) cur_solution.size(), cur_solution};
            }

            if (prev_vertex_in)
                cur_solution.pop_back();
            if (!is_first_vertex)
                model.remove(constraint);

            return;
        }

        int vertex = candidates.back();
        candidates.pop_back();

        separation();

        bool add[2] = {true, true};
        for (int i: cur_solution)
            if (graph.graph_matrix[vertex][i] == 0) {
                add[1] = false;
                break;
            }

        int h = 1;
        for (auto &v: {h, 1 - h}) {
            if (add[v]) {
                prev_vertex = vertex, prev_vertex_value = v;
                BnC();
            }
        }

        candidates.push_back(vertex);

        if (!is_first_vertex) {
            model.remove(constraint);
            if (prev_vertex_in)
                cur_solution.pop_back();
        }
    }

    bool check_solve() {
        for (auto &v1: solution.vertices)
            for (auto &v2: solution.vertices)
                if (v1 != v2 && graph.graph_matrix[v1][v2] == 0) {
                    return false;
                }
        return (int) solution.vertices.size() == solution.size;
    }

};

int main() {

    auto info = read_info();
    {
        ofstream result("../result.csv", std::ios_base::app);

        result
                << "is_clique,is_best_known,graph,time,bnb_sol,best_known,heuristic_clique,was_TL,min_independent_set_size\n";

        result.close();
    }
    for (auto &graph_case: info) {

        cout << "Filename: " << graph_case.first << "\n";
        auto graph = read_graph(graph_case.first);

        auto begin = chrono::system_clock::now();
        MaxCliqueSolver solver = MaxCliqueSolver(graph);
        solver.solve(2 * 60 * 60 * 1000, begin);
        auto time = getDiff(begin);

        cout << "Time: " << time << "; " << solver.TL << "; " << solver.solution.size << "; " << graph_case.second
             << "\n";

        ofstream result("../result.csv", std::ios_base::app);
        result << solver.check_solve() << ",";
        if (!solver.check_solve())
            throw;
        if (solver.solution.size == graph_case.second) {
            result << true << ",";
        } else {
            result << false << ",";
        }

        result << graph_case.first << ",";
        result << time << "," << solver.solution.size << "," << graph_case.second << "," << solver.max_clique.size()
               << "," << solver.TL << "," << solver.min_independent_set_size << "\n";

        result.close();
        cout << "\n";

    }
    return 0;
}
