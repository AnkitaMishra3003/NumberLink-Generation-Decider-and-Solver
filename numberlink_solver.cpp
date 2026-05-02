#include <bits/stdc++.h>
using namespace std;

struct Pos {
    int r = -1, c = -1;
    bool operator==(const Pos& o) const { return r == o.r && c == o.c; }
    bool operator!=(const Pos& o) const { return !(*this == o); }
};

struct Move {
    int label = -1;
    int tip_idx = -1;
    bool connect = false;
    Pos from, to;
    int old_deg_from = 0;
    int old_deg_to = 0;
    bool old_done = false;
    Pos old_tip;
};

static string trim(const string& s) {
    size_t i = 0, j = s.size();
    while (i < s.size() && isspace((unsigned char)s[i])) ++i;
    while (j > i && isspace((unsigned char)s[j - 1])) --j;
    return s.substr(i, j - i);
}

static bool parse_two_ints(const string& s, int& a, int& b) {
    istringstream iss(s);
    string x, y, extra;
    if (!(iss >> x >> y) || (iss >> extra)) return false;
    try {
        size_t p1 = 0, p2 = 0;
        a = stoi(x, &p1);
        b = stoi(y, &p2);
        return p1 == x.size() && p2 == y.size();
    } catch (...) {
        return false;
    }
}

static vector<string> read_all_stdin() {
    vector<string> lines;
    string s;
    while (getline(cin, s)) lines.push_back(s);
    return lines;
}

static vector<vector<int>> parse_boxed(const vector<string>& lines) {
    vector<vector<int>> rows;
    int width = -1;
    bool started = false;

    for (const string& raw : lines) {
        string line = trim(raw);
        if (line.rfind("+++", 0) == 0) {
            if (started && !rows.empty()) break;
            continue;
        }
        if (line.empty()) {
            if (started && !rows.empty()) break;
            continue;
        }
        if (line.find('|') == string::npos) {
            continue;
        }
        started = true;
        vector<int> row;
        size_t first = raw.find('|');
        while (first != string::npos) {
            size_t second = raw.find('|', first + 1);
            if (second == string::npos) break;
            string t = trim(raw.substr(first + 1, second - first - 1));
            row.push_back(t.empty() ? 0 : stoi(t));
            first = second;
        }
        if (!row.empty()) {
            if (width == -1) width = (int)row.size();
            if ((int)row.size() != width) throw runtime_error("Inconsistent boxed grid width.");
            rows.push_back(row);
        }
    }
    if (rows.empty()) throw runtime_error("Could not parse boxed puzzle from input.");
    return rows;
}

static vector<vector<int>> parse_plain(const vector<string>& lines) {
    int idx = 0;
    while (idx < (int)lines.size() && trim(lines[idx]).empty()) ++idx;
    if (idx == (int)lines.size()) throw runtime_error("Empty input.");

    int w = 0, h = 0;
    if (!parse_two_ints(trim(lines[idx]), w, h)) {
        throw runtime_error("Expected either boxed puzzle or first line as 'WIDTH HEIGHT'.");
    }
    ++idx;

    vector<vector<int>> rows;
    for (int r = 0; r < h; ++r) {
        while (idx < (int)lines.size() && trim(lines[idx]).empty()) ++idx;
        if (idx == (int)lines.size()) throw runtime_error("Not enough rows in plain puzzle input.");
        string line = trim(lines[idx++]);
        vector<int> row;
        istringstream iss(line);
        string tok;
        while (iss >> tok) {
            if (tok == "." || tok == "0") row.push_back(0);
            else row.push_back(stoi(tok));
        }
        if ((int)row.size() != w) throw runtime_error("Plain puzzle row width mismatch.");
        rows.push_back(row);
    }
    return rows;
}

static vector<vector<int>> parse_input(const vector<string>& lines) {
    for (const string& s : lines) {
        if (s.find('|') != string::npos) return parse_boxed(lines);
    }
    return parse_plain(lines);
}

