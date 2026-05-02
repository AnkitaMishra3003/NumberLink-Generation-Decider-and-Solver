#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/time.h>
#include <utility>
#include <vector>
using namespace std;

/*
    Numberlink Difficulty Designer + Detailed Analyzer
    --------------------------------------------------
    Standalone C++17 tool.

    Main modes:
      1) analyze : read a puzzle from stdin and print a detailed difficulty
   report 2) rate    : read a puzzle from stdin and print only the final integer
   difficulty score

    Input formats supported:
      - Plain numeric grid:
            H W              (or W H; the parser auto-detects by row count)
            0 1 0 0 1
            2 0 0 0 0
            ...
        Dots are accepted as zero.

      - Boxed grids printed by numberlink_generator_k.cpp:
            +++ The puzzle
            +----+----+
            |  1 |    |
            ...

    Compile:
      g++ -std=c++17 -O2 -pipe numberlink_difficulty_designer_analysis_only.cpp
   -o nldiff

    Examples:
      ./nldiff analyze --timeout-ms 5000 < puzzle.txt
      ./nldiff analyze --timeout-ms 5000 --no-pairs < puzzle.txt
      ./nldiff rate < puzzle.txt
*/

struct Pos {
  int r = -1, c = -1;
  bool operator==(const Pos &o) const { return r == o.r && c == o.c; }
  bool operator!=(const Pos &o) const { return !(*this == o); }
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

static string trim(const string &s) {
  size_t i = 0, j = s.size();
  while (i < s.size() && isspace((unsigned char)s[i]))
    ++i;
  while (j > i && isspace((unsigned char)s[j - 1]))
    --j;
  return s.substr(i, j - i);
}

static double clamp01(double x) { return max(0.0, min(1.0, x)); }
static double clamp100(double x) { return max(0.0, min(100.0, x)); }
static int manh(Pos a, Pos b) { return abs(a.r - b.r) + abs(a.c - b.c); }

static vector<string> read_all_stdin() {
  vector<string> lines;
  string s;
  while (getline(cin, s))
    lines.push_back(s);
  return lines;
}

static bool parse_two_ints(const string &s, int &a, int &b) {
  istringstream iss(s);
  string x, y, extra;
  if (!(iss >> x >> y) || (iss >> extra))
    return false;
  try {
    size_t p1 = 0, p2 = 0;
    a = stoi(x, &p1);
    b = stoi(y, &p2);
    return p1 == x.size() && p2 == y.size();
  } catch (...) {
    return false;
  }
}

static vector<int> parse_numeric_row(const string &raw) {
  string line = trim(raw);
  vector<int> row;
  if (line.empty() || line[0] == '#')
    return row;

  // Token based format: 0 1 . 2
  if (line.find(' ') != string::npos || line.find('\t') != string::npos) {
    istringstream iss(line);
    string tok;
    while (iss >> tok) {
      if (tok == "." || tok == "_")
        row.push_back(0);
      else
        row.push_back(stoi(tok));
    }
    return row;
  }

  // Compact digit format: 01020.  Only safe for labels 0-9.
  bool compact = true;
  for (char ch : line) {
    if (!(isdigit((unsigned char)ch) || ch == '.')) {
      compact = false;
      break;
    }
  }
  if (compact) {
    for (char ch : line)
      row.push_back(ch == '.' ? 0 : ch - '0');
  }
  return row;
}

static vector<vector<int>> parse_boxed(const vector<string> &lines) {
  vector<vector<int>> rows;
  int width = -1;
  bool started = false;

  for (const string &raw : lines) {
    string line = trim(raw);
    if (line.rfind("+++", 0) == 0) {
      if (started && !rows.empty())
        break;
      continue;
    }
    if (line.empty()) {
      if (started && !rows.empty())
        break;
      continue;
    }
    if (line.find('|') == string::npos)
      continue;
    started = true;

    vector<int> row;
    size_t first = raw.find('|');
    while (first != string::npos) {
      size_t second = raw.find('|', first + 1);
      if (second == string::npos)
        break;
      string t = trim(raw.substr(first + 1, second - first - 1));
      row.push_back(t.empty() ? 0 : stoi(t));
      first = second;
    }
    if (!row.empty()) {
      if (width == -1)
        width = (int)row.size();
      if ((int)row.size() != width)
        throw runtime_error("Inconsistent boxed grid width.");
      rows.push_back(row);
    }
  }
  if (rows.empty())
    throw runtime_error("Could not parse boxed puzzle from input.");
  return rows;
}

static vector<vector<int>> parse_plain(const vector<string> &lines) {
  int idx = 0;
  while (idx < (int)lines.size() && trim(lines[idx]).empty())
    ++idx;
  if (idx == (int)lines.size())
    throw runtime_error("Empty input.");

  int a = 0, b = 0;
  if (!parse_two_ints(trim(lines[idx]), a, b)) {
    throw runtime_error(
        "Expected first line as 'H W'/'W H', or a boxed puzzle.");
  }
  ++idx;

  vector<vector<int>> raw_rows;
  for (; idx < (int)lines.size(); ++idx) {
    string t = trim(lines[idx]);
    if (t.empty())
      break;
    if (t[0] == '#')
      continue;
    vector<int> row = parse_numeric_row(t);
    if (!row.empty())
      raw_rows.push_back(row);
  }
  if (raw_rows.empty())
    throw runtime_error("No grid rows found after dimensions.");

  auto all_width = [&](int w) {
    for (auto &row : raw_rows)
      if ((int)row.size() != w)
        return false;
    return true;
  };

  // Prefer the common BTP format: H W followed by H rows of W entries.
  if ((int)raw_rows.size() == a && all_width(b))
    return raw_rows;
  // Also accept generator style: W H followed by H rows of W entries.
  if ((int)raw_rows.size() == b && all_width(a))
    return raw_rows;

  ostringstream oss;
  oss << "Grid size mismatch. Header was " << a << " " << b << ", but found "
      << raw_rows.size() << " rows";
  if (!raw_rows.empty())
    oss << " of width " << raw_rows[0].size();
  oss << ".";
  throw runtime_error(oss.str());
}

static vector<vector<int>> parse_input(const vector<string> &lines) {
  for (const string &s : lines) {
    if (s.find('|') != string::npos)
      return parse_boxed(lines);
  }
  return parse_plain(lines);
}

static void print_plain_grid(const vector<vector<int>> &rows,
                             ostream &out = cout) {
  int h = (int)rows.size();
  int w = h ? (int)rows[0].size() : 0;
  out << h << " " << w << "\n";
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      if (c)
        out << ' ';
      out << rows[r][c];
    }
    out << "\n";
  }
}

