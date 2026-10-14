// ============================================================
//  Mahjong Fortune 2000 - Demo (C++17, konsol)
//  Koin virtual, hanya hiburan. Tidak bernilai uang.
// ============================================================
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace C {
const string RST = "\033[0m", BOLD = "\033[1m";
const string RED = "\033[31m", GRN = "\033[32m", YEL = "\033[33m";
const string BLU = "\033[34m", CYN = "\033[36m", WHT = "\033[97m";
const string BG_TILE = "\033[48;5;255m";
const string BG_GOLD = "\033[48;5;220m";
const string BG_WILD = "\033[48;5;160m";
const string BG_SCAT = "\033[48;5;208m";
const string BG_WIN  = "\033[48;5;226m";
const string BG_APP  = "\033[48;5;124m";
const string BG_TEAL = "\033[48;5;30m";
}

enum Sym { FA, ZHONG, BAI, WAN8, BAM5, DOT5, DOT3, DOT2, BAM2, SCAT, WILD, NSYM };

struct SymInfo {
    string label;
    string fg;
    double weight;
    array<int, 3> pay;
};

const array<SymInfo, NSYM> INFO = {{
    {" 發 ", C::GRN, 4,  {15, 30, 50}},
    {" 中 ", C::RED, 5,  {10, 25, 40}},
    {"[口]", C::BLU, 6,  { 8, 20, 30}},
    {"八萬", C::RED, 7,  { 6, 15, 25}},
    {"5竹 ", C::GRN, 9,  { 4, 10, 15}},
    {"5筒 ", C::BLU, 9,  { 4, 10, 15}},
    {"3筒 ", C::BLU, 11, { 2,  5, 10}},
    {"2筒 ", C::CYN, 12, { 1,  3,  6}},
    {"2竹 ", C::GRN, 12, { 1,  3,  6}},
    {"SCAT", C::RED, 1.6,{ 0,  0,  0}},
    {" 胡 ", C::YEL, 0,  { 0,  0,  0}},
}};

struct Tile { Sym s; bool gold; };

const array<int, 5> ROWS = {4, 5, 5, 5, 4};
const array<int, 4> BASE_M = {1, 2, 3, 5};
const array<int, 4> FREE_M = {2, 4, 6, 10};
const vector<long long> BETS = {20, 40, 100, 200, 400, 1000, 2000};
const string SAVE_FILE = "mahjong_saldo.txt";

mt19937 rng((unsigned)chrono::steady_clock::now().time_since_epoch().count());
vector<vector<Tile>> grid(5);
long long balance = 100000, lastWin = 0;
int betIdx = 4, freeSpins = 0, cascadeStep = 0;
bool turbo = false;

void sleepMs(int ms) { this_thread::sleep_for(chrono::milliseconds(turbo ? ms / 3 : ms)); }

string fmt(long long n) {
    string s = to_string(n < 0 ? -n : n), out;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        out.insert(out.begin(), s[i]);
        if (++c % 3 == 0 && i > 0) out.insert(out.begin(), '.');
    }
    return (n < 0 ? "-" : "") + out;
}

void loadBalance() { ifstream f(SAVE_FILE); long long b; if (f >> b && b > 0) balance = b; }
void saveBalance() { ofstream f(SAVE_FILE); f << balance; }

Tile randomTile(int reel) {
    double total = 0;
    for (int i = 0; i < WILD; ++i) total += INFO[i].weight;
    uniform_real_distribution<double> d(0, total);
    double r = d(rng);
    Sym s = DOT2;
    for (int i = 0; i < WILD; ++i) { if ((r -= INFO[i].weight) < 0) { s = (Sym)i; break; } }
    bool gold = reel > 0 && reel < 4 && s != SCAT && uniform_real_distribution<double>(0, 1)(rng) < 0.10;
    return {s, gold};
}

void newGrid() {
    for (int c = 0; c < 5; ++c) {
        grid[c].clear();
        for (int r = 0; r < ROWS[c]; ++r) grid[c].push_back(randomTile(c));
    }
}

void clearScreen() { cout << "\033[2J\033[H"; }

