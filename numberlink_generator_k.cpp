#include <bits/stdc++.h>
using namespace std;

struct Pt {
    int x, y;
    bool operator==(const Pt& o) const noexcept { return x == o.x && y == o.y; }
    bool operator<(const Pt& o) const noexcept { return x != o.x ? x < o.x : y < o.y; }
};

struct PtHash {
    size_t operator()(const Pt& p) const noexcept {
        return (static_cast<uint64_t>(static_cast<uint32_t>(p.x)) << 32) ^ static_cast<uint32_t>(p.y);
    }
};

static int sgn(int x) { return (x == 0 ? 0 : (x < 0 ? -1 : 1)); }

enum Step { T = 0, L = 1, R = 2 };

struct Path {
    vector<int> steps;

    vector<Pt> xys(int dx = 0, int dy = 1) const {
        int x = 0, y = 0;
        vector<Pt> res;
        res.push_back({x, y});
        for (int step : steps) {
            x += dx; y += dy;
            res.push_back({x, y});
            if (step == L) {
                int ndx = -dy, ndy = dx;
                dx = ndx; dy = ndy;
            }
            if (step == R) {
                int ndx = dy, ndy = -dx;
                dx = ndx; dy = ndy;
            } else if (step == T) {
                x += dx; y += dy;
                res.push_back({x, y});
            }
        }
        return res;
    }

    bool test_loop() const {
        auto ps = xys();
        unordered_set<Pt, PtHash> seen;
        for (auto &p : ps) seen.insert(p);
        return (int)ps.size() == (int)seen.size() || ((int)ps.size() == (int)seen.size() + 1 && ps.front() == ps.back());
    }

    int winding() const {
        int cntR = 0, cntL = 0;
        for (int s : steps) {
            if (s == R) ++cntR;
            if (s == L) ++cntL;
        }
        return cntR - cntL;
    }
};

static void unrotate(int &x, int &y, int &dx, int &dy) {
    while (!(dx == 0 && dy == 1)) {
        int nx = -y, ny = x;
        int ndx = -dy, ndy = dx;
        x = nx; y = ny; dx = ndx; dy = ndy;
    }
}

struct Key {
    int x, y, dx, dy;
    bool operator==(const Key& o) const noexcept {
        return x == o.x && y == o.y && dx == o.dx && dy == o.dy;
    }
};

struct KeyHash {
    size_t operator()(const Key& k) const noexcept {
        size_t h = 1469598103934665603ull;
        auto mix = [&](int v) { h ^= (uint32_t)v + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2); };
        mix(k.x); mix(k.y); mix(k.dx); mix(k.dy);
        return h;
    }
};

struct PathInfo {
    vector<int> path;
    int x, y, dx, dy;
};

struct RNG {
    mt19937_64 gen;
    explicit RNG(uint64_t seed = chrono::high_resolution_clock::now().time_since_epoch().count()) : gen(seed) {}
    int randint(int lo, int hi) {
        uniform_int_distribution<int> dist(lo, hi);
        return dist(gen);
    }
    int weighted_step(double wL, double wR, double wT) {
        discrete_distribution<int> dist({wL, wR, wT});
        int z = dist(gen);
        return z == 0 ? L : (z == 1 ? R : T);
    }
};

struct Mitm {
    int lr_price, t_price;
    vector<PathInfo> items;
    unordered_map<Key, vector<vector<int>>, KeyHash> inv;
    RNG *rng = nullptr;

    Mitm(int lr, int tp, RNG* r) : lr_price(lr), t_price(tp), rng(r) {}

    void good_paths(int x, int y, int dx, int dy, int budget,
                    unordered_set<Pt, PtHash>& seen,
                    vector<int>& prefix) {
        if (budget >= 0) {
            items.push_back({prefix, x, y, dx, dy});
            inv[{x, y, dx, dy}].push_back(prefix);
        }
        if (budget <= 0) return;

        seen.insert({x, y});
        int x1 = x + dx, y1 = y + dy;
        if (!seen.count({x1, y1})) {
            prefix.push_back(L);
            good_paths(x1, y1, -dy, dx, budget - lr_price, seen, prefix);
            prefix.pop_back();

            prefix.push_back(R);
            good_paths(x1, y1, dy, -dx, budget - lr_price, seen, prefix);
            prefix.pop_back();

            seen.insert({x1, y1});
            int x2 = x1 + dx, y2 = y1 + dy;
            if (!seen.count({x2, y2})) {
                prefix.push_back(T);
                good_paths(x2, y2, dx, dy, budget - t_price, seen, prefix);
                prefix.pop_back();
            }
            seen.erase({x1, y1});
        }
        seen.erase({x, y});
    }