static void print_boxed_grid(const vector<vector<int>> &rows,
                             const string &title, ostream &out = cout) {
  if (rows.empty())
    return;
  int h = (int)rows.size();
  int w = (int)rows[0].size();
  const string indent = "    ";
  const string sep = "+----";

  out << "+++ " << title << "\n";
  for (int r = 0; r < h; ++r) {
    out << indent;
    for (int c = 0; c < w; ++c)
      out << sep;
    out << "+\n";

    out << indent;
    for (int c = 0; c < w; ++c) {
      out << "|";
      if (rows[r][c] == 0)
        out << "    ";
      else
        out << " " << setw(2) << rows[r][c] << " ";
    }
    out << "|\n";
  }
  out << indent;
  for (int c = 0; c < w; ++c)
    out << sep;
  out << "+\n";
}

struct SearchStats {
  long long dfs_calls = 0;
  long long branch_nodes = 0;
  long long branch_options_sum = 0;
  long long moves_tried = 0;
  long long forced_moves = 0;
  long long backtracks = 0;
  long long dead_ends = 0;
  long long solutions_found = 0;

  long long prune_basic_degree = 0;
  long long prune_stuck_tip = 0;
  long long prune_reachability = 0;
  long long prune_empty_degree = 0;
  long long prune_component = 0;
  long long prune_timeout = 0;

  int max_depth = 0;
  int max_forced_chain = 0;
  int max_filled_cells = 0;
  bool timed_out = false;
  double elapsed_ms = 0.0;

  long long total_prunes() const {
    return prune_basic_degree + prune_stuck_tip + prune_reachability +
           prune_empty_degree + prune_component + prune_timeout;
  }
  double avg_branch_options() const {
    return branch_nodes ? (double)branch_options_sum / (double)branch_nodes
                        : 0.0;
  }
};

static double wall_time_ms() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

static uint64_t default_seed() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  uint64_t x =
      ((uint64_t)tv.tv_sec << 32) ^ (uint64_t)tv.tv_usec ^ (uint64_t)clock();
  x ^= 0x9e3779b97f4a7c15ULL;
  return x ? x : 88172645463393265ULL;
}

struct Deadline {
  double start_ms = wall_time_ms();
  long long timeout_ms = 5000;

  explicit Deadline(long long ms = 5000)
      : start_ms(wall_time_ms()), timeout_ms(ms) {}

  void reset() { start_ms = wall_time_ms(); }

  bool expired(long long tick = 0) const {
    if (timeout_ms <= 0)
      return false;
    if ((tick & 1023LL) != 0)
      return false; // avoid checking clock too often
    return wall_time_ms() - start_ms > (double)timeout_ms;
  }

  double elapsed() const { return wall_time_ms() - start_ms; }
};

struct FastRNG {
  uint64_t state;
  explicit FastRNG(uint64_t seed = default_seed())
      : state(seed ? seed : 88172645463393265ULL) {}

  uint64_t next() {
    uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }

  int randint(int lo, int hi) {
    if (hi < lo)
      return lo;
    uint64_t span = (uint64_t)(hi - lo + 1);
    return lo + (int)(next() % span);
  }
};

enum class PruneReason {
  NONE,
  BASIC_DEGREE,
  STUCK_TIP,
  REACHABILITY,
  EMPTY_DEGREE,
  COMPONENT,
  TIMEOUT
};

static string prune_name(PruneReason r) {
  switch (r) {
  case PruneReason::BASIC_DEGREE:
    return "degree consistency";
  case PruneReason::STUCK_TIP:
    return "stuck path tip";
  case PruneReason::REACHABILITY:
    return "endpoint reachability";
  case PruneReason::EMPTY_DEGREE:
    return "empty-cell future degree";
  case PruneReason::COMPONENT:
    return "empty-region component";
  case PruneReason::TIMEOUT:
    return "timeout";
  default:
    return "none";
  }
}

class InstrumentedSolver {
public:
  explicit InstrumentedSolver(vector<vector<int>> puzzle,
                              long long timeout_ms = 5000)
      : clue_raw(std::move(puzzle)), h((int)clue_raw.size()),
        w((int)clue_raw[0].size()), deadline(timeout_ms) {
    build_labels();
    owner.assign(h, vector<int>(w, 0));
    deg.assign(h, vector<int>(w, 0));
    is_source.assign(h, vector<char>(w, 0));
    src.assign(labels + 1, {Pos(), Pos()});
    tip.assign(labels + 1, {Pos(), Pos()});
    done.assign(labels + 1, 0);

    vector<int> seen(labels + 1, 0);
    filled_cells = 0;
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        int x = clue[r][c];
        if (!x)
          continue;
        owner[r][c] = x;
        is_source[r][c] = 1;
        ++filled_cells;
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
    stats.max_filled_cells = filled_cells;
  }

  bool solve_one() {
    deadline.reset();
    bool ok = dfs_solve(0);
    stats.elapsed_ms = deadline.elapsed();
    if (ok)
      solved_grid = solution_numbers();
    return ok;
  }