class NumberlinkSolver {
public:
    explicit NumberlinkSolver(vector<vector<int>> puzzle)
        : clue_raw(std::move(puzzle)), h((int)clue_raw.size()), w((int)clue_raw[0].size()) {
        build_labels();
        owner.assign(h, vector<int>(w, 0));
        deg.assign(h, vector<int>(w, 0));
        is_source.assign(h, vector<char>(w, 0));
        src.assign(labels + 1, {Pos(), Pos()});
        tip.assign(labels + 1, {Pos(), Pos()});
        done.assign(labels + 1, 0);

        vector<int> seen(labels + 1, 0);
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                int x = clue[r][c];
                if (!x) continue;
                owner[r][c] = x;
                is_source[r][c] = 1;
                if (seen[x] == 0) {
                    src[x][0] = {r, c};
                    tip[x][0] = {r, c};
                } else if (seen[x] == 1) {
                    src[x][1] = {r, c};
                    tip[x][1] = {r, c};
                }
                ++seen[x];
            }
        }
    }

    bool solve() {
        return dfs();
    }

    vector<vector<int>> solution_numbers() const {
        vector<vector<int>> out(h, vector<int>(w, 0));
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                out[r][c] = owner[r][c] ? original_label[owner[r][c]] : 0;
            }
        }
        return out;
    }

