#include "general.h"

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
                    the_best = this->min_independent_set_size;
                }
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

    double cplex_solve() {
        if (candidates.empty())
            return (double) cur_solution.size();

        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());

        double ans = -1;
        if (cplex.solve()) {
            size_t large_zero = candidates.size() - 1;
            for (size_t i = 0; i < candidates.size(); ++i)
                if (cplex.getValue(x[candidates[i]]) > cplex.getValue(x[candidates[large_zero]])) {
                    large_zero = i;
                }

            if (!candidates.empty() && large_zero != candidates.size() - 1)
                swap(candidates[candidates.size() - 1], candidates[large_zero]);

            ans = cplex.getObjValue();
        }

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
            for (int attempt = 0; attempt < 20 * number_vertices; ++attempt) {
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

        BnB();

        return solution;
    }

    bool TL = false;

    void BnB() {

        if (getDiff(this->start) > tl) {
            TL = true;
            return;
        }


        if ((int) candidates.size() + (int) cur_solution.size() <= global_answer)
            return;

        bool prev_vertex_in = (prev_vertex_value == 1);
        bool is_first_vertex = (prev_vertex == -1);

        IloRange constraint;
        IloExpr expr(env);

        if (!is_first_vertex) {
            if (prev_vertex_in)
                cur_solution.push_back(prev_vertex);
            expr = x[prev_vertex];
            constraint = IloRange(env, prev_vertex_value, expr, prev_vertex_value);
            model.add(constraint);
        }

        double sol = cplex_solve();

        if (floor(sol + EPS) <= cur_solution.size() || floor(sol + EPS) <= global_answer) {
            if (global_answer < (int) cur_solution.size()) {
                global_answer = cur_solution.size();
                solution = MAX_CLIQUE{global_answer, cur_solution};
            }
            if (prev_vertex_in)
                cur_solution.pop_back();
            if (!is_first_vertex)
                model.remove(constraint);
            expr.end();
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

            expr.end();
            return;
        }

        int vertex = candidates.back();
        candidates.pop_back();

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
                BnB();
            }
        }

        candidates.push_back(vertex);

        if (!is_first_vertex) {
            model.remove(constraint);
            if (prev_vertex_in)
                cur_solution.pop_back();
        }
        expr.end();
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
                << "is_clique,is_best_known,graph,time,bnb_sol,best_known,heuristic_clique,was_IL,min_independent_set_size\n";

        result.close();
    }
    for (auto &graph_case: info) {

        cout << "Filename: " << graph_case.first << "\n";
        auto graph = read_graph(graph_case.first);

        auto begin = chrono::system_clock::now();
        MaxCliqueSolver solver = MaxCliqueSolver(graph);
        solver.solve(5 * 60 * 60 * 1000, begin);
        auto time = getDiff(begin);

        cout << "Time: " << time << "; " << solver.TL << "; " << solver.solution.size << "; " << graph_case.second
             << "\n";

        ofstream result("../result.csv", std::ios_base::app);
        result << solver.check_solve() << ",";
        if (solver.solution.size == graph_case.second && solver.check_solve()) {
            result << true << ",";
        } else
            result << false << ",";

        result << graph_case.first << ",";
        result << time << "," << solver.solution.size << "," << graph_case.second << "," << solver.max_clique.size()
               << "," << solver.TL << "," << solver.min_independent_set_size << "\n";

        result.close();
        cout << "\n";

    }
    return 0;
}