    void prepare(int budget) {
        unordered_set<Pt, PtHash> seen;
        vector<int> prefix;
        good_paths(0, 0, 0, 1, budget, seen, prefix);
    }

    const vector<vector<int>>* lookup(int dx, int dy, int xn, int yn, int dxn, int dyn) {
        int xt = xn, yt = yn, dxt = dxn, dyt = dyn, ddx = dx, ddy = dy;
        unrotate(xt, yt, ddx, ddy);
        ddx = dx; ddy = dy;
        unrotate(dxt, dyt, ddx, ddy);
        Key k{xt, yt, dxt, dyt};
        auto it = inv.find(k);
        if (it == inv.end()) return nullptr;
        return &it->second;
    }

    Path rand_path2(int xn, int yn, int dxn, int dyn) {
        unordered_set<Pt, PtHash> seen;
        vector<int> path;
        while (true) {
            seen.clear(); path.clear();
            int x = 0, y = 0, dx = 0, dy = 1;
            seen.insert({x, y});
            int lim = 2 * (abs(xn) + abs(yn));
            for (int it = 0; it < lim; ++it) {
                int step = rng->weighted_step(1.0 / lr_price, 1.0 / lr_price, 2.0 / t_price);
                path.push_back(step);
                x += dx; y += dy;
                if (seen.count({x, y})) break;
                seen.insert({x, y});
                if (step == L) {
                    int ndx = -dy, ndy = dx; dx = ndx; dy = ndy;
                }
                if (step == R) {
                    int ndx = dy, ndy = -dx; dx = ndx; dy = ndy;
                } else if (step == T) {
                    x += dx; y += dy;
                    if (seen.count({x, y})) break;
                    seen.insert({x, y});
                }
                if (x == xn && y == yn) return Path{path};
                auto ends = lookup(dx, dy, xn - x, yn - y, dxn, dyn);
                if (ends && !ends->empty()) {
                    vector<int> all = path;
                    const auto& tail = (*ends)[rng->randint(0, (int)ends->size()-1)];
                    all.insert(all.end(), tail.begin(), tail.end());
                    return Path{all};
                }
            }
        }
    }

    Path rand_loop(int clock = 0) {
        while (true) {
            const auto& info = items[rng->randint(0, (int)items.size()-1)];
            auto path2s = lookup(info.dx, info.dy, -info.x, -info.y, 0, 1);
            if (!path2s || path2s->empty()) continue;
            const auto& path2 = (*path2s)[rng->randint(0, (int)path2s->size()-1)];
            vector<int> joined = info.path;
            joined.insert(joined.end(), path2.begin(), path2.end());
            Path p{joined};
            if (clock && p.winding() != clock * 4) continue;
            if (p.test_loop()) return p;
        }
    }
};

struct UnionFind {
    int n;
    vector<int> p, r;
    explicit UnionFind(int n_=0): n(n_), p(n_), r(n_,0) { iota(p.begin(), p.end(), 0); }
    int find(int a){ return p[a]==a ? a : p[a]=find(p[a]); }
    void unite(int a,int b){ a=find(a); b=find(b); if(a==b) return; if(r[a]<r[b]) swap(a,b); p[b]=a; if(r[a]==r[b]) ++r[a]; }
};

struct Grid {
    int w, h;
    vector<char> g;
    Grid(int w_=0, int h_=0): w(w_), h(h_), g(w_*h_, ' ') {}
    bool inb(int x,int y) const { return 0<=x && x<w && 0<=y && y<h; }
    char get(int x,int y) const { return inb(x,y) ? g[y*w+x] : ' '; }
    void set(int x,int y,char c){ if(inb(x,y)) g[y*w+x]=c; }
    bool has(int x,int y) const { return inb(x,y) && get(x,y) != ' '; }
    void clear(){ fill(g.begin(), g.end(), ' '); }