  int count_solutions_up_to(int limit) {
    deadline.reset();
    int count = 0;
    dfs_count(0, limit, count);
    stats.solutions_found = count;
    stats.elapsed_ms = deadline.elapsed();
    return count;
  }

  const SearchStats &get_stats() const { return stats; }
  int label_count() const { return labels; }
  bool timed_out() const { return stats.timed_out; }
  vector<vector<int>> solved() const { return solved_grid; }

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
  vector<vector<int>> solved_grid;
  int h = 0, w = 0;
  int labels = 0;
  int filled_cells = 0;
  vector<int> original_label;

  vector<vector<int>> owner;
  vector<vector<int>> deg;
  vector<vector<char>> is_source;
  vector<array<Pos, 2>> src;
  vector<array<Pos, 2>> tip;
  vector<char> done;

  SearchStats stats;
  Deadline deadline;

  const int DR[4] = {-1, 1, 0, 0};
  const int DC[4] = {0, 0, -1, 1};

  bool inb(int r, int c) const { return 0 <= r && r < h && 0 <= c && c < w; }

  void build_labels() {
    if (clue_raw.empty() || clue_raw[0].empty())
      throw runtime_error("Empty puzzle grid.");
    int width = (int)clue_raw[0].size();
    for (const auto &row : clue_raw) {
      if ((int)row.size() != width)
        throw runtime_error("Non-rectangular puzzle grid.");
    }

    map<int, int> mp;
    clue = clue_raw;
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        if (clue[r][c] < 0)
          throw runtime_error("Negative labels are not allowed.");
        if (clue[r][c] && !mp.count(clue[r][c]))
          mp[clue[r][c]] = (int)mp.size() + 1;
      }
    }
    labels = (int)mp.size();
    original_label.assign(labels + 1, 0);
    vector<int> cnt(labels + 1, 0);
    for (auto &[orig, comp] : mp)
      original_label[comp] = orig;
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        if (clue[r][c])
          clue[r][c] = mp[clue[r][c]];
        if (clue[r][c])
          ++cnt[clue[r][c]];
      }
    }
    for (int id = 1; id <= labels; ++id) {
      if (cnt[id] != 2) {
        ostringstream oss;
        oss << "Each number must appear exactly twice. Label "
            << original_label[id] << " appears " << cnt[id] << " times.";
        throw runtime_error(oss.str());
      }
    }
  }

  void add_prune(PruneReason r) {
    switch (r) {
    case PruneReason::BASIC_DEGREE:
      ++stats.prune_basic_degree;
      break;
    case PruneReason::STUCK_TIP:
      ++stats.prune_stuck_tip;
      break;
    case PruneReason::REACHABILITY:
      ++stats.prune_reachability;
      break;
    case PruneReason::EMPTY_DEGREE:
      ++stats.prune_empty_degree;
      break;
    case PruneReason::COMPONENT:
      ++stats.prune_component;
      break;
    case PruneReason::TIMEOUT:
      ++stats.prune_timeout;
      stats.timed_out = true;
      break;
    default:
      break;
    }
  }

  bool is_current_tip(int r, int c) const {
    for (int id = 1; id <= labels; ++id) {
      if (done[id])
        continue;
      if (tip[id][0].r == r && tip[id][0].c == c)
        return true;
      if (tip[id][1].r == r && tip[id][1].c == c)
        return true;
    }
    return false;
  }

  vector<Move> legal_moves(int label, int tip_idx) const {
    vector<Move> mv;
    if (done[label])
      return mv;
    Pos u = tip[label][tip_idx];
    Pos other = tip[label][1 - tip_idx];
    int max_deg_u = is_source[u.r][u.c] ? 1 : 2;
    if (deg[u.r][u.c] >= max_deg_u)
      return mv;

    for (int k = 0; k < 4; ++k) {
      int nr = u.r + DR[k], nc = u.c + DC[k];
      if (!inb(nr, nc))
        continue;
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

  void apply_move(Move &m, bool forced) {
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
      ++filled_cells;
      stats.max_filled_cells = max(stats.max_filled_cells, filled_cells);
    }
    if (forced)
      ++stats.forced_moves;
    else
      ++stats.moves_tried;
  }

  void undo_move(const Move &m) {
    deg[m.from.r][m.from.c] = m.old_deg_from;
    deg[m.to.r][m.to.c] = m.old_deg_to;
    done[m.label] = m.old_done;
    if (!m.connect) {
      owner[m.to.r][m.to.c] = 0;
      --filled_cells;
    }
    tip[m.label][m.tip_idx] = m.old_tip;
  }

  bool final_state_ok() const {
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        if (owner[r][c] == 0)
          return false;
        if (is_source[r][c]) {
          if (deg[r][c] != 1)
            return false;
        } else {
          if (deg[r][c] != 2)
            return false;
        }
      }
    }
    return true;
  }

  bool basic_degree_ok() const {
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        if (owner[r][c] == 0)
          continue;
        int lim = is_source[r][c] ? 1 : 2;
        if (deg[r][c] > lim)
          return false;
        if (!is_source[r][c] && deg[r][c] < 2 && !is_current_tip(r, c))
          return false;

        int same_adj = 0;
        for (int k = 0; k < 4; ++k) {
          int nr = r + DR[k], nc = c + DC[k];
          if (inb(nr, nc) && owner[nr][nc] == owner[r][c])
            ++same_adj;
        }
        int pending = 0;
        int label = owner[r][c];
        if (!done[label]) {
          Pos a = tip[label][0], b = tip[label][1];
          if ((a.r == r && a.c == c) || (b.r == r && b.c == c)) {
            if (abs(a.r - b.r) + abs(a.c - b.c) == 1)
              pending = 1;
          }
        }
        if (same_adj != deg[r][c] + pending)
          return false;
      }
    }
    return true;
  }

  bool unfinished_pairs_reachable() const {
    vector<vector<int>> vis(h, vector<int>(w, 0));
    for (int id = 1; id <= labels; ++id) {
      if (done[id])
        continue;
      Pos s = tip[id][0], t = tip[id][1];
      for (auto &row : vis)
        fill(row.begin(), row.end(), 0);
      queue<Pos> q;
      q.push(s);
      vis[s.r][s.c] = 1;
      bool ok = false;
      while (!q.empty()) {
        Pos u = q.front();
        q.pop();
        if (u == t) {
          ok = true;
          break;
        }
        for (int k = 0; k < 4; ++k) {
          int nr = u.r + DR[k], nc = u.c + DC[k];
          if (!inb(nr, nc) || vis[nr][nc])
            continue;
          if ((nr == t.r && nc == t.c) || owner[nr][nc] == 0) {
            vis[nr][nc] = 1;
            q.push({nr, nc});
          }
        }
      }
      if (!ok)
        return false;
    }
    return true;
  }

  bool empty_cell_future_degree_ok() const {
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        if (owner[r][c] != 0)
          continue;
        int ports = 0;
        for (int k = 0; k < 4; ++k) {
          int nr = r + DR[k], nc = c + DC[k];
          if (!inb(nr, nc))
            continue;
          if (owner[nr][nc] == 0 || is_current_tip(nr, nc))
            ++ports;
        }
        if (ports < 2)
          return false;
      }
    }
    return true;
  }

  bool empty_components_ok() const {
    vector<vector<int>> vis(h, vector<int>(w, 0));
    for (int sr = 0; sr < h; ++sr) {
      for (int sc = 0; sc < w; ++sc) {
        if (owner[sr][sc] != 0 || vis[sr][sc])
          continue;
        queue<Pos> q;
        q.push({sr, sc});
        vis[sr][sc] = 1;
        set<pair<int, int>> adj_tips;
        int cells = 0;
        while (!q.empty()) {
          Pos u = q.front();
          q.pop();
          ++cells;
          for (int k = 0; k < 4; ++k) {
            int nr = u.r + DR[k], nc = u.c + DC[k];
            if (!inb(nr, nc))
              continue;
            if (owner[nr][nc] == 0 && !vis[nr][nc]) {
              vis[nr][nc] = 1;
              q.push({nr, nc});
            } else if (owner[nr][nc] != 0 && is_current_tip(nr, nc)) {
              adj_tips.insert({nr, nc});
            }
          }
        }
        (void)cells;
        if ((int)adj_tips.size() < 2)
          return false;
      }
    }
    return true;
  }

  bool stuck_tip_ok() const {
    for (int id = 1; id <= labels; ++id) {
      if (done[id])
        continue;
      if (legal_moves(id, 0).empty())
        return false;
      if (legal_moves(id, 1).empty())
        return false;
    }
    return true;
  }

  bool prune_ok(PruneReason &reason) const {
    reason = PruneReason::NONE;
    if (!basic_degree_ok()) {
      reason = PruneReason::BASIC_DEGREE;
      return false;
    }
    if (!stuck_tip_ok()) {
      reason = PruneReason::STUCK_TIP;
      return false;
    }
    if (!unfinished_pairs_reachable()) {
      reason = PruneReason::REACHABILITY;
      return false;
    }
    if (!empty_cell_future_degree_ok()) {
      reason = PruneReason::EMPTY_DEGREE;
      return false;
    }
    if (!empty_components_ok()) {
      reason = PruneReason::COMPONENT;
      return false;
    }
    return true;
  }

  bool forced_propagate(vector<Move> &history) {
    while (true) {
      if (deadline.expired(stats.dfs_calls)) {
        add_prune(PruneReason::TIMEOUT);
        return false;
      }
      bool progressed = false;
      for (int id = 1; id <= labels; ++id) {
        if (done[id])
          continue;
        for (int t = 0; t < 2; ++t) {
          auto mv = legal_moves(id, t);
          if (mv.empty()) {
            ++stats.dead_ends;
            add_prune(PruneReason::STUCK_TIP);
            return false;
          }
          if ((int)mv.size() == 1) {
            Move m = mv[0];
            apply_move(m, true);
            history.push_back(m);
            stats.max_forced_chain =
                max(stats.max_forced_chain, (int)history.size());
            PruneReason why;
            if (!prune_ok(why)) {
              add_prune(why);
              return false;
            }
            progressed = true;
            goto next_round;
          }
        }
      }
      if (!progressed)
        break;
    next_round:;
    }
    return true;
  }

  bool choose_branch(int &out_label, int &out_tip,
                     vector<Move> &out_moves) const {
    int best = INT_MAX;
    bool found = false;
    for (int id = 1; id <= labels; ++id) {
      if (done[id])
        continue;
      for (int t = 0; t < 2; ++t) {
        auto mv = legal_moves(id, t);
        if (mv.empty())
          return false;
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

  int move_score(const Move &m) const {
    // Prefer completing a pair. Otherwise prefer entering cells with fewer
    // onward freedoms, then prefer moving closer to the opposite tip.
    if (m.connect)
      return -100000000;
    int free_neighbors = 0;
    for (int k = 0; k < 4; ++k) {
      int nr = m.to.r + DR[k], nc = m.to.c + DC[k];
      if (!inb(nr, nc))
        continue;
      if (owner[nr][nc] == 0)
        ++free_neighbors;
    }
    Pos other = tip[m.label][1 - m.tip_idx];
    return 100 * free_neighbors + manh(m.to, other);
  }

  bool all_pairs_done() const {
    for (int id = 1; id <= labels; ++id)
      if (!done[id])
        return false;
    return true;
  }

  bool dfs_solve(int depth) {
    ++stats.dfs_calls;
    stats.max_depth = max(stats.max_depth, depth);
    if (deadline.expired(stats.dfs_calls)) {
      add_prune(PruneReason::TIMEOUT);
      return false;
    }

    vector<Move> forced;
    if (!forced_propagate(forced)) {
      for (int i = (int)forced.size() - 1; i >= 0; --i)
        undo_move(forced[i]);
      return false;
    }

    if (all_pairs_done()) {
      bool ok = final_state_ok();
      if (!ok) {
        ++stats.dead_ends;
        for (int i = (int)forced.size() - 1; i >= 0; --i)
          undo_move(forced[i]);
      } else {
        stats.solutions_found = 1;
      }
      return ok;
    }

    int label = -1, tip_idx = -1;
    vector<Move> moves;
    if (!choose_branch(label, tip_idx, moves)) {
      ++stats.dead_ends;
      for (int i = (int)forced.size() - 1; i >= 0; --i)
        undo_move(forced[i]);
      return false;
    }

    ++stats.branch_nodes;
    stats.branch_options_sum += (long long)moves.size();

    stable_sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
      return move_score(a) < move_score(b);
    });

    for (Move m : moves) {
      apply_move(m, false);
      PruneReason why;
      if (prune_ok(why) && dfs_solve(depth + 1)) {
        // Keep final state. Do not undo success path.
        return true;
      }
      if (why != PruneReason::NONE)
        add_prune(why);
      undo_move(m);
      ++stats.backtracks;
    }

    for (int i = (int)forced.size() - 1; i >= 0; --i)
      undo_move(forced[i]);
    return false;
  }

  void dfs_count(int depth, int limit, int &count) {
    if (count >= limit)
      return;
    ++stats.dfs_calls;
    stats.max_depth = max(stats.max_depth, depth);
    if (deadline.expired(stats.dfs_calls)) {
      add_prune(PruneReason::TIMEOUT);
      return;
    }

    vector<Move> forced;
    if (!forced_propagate(forced)) {
      for (int i = (int)forced.size() - 1; i >= 0; --i)
        undo_move(forced[i]);
      return;
    }

    if (all_pairs_done()) {
      if (final_state_ok()) {
        ++count;
      } else {
        ++stats.dead_ends;
      }
      for (int i = (int)forced.size() - 1; i >= 0; --i)
        undo_move(forced[i]);
      return;
    }

    int label = -1, tip_idx = -1;
    vector<Move> moves;
    if (!choose_branch(label, tip_idx, moves)) {
      ++stats.dead_ends;
      for (int i = (int)forced.size() - 1; i >= 0; --i)
        undo_move(forced[i]);
      return;
    }

    ++stats.branch_nodes;
    stats.branch_options_sum += (long long)moves.size();

    stable_sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
      return move_score(a) < move_score(b);
    });

    for (Move m : moves) {
      if (count >= limit || stats.timed_out)
        break;
      apply_move(m, false);
      PruneReason why;
      if (prune_ok(why))
        dfs_count(depth + 1, limit, count);
      else
        add_prune(why);
      undo_move(m);
      ++stats.backtracks;
    }

    for (int i = (int)forced.size() - 1; i >= 0; --i)
      undo_move(forced[i]);
  }
};