string tileStr(const Tile& t, bool highlight) {
    string bg = highlight ? C::BG_WIN
              : t.s == WILD ? C::BG_WILD
              : t.s == SCAT ? C::BG_SCAT
              : t.gold ? C::BG_GOLD : C::BG_TILE;
    string fg = t.s == WILD ? C::WHT : INFO[t.s].fg;
    return bg + C::BOLD + fg + " " + INFO[t.s].label + " " + C::RST;
}

void draw(const vector<vector<bool>>* marks = nullptr, const string& msg = "") {
    clearScreen();
    const auto& M = freeSpins ? FREE_M : BASE_M;
    cout << C::BG_APP << C::BOLD << C::WHT << "  MAHJONG FORTUNE 2000            DEMO  " << C::RST << "\n";
    cout << C::YEL << C::BOLD << "            * 2000 WAYS *" << C::RST << "\n";

    cout << C::BG_TEAL << "  ";
    for (int i = 0; i < 4; ++i) {
        bool on = i == min(cascadeStep, 3);
        cout << (on ? C::BOLD + C::YEL + " >x" : C::RST + C::BG_TEAL + "  x")
             << M[i] << (on ? "< " : "   ") << C::RST << C::BG_TEAL << "  ";
    }
    cout << C::RST << "\n\n";

    for (int line = 0; line < 10; ++line) {
        cout << "  ";
        for (int c = 0; c < 5; ++c) {
            bool shortReel = ROWS[c] == 4;
            int idx = shortReel ? (line - 1) : line;
            if (idx < 0 || idx / 2 >= ROWS[c]) { cout << "       "; continue; }
            int r = idx / 2;
            if (idx % 2 == 0) {
                bool hl = marks && (*marks)[c][r];
                cout << tileStr(grid[c][r], hl) << " ";
            } else {
                cout << "       ";
            }
        }
        cout << "\n";
    }

    cout << "\n" << C::BG_APP << C::YEL << C::BOLD;
    if (freeSpins) cout << "   FREE SPIN TERSISA: " << freeSpins << "   ";
    else           cout << "   3+ SCATTER = 10 FREE SPINS   ";
    cout << C::RST << "\n\n";

    cout << " Saldo  : " << C::CYN << fmt(balance) << C::RST
         << "\n Taruhan: " << C::CYN << fmt(BETS[betIdx]) << C::RST
         << "\n Menang : " << C::CYN << fmt(lastWin) << C::RST
         << "   " << (turbo ? C::YEL + "[TURBO]" + C::RST : "") << "\n";
    if (!msg.empty()) cout << "\n " << C::BOLD << C::YEL << msg << C::RST << "\n";
    cout.flush();
}

long long evaluate(vector<vector<bool>>& marks) {
    marks.assign(5, {});
    for (int c = 0; c < 5; ++c) marks[c].assign(ROWS[c], false);
    long long bet = BETS[betIdx];
    double total = 0;

    for (int k = 0; k < SCAT; ++k) {
        long long ways = 1; int len = 0; bool hasReal = false;
        for (int c = 0; c < 5; ++c) {
            int n = 0;
            for (auto& t : grid[c]) {
                if (t.s == k || t.s == WILD) ++n;
                if (t.s == k) hasReal = true;
            }
            if (!n) break;
            ways *= n; ++len;
        }
        if (len >= 3 && hasReal) {
            total += INFO[k].pay[len - 3] * ways * bet / 20.0;
            for (int c = 0; c < len; ++c)
                for (int r = 0; r < ROWS[c]; ++r)
                    if (grid[c][r].s == k || grid[c][r].s == WILD) marks[c][r] = true;
        }
    }
    return (long long)total;
}