    Grid shrink() const {
        Grid s(w/2, h/2);
        for(int y=0;y<h/2;++y) for(int x=0;x<w/2;++x) s.set(x,y,get(2*x+1,2*y+1));
        return s;
    }

    bool test_path(const Path& path, int x0, int y0, int dx0=0, int dy0=1) const {
        for (auto &p : path.xys(dx0,dy0)) {
            int gx = x0 - p.x + p.y;
            int gy = y0 + p.x + p.y;
            if (!(0 <= gx && gx < w && 0 <= gy && gy < h && !has(gx, gy))) return false;
        }
        return true;
    }

    void draw_path(const Path& path, int x0, int y0, int dx0=0, int dy0=1, bool loop=false) {
        vector<Pt> ps = path.xys(dx0,dy0);
        if (loop) {
            if (!(ps.front() == ps.back())) throw runtime_error("loop path not closed");
            ps.push_back(ps[1]);
        }
        for (int i = 1; i + 1 < (int)ps.size(); ++i) {
            auto [xp, yp] = ps[i-1];
            auto [x, y]   = ps[i];
            auto [xn, yn] = ps[i+1];
            int a = xn - xp, b = yn - yp;
            int c = sgn((x - xp) * (yn - y) - (xn - x) * (y - yp));
            char out = '?';
            if ((a==1&&b==1&&c==1) || (a==-1&&b==-1&&c==-1)) out = '<';
            else if ((a==1&&b==1&&c==-1) || (a==-1&&b==-1&&c==1)) out = '>';
            else if ((a==-1&&b==1&&c==1) || (a==1&&b==-1&&c==-1)) out = 'v';
            else if ((a==-1&&b==1&&c==-1) || (a==1&&b==-1&&c==1)) out = '^';
            else if ((a==0&&b==2&&c==0) || (a==0&&b==-2&&c==0)) out = '\\';
            else if ((a==2&&b==0&&c==0) || (a==-2&&b==0&&c==0)) out = '/';
            else throw runtime_error("unexpected local geometry in draw_path");
            int gx = x0 - x + y;
            int gy = y0 + x + y;
            set(gx, gy, out);
        }
    }

    pair<Grid, UnionFind> make_tubes() const {
        UnionFind uf(w*h);
        Grid tube(w,h);
        for (int x = 0; x < w; ++x) {
            char d = '-';
            for (int y = 0; y < h; ++y) {
                string key; key.push_back(get(x,y)); key.push_back(d);
                vector<pair<int,int>> deltas;
                if (key == "/-") deltas = {{0,1}};
                else if (key == "\\-") deltas = {{1,0},{0,1}};
                else if (key == "/|") deltas = {{1,0}};
                else if (key == " -") deltas = {{1,0}};
                else if (key == " |") deltas = {{0,1}};
                else if (key == "v|") deltas = {{0,1}};
                else if (key == ">|") deltas = {{1,0}};
                else if (key == "v-") deltas = {{0,1}};
                else if (key == ">-") deltas = {{1,0}};
                for (auto [dx,dy] : deltas) {
                    int nx = x + dx, ny = y + dy;
                    if (0 <= nx && nx < w && 0 <= ny && ny < h) uf.unite(y*w + x, ny*w + nx);
                }

                char out = 'x';
                if (key == "/-") out = 'A';
                else if (key == "\\-") out = 'B';
                else if (key == "/|") out = 'C';
                else if (key == "\\|") out = 'D';
                else if (key == " -") out = '-';
                else if (key == " |") out = '|';
                tube.set(x,y,out);

                char cur = get(x,y);
                if (cur == '\\' || cur == '/' || cur == 'v' || cur == '^') d = (d == '-' ? '|' : '-');
            }
        }
        return {tube, uf};
    }

    void clear_path(const Path& path, int x0, int y0) {
        Grid pg(w,h);
        pg.draw_path(path, x0, y0, 0, 1, true);
        auto [tube, uf] = pg.make_tubes();
        (void)uf;
        for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
            if (tube.get(x,y) == '|') set(x,y,' ');
        }
    }
};