struct PairFeature {
  int original_label = 0;
  Pos a, b;
  int manhattan = 0;
  int free_a = 0;
  int free_b = 0;
  bool same_checker_color = false;
};

struct StaticFeatures {
  int h = 0, w = 0, cells = 0, pairs = 0, endpoints = 0;
  double endpoint_density = 0.0;
  double avg_manhattan = 0.0;
  double norm_distance = 0.0;
  double avg_endpoint_free = 0.0;
  double endpoint_freedom = 0.0;
  double bottleneck = 0.0;
  double sparsity_pressure = 0.0;
  double interaction_pressure = 0.0;
  double clustering_pressure = 0.0;
  double parity_pressure = 0.0;
  double manhattan_cv = 0.0;

  double board_score = 0.0;
  double distance_score = 0.0;
  double freedom_score = 0.0;
  double bottleneck_score = 0.0;
  double sparsity_score = 0.0;
  double interaction_score = 0.0;
  double clustering_score = 0.0;
  double parity_score = 0.0;
  double variance_score = 0.0;
  double static_score = 0.0;

  vector<PairFeature> pair_features;
};

static StaticFeatures
compute_static_features(const vector<vector<int>> &puzzle) {
  StaticFeatures f;
  f.h = (int)puzzle.size();
  f.w = f.h ? (int)puzzle[0].size() : 0;
  f.cells = f.h * f.w;

  map<int, vector<Pos>> mp;
  for (int r = 0; r < f.h; ++r) {
    for (int c = 0; c < f.w; ++c) {
      if (puzzle[r][c] > 0)
        mp[puzzle[r][c]].push_back({r, c});
    }
  }
  f.pairs = (int)mp.size();
  f.endpoints = 2 * f.pairs;
  f.endpoint_density = f.cells ? (double)f.endpoints / (double)f.cells : 0.0;

  const int DR[4] = {-1, 1, 0, 0};
  const int DC[4] = {0, 0, -1, 1};
  auto inb = [&](int r, int c) {
    return 0 <= r && r < f.h && 0 <= c && c < f.w;
  };
  auto freeN = [&](Pos u) {
    int cnt = 0;
    for (int k = 0; k < 4; ++k) {
      int nr = u.r + DR[k], nc = u.c + DC[k];
      if (inb(nr, nc) && puzzle[nr][nc] == 0)
        ++cnt;
    }
    return cnt;
  };

  double sum_dist = 0.0;
  double sum_free = 0.0;
  int same_color_pairs = 0;
  vector<int> distances;

  for (auto &[lab, ps] : mp) {
    if (ps.size() != 2)
      continue;
    PairFeature pf;
    pf.original_label = lab;
    pf.a = ps[0];
    pf.b = ps[1];
    pf.manhattan = manh(pf.a, pf.b);
    pf.free_a = freeN(pf.a);
    pf.free_b = freeN(pf.b);
    pf.same_checker_color = ((pf.a.r + pf.a.c) & 1) == ((pf.b.r + pf.b.c) & 1);
    f.pair_features.push_back(pf);
    sum_dist += pf.manhattan;
    distances.push_back(pf.manhattan);
    sum_free += pf.free_a + pf.free_b;
    if (pf.same_checker_color)
      ++same_color_pairs;
  }

  if (f.pairs > 0) {
    f.avg_manhattan = sum_dist / f.pairs;
    int max_manh = max(1, f.h + f.w - 2);
    f.norm_distance = clamp01(f.avg_manhattan / max_manh);
    f.avg_endpoint_free = sum_free / (2.0 * f.pairs);
    f.endpoint_freedom = clamp01(f.avg_endpoint_free / 4.0);
    f.bottleneck = 1.0 - f.endpoint_freedom;

    // Lower endpoint density usually implies longer, more global paths.
    // At density >= 0.45, sparsity pressure is nearly gone.
    f.sparsity_pressure = clamp01(1.0 - f.endpoint_density / 0.45);

    // Pair interaction: bounding boxes of pairs overlap. This is a cheap proxy
    // for interweaving.
    long long overlaps = 0;
    long long total_pair_pairs = 1LL * f.pairs * (f.pairs - 1) / 2;
    for (int i = 0; i < (int)f.pair_features.size(); ++i) {
      auto A = f.pair_features[i];
      int ar1 = min(A.a.r, A.b.r), ar2 = max(A.a.r, A.b.r);
      int ac1 = min(A.a.c, A.b.c), ac2 = max(A.a.c, A.b.c);
      for (int j = i + 1; j < (int)f.pair_features.size(); ++j) {
        auto B = f.pair_features[j];
        int br1 = min(B.a.r, B.b.r), br2 = max(B.a.r, B.b.r);
        int bc1 = min(B.a.c, B.b.c), bc2 = max(B.a.c, B.b.c);
        bool row_overlap = max(ar1, br1) <= min(ar2, br2);
        bool col_overlap = max(ac1, bc1) <= min(ac2, bc2);
        if (row_overlap && col_overlap)
          ++overlaps;
      }
    }
    f.interaction_pressure =
        total_pair_pairs ? (double)overlaps / (double)total_pair_pairs : 0.0;

    // Clustering/confusion: if endpoints of different labels are very close,
    // humans and solvers see more local ambiguity.
    int min_cross = INT_MAX;
    vector<pair<Pos, int>> endpoints;
    for (auto &pf : f.pair_features) {
      endpoints.push_back({pf.a, pf.original_label});
      endpoints.push_back({pf.b, pf.original_label});
    }
    for (int i = 0; i < (int)endpoints.size(); ++i) {
      for (int j = i + 1; j < (int)endpoints.size(); ++j) {
        if (endpoints[i].second == endpoints[j].second)
          continue;
        min_cross =
            min(min_cross, manh(endpoints[i].first, endpoints[j].first));
      }
    }
    max_manh = max(1, f.h + f.w - 2);
    f.clustering_pressure = (min_cross == INT_MAX)
                                ? 0.0
                                : clamp01(1.0 - (double)min_cross / max_manh);

    // Parity pressure: same-color endpoints imply odd cell count paths. Mixed
    // parity patterns are not invalid, but skew often makes exact full-cover
    // routing tighter.
    double same_ratio = (double)same_color_pairs / f.pairs;
    f.parity_pressure = fabs(same_ratio - 0.5) * 2.0;

    double mean = f.avg_manhattan;
    double var = 0.0;
    for (int d : distances)
      var += (d - mean) * (d - mean);
    var /= max(1, (int)distances.size());
    f.manhattan_cv = mean > 0 ? sqrt(var) / mean : 0.0;
  }

  f.board_score = clamp100(100.0 * log2((double)max(2, f.cells)) / log2(400.0));
  f.distance_score = 100.0 * f.norm_distance;
  f.freedom_score = 100.0 * f.endpoint_freedom;
  f.bottleneck_score = 100.0 * f.bottleneck;
  f.sparsity_score = 100.0 * f.sparsity_pressure;
  f.interaction_score = 100.0 * f.interaction_pressure;
  f.clustering_score = 100.0 * f.clustering_pressure;
  f.parity_score = 100.0 * f.parity_pressure;
  f.variance_score = clamp100(100.0 * min(1.0, f.manhattan_cv));

  // Static difficulty is deliberately not just board size. Numberlink hardness
  // usually comes from long paths + sparse clues + interacting boxes +
  // misleading local freedom.
  f.static_score = 0.12 * f.board_score + 0.18 * f.distance_score +
                   0.14 * f.sparsity_score + 0.18 * f.interaction_score +
                   0.11 * f.freedom_score + 0.08 * f.bottleneck_score +
                   0.08 * f.clustering_score + 0.05 * f.parity_score +
                   0.06 * f.variance_score;
  f.static_score = clamp100(f.static_score);
  return f;
}