private:
    vector<vector<int>> clue_raw;
    vector<vector<int>> clue;
    int h, w;
    int labels = 0;
    vector<int> original_label;

    vector<vector<int>> owner;
    vector<vector<int>> deg;
    vector<vector<char>> is_source;
    vector<array<Pos, 2>> src;
    vector<array<Pos, 2>> tip;
    vector<char> done;

    const int DR[4] = {-1, 1, 0, 0};
    const int DC[4] = {0, 0, -1, 1};

    bool inb(int r, int c) const {
        return 0 <= r && r < h && 0 <= c && c < w;
    }

    void build_labels() {
        map<int, int> mp;
        clue = clue_raw;
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                if (clue[r][c] && !mp.count(clue[r][c])) {
                    mp[clue[r][c]] = (int)mp.size() + 1;
                }
            }
        }
        labels = (int)mp.size();
        original_label.assign(labels + 1, 0);
        vector<int> cnt(labels + 1, 0);
        for (auto& [orig, comp] : mp) original_label[comp] = orig;
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                if (clue[r][c]) clue[r][c] = mp[clue[r][c]];
                if (clue[r][c]) ++cnt[clue[r][c]];
            }
        }
        for (int id = 1; id <= labels; ++id) {
            if (cnt[id] != 2) {
                throw runtime_error("Each number must appear exactly twice.");
            }
        }
    }

    bool is_current_tip(int r, int c) const {
        for (int id = 1; id <= labels; ++id) {
            if (done[id]) continue;
            if (tip[id][0].r == r && tip[id][0].c == c) return true;
            if (tip[id][1].r == r && tip[id][1].c == c) return true;
        }
        return false;
    }

    bool is_tip_of_label(int label, int r, int c) const {
        return (!done[label] && ((tip[label][0].r == r && tip[label][0].c == c) ||
                                 (tip[label][1].r == r && tip[label][1].c == c)));
    }

    vector<Move> legal_moves(int label, int tip_idx) const {
        vector<Move> mv;
        if (done[label]) return mv;
        Pos u = tip[label][tip_idx];
        Pos other = tip[label][1 - tip_idx];
        int max_deg_u = is_source[u.r][u.c] ? 1 : 2;
        if (deg[u.r][u.c] >= max_deg_u) return mv;

        for (int k = 0; k < 4; ++k) {
            int nr = u.r + DR[k], nc = u.c + DC[k];
            if (!inb(nr, nc)) continue;
            if (nr == other.r && nc == other.c) {
                int max_deg_v = is_source[nr][nc] ? 1 : 2;
                if (deg[nr][nc] < max_deg_v) {
                    Move m;
                    m.label = label;
                    m.tip_idx = tip_idx;
                    m.connect = true;
                    m.from = u;
                    m.to = {nr, nc};
                    mv.push_back(m);
                }
            } else if (owner[nr][nc] == 0) {
                Move m;
                m.label = label;
                m.tip_idx = tip_idx;
                m.connect = false;
                m.from = u;
                m.to = {nr, nc};
                mv.push_back(m);
            }
        }
        return mv;
    }

    void apply_move(Move& m) {
        m.old_deg_from = deg[m.from.r][m.from.c];
        m.old_deg_to = deg[m.to.r][m.to.c];
        m.old_done = done[m.label];
        m.old_tip = tip[m.label][m.tip_idx];

        deg[m.from.r][m.from.c]++;
        if (m.connect) {
            deg[m.to.r][m.to.c]++;
            done[m.label] = 1;
        } else {
            owner[m.to.r][m.to.c] = m.label;
            deg[m.to.r][m.to.c] = 1;
            tip[m.label][m.tip_idx] = m.to;
        }
    }

    void undo_move(const Move& m) {
        deg[m.from.r][m.from.c] = m.old_deg_from;
        deg[m.to.r][m.to.c] = m.old_deg_to;
        done[m.label] = m.old_done;
        if (!m.connect) owner[m.to.r][m.to.c] = 0;
        tip[m.label][m.tip_idx] = m.old_tip;
    }

    bool final_state_ok() const {
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                if (owner[r][c] == 0) return false;
                if (is_source[r][c]) {
                    if (deg[r][c] != 1) return false;
                } else {
                    if (deg[r][c] != 2) return false;
                }
            }
        }
        return true;
    }

    bool basic_degree_ok() const {
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                if (owner[r][c] == 0) continue;
                int lim = is_source[r][c] ? 1 : 2;
                if (deg[r][c] > lim) return false;
                if (!is_source[r][c] && deg[r][c] < 2 && !is_current_tip(r, c)) return false;

                int same_adj = 0;
                for (int k = 0; k < 4; ++k) {
                    int nr = r + DR[k], nc = c + DC[k];
                    if (inb(nr, nc) && owner[nr][nc] == owner[r][c]) ++same_adj;
                }
                int pending = 0;
                int label = owner[r][c];
                if (!done[label]) {
                    Pos a = tip[label][0], b = tip[label][1];
                    if ((a.r == r && a.c == c) || (b.r == r && b.c == c)) {
                        if (abs(a.r - b.r) + abs(a.c - b.c) == 1) pending = 1;
                    }
                }
                if (same_adj != deg[r][c] + pending) return false;
            }
        }
        return true;
    }

    bool unfinished_pairs_reachable() const {
        vector<vector<int>> vis(h, vector<int>(w, 0));
        for (int id = 1; id <= labels; ++id) {
            if (done[id]) continue;
            Pos s = tip[id][0], t = tip[id][1];
            for (auto& row : vis) fill(row.begin(), row.end(), 0);
            queue<Pos> q;
            q.push(s);
            vis[s.r][s.c] = 1;
            bool ok = false;
            while (!q.empty()) {
                Pos u = q.front(); q.pop();
                if (u == t) {
                    ok = true;
                    break;
                }
                for (int k = 0; k < 4; ++k) {
                    int nr = u.r + DR[k], nc = u.c + DC[k];
                    if (!inb(nr, nc) || vis[nr][nc]) continue;
                    if ((nr == t.r && nc == t.c) || owner[nr][nc] == 0) {
                        vis[nr][nc] = 1;
                        q.push({nr, nc});
                    }
                }
            }
            if (!ok) return false;
        }
        return true;
    }

    bool empty_cell_future_degree_ok() const {
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                if (owner[r][c] != 0) continue;
                int ports = 0;
                for (int k = 0; k < 4; ++k) {
                    int nr = r + DR[k], nc = c + DC[k];
                    if (!inb(nr, nc)) continue;
                    if (owner[nr][nc] == 0 || is_current_tip(nr, nc)) ++ports;
                }
                if (ports < 2) return false;
            }
        }
        return true;
    }

    bool empty_components_ok() const {
        vector<vector<int>> vis(h, vector<int>(w, 0));
        for (int sr = 0; sr < h; ++sr) {
            for (int sc = 0; sc < w; ++sc) {
                if (owner[sr][sc] != 0 || vis[sr][sc]) continue;
                queue<Pos> q;
                q.push({sr, sc});
                vis[sr][sc] = 1;
                set<pair<int,int>> adj_tips;
                while (!q.empty()) {
                    Pos u = q.front(); q.pop();
                    for (int k = 0; k < 4; ++k) {
                        int nr = u.r + DR[k], nc = u.c + DC[k];
                        if (!inb(nr, nc)) continue;
                        if (owner[nr][nc] == 0 && !vis[nr][nc]) {
                            vis[nr][nc] = 1;
                            q.push({nr, nc});
                        } else if (owner[nr][nc] != 0 && is_current_tip(nr, nc)) {
                            adj_tips.insert({nr, nc});
                        }
                    }
                }
                if ((int)adj_tips.size() < 2) return false;
            }
        }
        return true;
    }

    bool stuck_tip_ok() const {
        for (int id = 1; id <= labels; ++id) {
            if (done[id]) continue;
            if (legal_moves(id, 0).empty()) return false;
            if (legal_moves(id, 1).empty()) return false;
        }
        return true;
    }

    bool prune_ok() const {
        return basic_degree_ok() &&
               stuck_tip_ok() &&
               unfinished_pairs_reachable() &&
               empty_cell_future_degree_ok() &&
               empty_components_ok();
    }

    bool forced_propagate(vector<Move>& history) {
        while (true) {
            bool progressed = false;
            for (int id = 1; id <= labels; ++id) {
                if (done[id]) continue;
                for (int t = 0; t < 2; ++t) {
                    auto mv = legal_moves(id, t);
                    if (mv.empty()) return false;
                    if ((int)mv.size() == 1) {
                        Move m = mv[0];
                        apply_move(m);
                        history.push_back(m);
                        if (!prune_ok()) return false;
                        progressed = true;
                        goto next_round;
                    }
                }
            }
            if (!progressed) break;
        next_round:;
        }
        return true;
    }

    bool choose_branch(int& out_label, int& out_tip, vector<Move>& out_moves) const {
        int best = INT_MAX;
        bool found = false;
        for (int id = 1; id <= labels; ++id) {
            if (done[id]) continue;
            for (int t = 0; t < 2; ++t) {
                auto mv = legal_moves(id, t);
                if (mv.empty()) return false;
                int sz = (int)mv.size();
                if (sz < best) {
                    best = sz;
                    out_label = id;
                    out_tip = t;
                    out_moves = std::move(mv);
                    found = true;
                }
            }
        }
        return found;
    }

    int score_move(const Move& m) const {
        // Prefer completing a pair. Otherwise prefer moves into tighter cells.
        if (m.connect) return -1000000;
        int free_neighbors = 0;
        for (int k = 0; k < 4; ++k) {
            int nr = m.to.r + DR[k], nc = m.to.c + DC[k];
            if (!inb(nr, nc)) continue;
            if (owner[nr][nc] == 0) ++free_neighbors;
        }
        return free_neighbors;
    }

    bool dfs() {
        vector<Move> forced;
        if (!forced_propagate(forced)) {
            for (int i = (int)forced.size() - 1; i >= 0; --i) undo_move(forced[i]);
            return false;
        }

        bool all_done = true;
        for (int id = 1; id <= labels; ++id) if (!done[id]) { all_done = false; break; }
        if (all_done) {
            bool ok = final_state_ok();
            if (!ok) {
                for (int i = (int)forced.size() - 1; i >= 0; --i) undo_move(forced[i]);
            }
            return ok;
        }

        int label = -1, tip_idx = -1;
        vector<Move> moves;
        if (!choose_branch(label, tip_idx, moves)) {
            for (int i = (int)forced.size() - 1; i >= 0; --i) undo_move(forced[i]);
            return false;
        }

        stable_sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
            return score_move(a) < score_move(b);
        });

        for (Move m : moves) {
            apply_move(m);
            if (prune_ok() && dfs()) {
                // Keep final state, but do not undo forced moves on the success path.
                return true;
            }
            undo_move(m);
        }

        for (int i = (int)forced.size() - 1; i >= 0; --i) undo_move(forced[i]);
        return false;
    }
};

static void print_boxed_grid(const vector<vector<int>>& rows, const string& title) {
    if (rows.empty()) return;
    int h = (int)rows.size();
    int w = (int)rows[0].size();
    const string indent = "    ";
    const string sep = "+----";

    cout << "+++ " << title << "\n";
    for (int r = 0; r < h; ++r) {
        cout << indent;
        for (int c = 0; c < w; ++c) cout << sep;
        cout << "+\n";

        cout << indent;
        for (int c = 0; c < w; ++c) {
            cout << "|";
            if (rows[r][c] == 0) cout << "    ";
            else cout << " " << setw(2) << rows[r][c] << " ";
        }
        cout << "|\n";
    }
    cout << indent;
    for (int c = 0; c < w; ++c) cout << sep;
    cout << "+\n";
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    try {
        vector<string> lines = read_all_stdin();
        auto puzzle = parse_input(lines);
        NumberlinkSolver solver(puzzle);
        if (!solver.solve()) {
            cout << "IMPOSSIBLE\n";
            return 0;
        }
        auto sol = solver.solution_numbers();
        print_boxed_grid(sol, "The solution");
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