static bool is_endpoint_char(char c) {
    return c=='v' || c=='^' || c=='<' || c=='>';
}

static bool has_loops(const Grid& grid, UnionFind uf) {
    unordered_set<int> groups;
    int ends = 0;
    for(int y=0;y<grid.h;++y) for(int x=0;x<grid.w;++x) {
        groups.insert(uf.find(y*grid.w+x));
        if (is_endpoint_char(grid.get(x,y))) ++ends;
    }
    return ends != 2 * (int)groups.size();
}

static bool has_pair(const Grid& tg, UnionFind uf) {
    for(int y=0;y<tg.h;++y) for(int x=0;x<tg.w;++x) {
        const int DX[2] = {1,0}, DY[2] = {0,1};
        for(int k=0;k<2;++k){
            int x1=x+DX[k], y1=y+DY[k];
            if(x1<tg.w && y1<tg.h) {
                if (tg.get(x,y)=='x' && tg.get(x1,y1)=='x' && uf.find(y*tg.w+x)==uf.find(y1*tg.w+x1)) return true;
            }
        }
    }
    return false;
}

static bool has_triple(const Grid& tg, UnionFind uf) {
    const int DX[4] = {1,0,-1,0}, DY[4] = {0,1,0,-1};
    for(int y=0;y<tg.h;++y) for(int x=0;x<tg.w;++x) {
        int r = uf.find(y*tg.w+x);
        int nbs = 0;
        for(int k=0;k<4;++k){
            int x1=x+DX[k], y1=y+DY[k];
            if (0<=x1 && x1<tg.w && 0<=y1 && y1<tg.h && uf.find(y1*tg.w+x1)==r) ++nbs;
        }
        if (nbs >= 3) return true;
    }
    return false;
}

static int count_pairs(const Grid& grid) {
    Grid sg = grid.shrink();
    auto [stg, uf] = sg.make_tubes();
    (void)uf;
    int xs = 0;
    for (char c : stg.g) if (c == 'x') ++xs;
    return xs / 2;
}

static bool test_ready_exact(const Grid& grid, int target_pairs) {
    Grid sg = grid.shrink();
    auto [stg, uf] = sg.make_tubes();
    int xs = 0;
    for(char c : stg.g) if (c == 'x') ++xs;
    int numbers = xs / 2;
    return numbers == target_pairs &&
           !has_loops(sg, uf) && !has_pair(stg, uf) && !has_triple(stg, uf);
}

static Grid make_puzzle(int w, int h, int target_pairs, Mitm& mitm, RNG& rng) {
    static const int LOOP_TRIES = 1000;
    Grid grid(2*w + 1, 2*h + 1);

    while (true) {
        grid.clear();

        Path path = mitm.rand_path2(h, h, 0, -1);
        if (!grid.test_path(path, 0, 0)) continue;
        grid.draw_path(path, 0, 0);
        grid.set(0, 0, '\\');
        grid.set(0, 2*h, '/');

        Path path2 = mitm.rand_path2(h, h, 0, -1);
        if (!grid.test_path(path2, 2*w, 2*h, 0, -1)) continue;
        grid.draw_path(path2, 2*w, 2*h, 0, -1);
        grid.set(2*w, 0, '/');
        grid.set(2*w, 2*h, '\\');

        if (test_ready_exact(grid, target_pairs)) return grid.shrink();

        auto [tg0, uf0] = grid.make_tubes();
        (void)uf0;
        Grid tg = tg0;
        bool too_many_pairs = false;
        for (int tries=0; tries<LOOP_TRIES; ++tries) {
            int x = 2 * rng.randint(0, w-1);
            int y = 2 * rng.randint(0, h-1);
            char orient = tg.get(x, y);
            if (orient != '-' && orient != '|') continue;

            Path lp = mitm.rand_loop(orient == '-' ? 1 : -1);
            if (!grid.test_path(lp, x, y)) continue;

            grid.clear_path(lp, x, y);
            grid.draw_path(lp, x, y, 0, 1, true);
            tg = grid.make_tubes().first;

            int pairs_now = count_pairs(grid);
            if (pairs_now > target_pairs) {
                too_many_pairs = true;
                break;
            }
            if (test_ready_exact(grid, target_pairs)) return grid.shrink();
        }
        if (too_many_pairs) continue;
    }
}