struct DifficultyAnalysis {
  StaticFeatures sf;
  SearchStats solve_stats;
  SearchStats count_stats;
  bool solved = false;
  bool count_timed_out = false;
  int solutions_up_to_2 = 0;
  double dynamic_score = 0.0;
  double final_score = 0.0;
  string band;
  string quality;
  vector<vector<int>> solution;
};

static string score_band(double score) {
  if (score < 20)
    return "Trivial";
  if (score < 40)
    return "Easy";
  if (score < 60)
    return "Medium";
  if (score < 80)
    return "Hard";
  return "Expert";
}

static double compute_dynamic_score(const SearchStats &s, int cells) {
  double node_score =
      clamp100(20.0 * log10((double)s.dfs_calls + 1.0)); // 1e5 calls -> ~100
  double backtrack_score = clamp100(20.0 * log10((double)s.backtracks + 1.0));
  double branch_score =
      clamp100(100.0 * min(1.0, s.avg_branch_options() / 4.0));
  double force_ratio =
      (s.forced_moves + s.moves_tried)
          ? (double)s.forced_moves / (double)(s.forced_moves + s.moves_tried)
          : 1.0;
  double forced_scarcity_score =
      100.0 * (1.0 - force_ratio); // fewer forced moves -> harder
  double depth_score =
      cells ? 100.0 * (double)s.max_filled_cells / (double)cells : 0.0;
  double prune_score = clamp100(20.0 * log10((double)s.total_prunes() + 1.0));

  double dyn = 0.34 * node_score + 0.20 * backtrack_score +
               0.14 * branch_score + 0.12 * forced_scarcity_score +
               0.10 * depth_score + 0.10 * prune_score;
  if (s.timed_out)
    dyn = max(dyn, 92.0);
  return clamp100(dyn);
}