long long runCascades() {
    long long spinWin = 0;
    cascadeStep = 0;
    vector<vector<bool>> marks;
    while (true) {
        long long base = evaluate(marks);
        if (!base) break;
        const auto& M = freeSpins ? FREE_M : BASE_M;
        int mult = M[min(cascadeStep, 3)];
        long long w = base * mult;
        spinWin += w;
        lastWin = spinWin;
        draw(&marks, "MENANG " + fmt(w) + "  (x" + to_string(mult) + ")");
        sleepMs(1100);

        for (int c = 0; c < 5; ++c) {
            vector<Tile> keep;
            for (int r = 0; r < ROWS[c]; ++r) {
                if (!marks[c][r]) keep.push_back(grid[c][r]);
                else if (grid[c][r].gold) keep.push_back({WILD, false});
            }
            vector<Tile> col;
            while ((int)(col.size() + keep.size()) < ROWS[c]) col.push_back(randomTile(c));
            col.insert(col.end(), keep.begin(), keep.end());
            grid[c] = col;
        }
        ++cascadeStep;
        draw(nullptr, "Cascade!");
        sleepMs(600);
    }
    return spinWin;
}

int countScatters() {
    int n = 0;
    for (auto& col : grid) for (auto& t : col) if (t.s == SCAT) ++n;
    return n;
}

void pause(const string& msg) {
    cout << "\n " << C::BOLD << C::YEL << msg << C::RST << "\n Tekan ENTER...";
    string tmp; getline(cin, tmp);
}

void spinOnce(bool paid) {
    long long bet = BETS[betIdx];
    if (paid && !freeSpins) balance -= bet;
    lastWin = 0;

    for (int i = 0; i < 4; ++i) { newGrid(); cascadeStep = 0; draw(nullptr, "Memutar..."); sleepMs(120); }
    newGrid();
    draw();
    sleepMs(300);

    long long w = runCascades();
    balance += w;
    lastWin = w;
    if (freeSpins) --freeSpins;

    int sc = countScatters();
    if (sc >= 3) {
        int add = 10 + (sc - 3) * 2;
        draw(nullptr, to_string(sc) + " SCATTER! +" + to_string(add) + " FREE SPINS (x2 x4 x6 x10)");
        freeSpins += add;
        sleepMs(1800);
    }
    saveBalance();
}

void playFreeSpins() {
    long long total = 0;
    while (freeSpins > 0) {
        spinOnce(false);
        total += lastWin;
        sleepMs(700);
    }
    cascadeStep = 0;
    draw(nullptr, "FREE SPIN SELESAI - TOTAL MENANG " + fmt(total));
    pause("");
}

void doSpin() {
    if (balance < BETS[betIdx]) {
        pause("Saldo habis. Saldo demo diisi ulang menjadi 100.000.");
        balance = 100000; saveBalance(); return;
    }
    spinOnce(true);
    if (freeSpins) playFreeSpins();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleA("Mahjong Fortune 2000 - Demo");
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);
#endif
    loadBalance();
    newGrid();

    string help = "[ENTER] Putar  [+/-] Taruhan  [t] Turbo  [a N] Auto N kali  [b] Feature Buy  [q] Keluar";
    while (true) {
        cascadeStep = 0;
        draw();
        cout << "\n " << help << "\n > ";
        string in;
        if (!getline(cin, in)) break;

        if (in.empty()) doSpin();
        else if (in == "+") { if (betIdx + 1 < (int)BETS.size()) ++betIdx; }
        else if (in == "-") { if (betIdx > 0) --betIdx; }
        else if (in == "t") turbo = !turbo;
        else if (in[0] == 'a') {
            int n = 10;
            try { if (in.size() > 2) n = stoi(in.substr(2)); } catch (...) {}
            for (int i = 0; i < n && balance >= BETS[betIdx]; ++i) { doSpin(); sleepMs(500); }
        }
        else if (in == "b") {
            long long cost = BETS[betIdx] * 50;
            if (balance < cost) { pause("Saldo kurang, butuh " + fmt(cost) + " koin demo."); continue; }
            balance -= cost; freeSpins = 10;
            draw(nullptr, "FEATURE BUY: 10 FREE SPINS (x2 x4 x6 x10)");
            sleepMs(1500);
            playFreeSpins();
        }
        else if (in == "q") break;
    }
    saveBalance();
    cout << "\nTerima kasih sudah bermain!\n";
    return 0;
}