static vector<vector<int>> render_puzzle_numbers(const Grid& grid) {
    auto [tube, uf] = grid.make_tubes();
    (void)tube;
    unordered_map<int, int> lab;
    int ptr = 1;
    vector<vector<int>> rows(grid.h, vector<int>(grid.w, 0));
    for (int y=0;y<grid.h;++y) for (int x=0;x<grid.w;++x) {
        if (is_endpoint_char(grid.get(x,y))) {
            int root = uf.find(y*grid.w+x);
            if (!lab.count(root)) {
                lab[root] = ptr;
                ++ptr;
            }
            rows[y][x] = lab[root];
        }
    }
    return rows;
}

static vector<vector<int>> render_solution_numbers(const Grid& grid) {
    auto [tube, uf] = grid.make_tubes();
    (void)tube;
    unordered_map<int, int> lab;
    int ptr = 1;
    vector<vector<int>> rows(grid.h, vector<int>(grid.w, 0));
    for (int y=0;y<grid.h;++y) for (int x=0;x<grid.w;++x) {
        int root = uf.find(y*grid.w+x);
        if (!lab.count(root)) {
            lab[root] = ptr;
            ++ptr;
        }
        rows[y][x] = lab[root];
    }
    return rows;
}

static void print_boxed_grid(const vector<vector<int>>& rows, const string& title) {
    if (rows.empty()) return;
    const int h = (int)rows.size();
    const int w = (int)rows[0].size();
    const string indent = "    ";
    const string sep = "+----";

    cout << "+++ " << title << "\n";
    for (int y = 0; y < h; ++y) {
        cout << indent;
        for (int x = 0; x < w; ++x) cout << sep;
        cout << "+\n";

        cout << indent;
        for (int x = 0; x < w; ++x) {
            cout << "|";
            if (rows[y][x] == 0) cout << "    ";
            else cout << " " << setw(2) << rows[y][x] << " ";
        }
        cout << "|\n";
    }
    cout << indent;
    for (int x = 0; x < w; ++x) cout << sep;
    cout << "+\n";
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " WIDTH HEIGHT K [--count n] [--seed s] [--solve]\n";
        cerr << "  WIDTH HEIGHT K : generate one puzzle of size WIDTH x HEIGHT with exactly K endpoint pairs\n";
        cerr << "  --count n      : generate n such puzzles (default 1)\n";
        return 1;
    }

    int w = stoi(argv[1]);
    int h = stoi(argv[2]);
    int target_pairs = stoi(argv[3]);
    if (w < 4 || h < 4) {
        cerr << "Please choose width and height at least 4.\n";
        return 1;
    }
    if (target_pairs < 2 || target_pairs > (w * h) / 2) {
        cerr << "K must be between 2 and floor(WIDTH*HEIGHT/2).\n";
        return 1;
    }

    int count = 1;
    bool solve = false;
    uint64_t seed = chrono::high_resolution_clock::now().time_since_epoch().count();

    for (int i = 4; i < argc; ++i) {
        string a = argv[i];
        if (a == "--count" && i + 1 < argc) count = stoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = stoull(argv[++i]);
        else if (a == "--solve") solve = true;
        else {
            cerr << "Unknown argument: " << a << "\n";
            return 1;
        }
    }
    if (count < 1) {
        cerr << "--count must be at least 1.\n";
        return 1;
    }

    RNG rng(seed);
    Mitm mitm(2, 1, &rng);
    mitm.prepare(min(20, max(h, 6)));

    for (int it = 0; it < count; ++it) {
        Grid puzzle = make_puzzle(w, h, target_pairs, mitm, rng);
        auto rows = render_puzzle_numbers(puzzle);
        print_boxed_grid(rows, "The puzzle");
        cout << "\n";
        if (solve) {
            auto sol = render_solution_numbers(puzzle);
            print_boxed_grid(sol, "The solution");
            cout << "\n";
        }
    }
    return 0;
}