static DifficultyAnalysis analyze_puzzle(const vector<vector<int>> &puzzle,
                                         long long timeout_ms,
                                         bool need_solution = false) {
  DifficultyAnalysis a;
  a.sf = compute_static_features(puzzle);

  // First solve: gives one realistic effort trace.
  InstrumentedSolver solver(puzzle, timeout_ms);
  a.solved = solver.solve_one();
  a.solve_stats = solver.get_stats();
  if (need_solution && a.solved)
    a.solution = solver.solved();

  // Then count up to 2: tells if puzzle is unique, ambiguous, or
  // timeout-limited.
  InstrumentedSolver counter(puzzle, timeout_ms);
  a.solutions_up_to_2 = counter.count_solutions_up_to(2);
  a.count_stats = counter.get_stats();
  a.count_timed_out = a.count_stats.timed_out;

  a.dynamic_score = compute_dynamic_score(a.solve_stats, a.sf.cells);
  double raw = 0.52 * a.sf.static_score + 0.48 * a.dynamic_score;

  if (!a.solved && a.solve_stats.timed_out) {
    a.quality = "UNKNOWN: solver timed out before proving solvability";
    raw = max(raw, 85.0);
  } else if (!a.solved) {
    a.quality = "INVALID: no full-cover solution found";
    raw = max(raw, 75.0);
  } else if (a.count_timed_out) {
    a.quality = "SOLVABLE: uniqueness unknown because counting timed out";
    raw = max(raw, 70.0);
  } else if (a.solutions_up_to_2 == 1) {
    a.quality = "WELL-FORMED: unique solution";
  } else if (a.solutions_up_to_2 >= 2) {
    a.quality = "AMBIGUOUS: at least two solutions";
    raw = max(
        0.0,
        raw -
            8.0); // ambiguous puzzles are lower quality, not necessarily easier
  } else {
    a.quality = "INVALID: solution counter found zero solutions";
    raw = max(raw, 75.0);
  }

  a.final_score = clamp100(raw);
  a.band = score_band(a.final_score);
  return a;
}

static string one_line_hint(const DifficultyAnalysis &a) {
  vector<pair<double, string>> drivers;
  drivers.push_back({a.sf.distance_score, "long endpoint distances"});
  drivers.push_back({a.sf.sparsity_score, "sparse clues / long paths"});
  drivers.push_back({a.sf.interaction_score, "overlapping pair boxes"});
  drivers.push_back({a.sf.freedom_score, "many early movement choices"});
  drivers.push_back({a.dynamic_score, "solver search effort"});
  sort(drivers.rbegin(), drivers.rend());
  ostringstream oss;
  oss << drivers[0].second;
  if (drivers.size() > 1)
    oss << ", " << drivers[1].second;
  return oss.str();
}

static void print_feature_line(const string &name, double value, double score,
                               const string &meaning) {
  cout << left << setw(28) << name << right << setw(12) << fixed
       << setprecision(3) << value << setw(12) << fixed << setprecision(1)
       << score << "   " << meaning << "\n";
}

static void print_analysis_report(const DifficultyAnalysis &a,
                                  bool show_pairs) {
  cout << "\n===== Numberlink Difficulty Designer Report =====\n";
  cout << "Grid: " << a.sf.h << " x " << a.sf.w << "  | cells=" << a.sf.cells
       << "  | pairs=" << a.sf.pairs << "  | endpoints=" << a.sf.endpoints
       << "\n";
  cout << "Quality: " << a.quality << "\n";
  cout << "Difficulty: " << (int)llround(a.final_score) << "/100  (" << a.band
       << ")\n";
  cout << "Main drivers: " << one_line_hint(a) << "\n";

  cout << "\n--- Static puzzle features ---\n";
  cout << left << setw(28) << "Feature" << right << setw(12) << "Value"
       << setw(12) << "Score" << "   Meaning\n";
  cout << string(78, '-') << "\n";
  print_feature_line("Board scale", a.sf.cells, a.sf.board_score,
                     "larger board increases global planning");
  print_feature_line("Endpoint density", a.sf.endpoint_density,
                     a.sf.sparsity_score, "lower density means longer paths");
  print_feature_line("Avg Manhattan distance", a.sf.avg_manhattan,
                     a.sf.distance_score, "farther pairs need longer routes");
  print_feature_line("Avg endpoint freedom", a.sf.avg_endpoint_free,
                     a.sf.freedom_score,
                     "more choices near clues increases ambiguity");
  print_feature_line("Endpoint bottleneck", a.sf.bottleneck,
                     a.sf.bottleneck_score,
                     "tight starts create forced corridors");
  print_feature_line("Box interaction", a.sf.interaction_pressure,
                     a.sf.interaction_score,
                     "overlapping pair rectangles imply path interference");
  print_feature_line("Clue clustering", a.sf.clustering_pressure,
                     a.sf.clustering_score,
                     "nearby different labels can mislead local routing");
  print_feature_line("Parity skew", a.sf.parity_pressure, a.sf.parity_score,
                     "checkerboard imbalance creates exact-cover pressure");
  print_feature_line("Distance variation", a.sf.manhattan_cv,
                     a.sf.variance_score,
                     "mix of short and long paths affects planning");
  cout << "Static subtotal: " << fixed << setprecision(1) << a.sf.static_score
       << "/100\n";

  cout << "\n--- Solver effort trace ---\n";
  cout << "Solved: " << (a.solved ? "yes" : "no") << "  | solve time: " << fixed
       << setprecision(2) << a.solve_stats.elapsed_ms << " ms"
       << "  | timed out: " << (a.solve_stats.timed_out ? "yes" : "no") << "\n";
  cout << "DFS calls: " << a.solve_stats.dfs_calls
       << "  | branch nodes: " << a.solve_stats.branch_nodes
       << "  | avg branch options: " << fixed << setprecision(2)
       << a.solve_stats.avg_branch_options() << "\n";
  cout << "Moves tried: " << a.solve_stats.moves_tried
       << "  | forced moves: " << a.solve_stats.forced_moves
       << "  | backtracks: " << a.solve_stats.backtracks
       << "  | dead ends: " << a.solve_stats.dead_ends << "\n";
  cout << "Max search depth: " << a.solve_stats.max_depth
       << "  | max filled cells seen: " << a.solve_stats.max_filled_cells << "/"
       << a.sf.cells
       << "  | max forced chain: " << a.solve_stats.max_forced_chain << "\n";

  cout << "\nPrune hits:\n";
  cout << "  degree consistency       : " << a.solve_stats.prune_basic_degree
       << "\n";
  cout << "  stuck tip                : " << a.solve_stats.prune_stuck_tip
       << "\n";
  cout << "  endpoint reachability    : " << a.solve_stats.prune_reachability
       << "\n";
  cout << "  empty-cell future degree : " << a.solve_stats.prune_empty_degree
       << "\n";
  cout << "  empty-region component   : " << a.solve_stats.prune_component
       << "\n";
  cout << "  timeout                  : " << a.solve_stats.prune_timeout
       << "\n";
  cout << "Dynamic subtotal: " << fixed << setprecision(1) << a.dynamic_score
       << "/100\n";

  cout << "\n--- Uniqueness check ---\n";
  cout << "Solutions found up to 2: " << a.solutions_up_to_2
       << "  | count time: " << fixed << setprecision(2)
       << a.count_stats.elapsed_ms << " ms"
       << "  | timed out: " << (a.count_timed_out ? "yes" : "no") << "\n";

  if (show_pairs) {
    cout << "\n--- Pair-by-pair clue analysis ---\n";
    cout << left << setw(8) << "Label" << setw(14) << "A(r,c)" << setw(14)
         << "B(r,c)" << right << setw(8) << "Manh" << setw(10) << "FreeA"
         << setw(10) << "FreeB" << setw(13) << "SameColor" << "\n";
    cout << string(77, '-') << "\n";
    for (const auto &pf : a.sf.pair_features) {
      ostringstream A, B;
      A << "(" << pf.a.r << "," << pf.a.c << ")";
      B << "(" << pf.b.r << "," << pf.b.c << ")";
      cout << left << setw(8) << pf.original_label << setw(14) << A.str()
           << setw(14) << B.str() << right << setw(8) << pf.manhattan
           << setw(10) << pf.free_a << setw(10) << pf.free_b << setw(13)
           << (pf.same_checker_color ? "yes" : "no") << "\n";
    }
  }

  cout << "\nVerdict: ";
  if (a.band == "Expert")
    cout
        << "very search-heavy; use it as a top-level challenge, not a warm-up.";
  else if (a.band == "Hard")
    cout << "good serious puzzle; enough ambiguity to require planning.";
  else if (a.band == "Medium")
    cout << "balanced; suitable for normal practice sets.";
  else if (a.band == "Easy")
    cout << "mostly guided by local constraints; useful as an intro puzzle.";
  else
    cout << "too direct; likely not interesting unless used for demo/testing.";
  cout << "\n";
}

static void usage(const char *argv0) {
  cerr << "Numberlink Difficulty Analyzer\n\n";
  cerr << "Usage:\n";
  cerr << "  " << argv0
       << " analyze [--timeout-ms N] [--no-pairs] < puzzle.txt\n";
  cerr << "  " << argv0 << " rate    [--timeout-ms N] < puzzle.txt\n\n";
  cerr << "Examples:\n";
  cerr << "  " << argv0 << " analyze < puzzle.txt\n";
  cerr << "  " << argv0
       << " analyze --timeout-ms 5000 --no-pairs < puzzle.txt\n";
  cerr << "  " << argv0 << " rate < puzzle.txt\n";
}

int main(int argc, char **argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  string mode = argv[1];

  try {
    if (mode != "analyze" && mode != "rate") {
      usage(argv[0]);
      return 1;
    }

    long long timeout_ms = 5000;
    bool show_pairs = true;

    for (int i = 2; i < argc; ++i) {
      string arg = argv[i];

      if (arg == "--timeout-ms" && i + 1 < argc) {
        timeout_ms = stoll(argv[++i]);
      } else if (arg == "--no-pairs") {
        if (mode == "rate") {
          cerr << "--no-pairs is only valid with analyze; rate prints only the "
                  "score.\n";
          return 1;
        }
        show_pairs = false;
      } else {
        cerr << "Unknown argument: " << arg << "\n";
        usage(argv[0]);
        return 1;
      }
    }

    auto puzzle = parse_input(read_all_stdin());
    auto analysis = analyze_puzzle(puzzle, timeout_ms, false);

    if (mode == "rate") {
      cout << (int)llround(analysis.final_score) << "\n";
    } else {
      print_analysis_report(analysis, show_pairs);
    }

    return 0;
  } catch (const exception &e) {
    cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
