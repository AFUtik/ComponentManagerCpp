#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <span>
#include <string>
#include <vector>
#include <locale>

#include "spatial/QuadTree.hpp"
#include <glm/glm.hpp>

using Clock = std::chrono::steady_clock;
using ns    = std::chrono::nanoseconds;

struct BenchResult {
    std::string name;
    double      totalMs;
    double      nsPerOp;
    double      mOpsPerSec;
    u32         checksum; 
};

template<typename Fn>
void warmup(Fn&& fn, int iters = 3) {
    for (int i = 0; i < iters; ++i) fn();
}

// Несколько прогонов → берём медиану
template<typename Fn>
BenchResult measure(const std::string& name, int runs, int opsPerRun, Fn&& fn) {
    std::vector<double> times;
    times.reserve(runs);
    u32 cs = 0;
    warmup(fn);
    for (int r = 0; r < runs; ++r) {
        auto t0 = Clock::now();
        cs ^= fn();
        auto t1 = Clock::now();
        times.push_back(static_cast<double>(
            std::chrono::duration_cast<ns>(t1 - t0).count()));
    }
    std::sort(times.begin(), times.end());
    const double median = times[times.size() / 2];
    return {
        name,
        median * 1e-6,
        median / opsPerRun,
        (opsPerRun / median) * 1e3,
        cs
    };
}

void printHeader() {
    std::cout << "\n"
              << std::left  << std::setw(38) << "Тест"
              << std::right << std::setw(10) << "Median ms"
              << std::right << std::setw(12) << "ns/op"
              << std::right << std::setw(12) << "Mops/s"
              << "\n"
              << std::string(72, '-') << "\n";
}

void printRow(const BenchResult& r) {
    std::cout << std::left  << std::setw(38) << r.name
              << std::right << std::setw(10) << std::fixed << std::setprecision(2) << r.totalMs
              << std::right << std::setw(12) << std::fixed << std::setprecision(1) << r.nsPerOp
              << std::right << std::setw(12) << std::fixed << std::setprecision(2) << r.mOpsPerSec
              << "\n";
}

void printSeparator(const std::string& label) {
    std::cout << "\n-- " << label << " " << std::string(std::max<int>(0, 68 - (int)label.size() - 4), '-') << "\n";
}

using F   = float;
using V2  = glm::vec<2, F>;
using Point = V2;
using QT  = QuadTree<u32, Point, F, V2>;
using Box = AABB2D<F, V2>;

// Равномерное распределение
std::vector<V2> makeUniform(int n, uint64_t seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<F> dist(0.f, 1000.f);
    std::vector<V2> pts(n);
    for (auto& p : pts) p = {dist(rng), dist(rng)};
    return pts;
}

// Кластерное распределение (более реалистично для игр/симуляций)
std::vector<V2> makeClustered(int n, int clusters = 32, uint64_t seed = 7) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<F> world(50.f, 950.f);
    std::normal_distribution<F>       spread(0.f, 20.f);
    std::vector<V2> centers(clusters);
    for (auto& c : centers) c = {world(rng), world(rng)};
    std::vector<V2> pts(n);
    for (int i = 0; i < n; ++i) {
        const V2& c = centers[i % clusters];
        F x = std::clamp(c.x + spread(rng), (F)0., (F)999.9);
        F y = std::clamp(c.y + spread(rng), (F)0., (F)999.9);
        pts[i] = {x, y};
    }
    return pts;
}

// Вспомогательная: строим дерево с данными
QT buildTree(const std::vector<V2>& pts, u32 maxDepth=6, u32 cap=8) {
    QT qt;
    qt.maxDepth     = maxDepth;
    qt.nodeCapacity = cap;
    qt.init({0,0,1000,1000},
            /*nodes*/ static_cast<u32>(pts.size() * 4 + 64),
            /*items*/ static_cast<u32>(pts.size() * 2));
    for (u32 i = 0; i < static_cast<u32>(pts.size()); ++i)
        qt.insert(i, pts[i]);
    return qt;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Brute-force baseline
// ─────────────────────────────────────────────────────────────────────────────
struct BruteForce {
    std::vector<V2> pos;
    std::vector<u32> data;

    void build(const std::vector<V2>& pts) {
        pos  = pts;
        data.resize(pts.size());
        for (u32 i = 0; i < data.size(); ++i) data[i] = i;
    }

    template<typename Cb>
    void query(const Box& rect, Cb&& cb) const noexcept {
        for (u32 i = 0; i < pos.size(); ++i)
            if (rect.contains(pos[i])) cb(data[i]);
    }

    const u32* nearest(V2 p, F radius) const noexcept {
        F best2 = radius * radius; const u32* best = nullptr;
        for (u32 i = 0; i < pos.size(); ++i) {
            V2 d = pos[i] - p;
            F d2 = d.x*d.x + d.y*d.y;
            if (d2 < best2) { best2 = d2; best = &data[i]; }
        }
        return best;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
     setlocale(LC_ALL, "ru_RU.UTF-8");

    std::cout << "QuadTree performance benchmark\n";
    std::cout << "Node size: " << sizeof(QT::Node) << " bytes";
    std::cout << "  (nodes per 64-byte cache line: "
              << std::fixed << std::setprecision(2)
              << 64.0 / sizeof(QT::Node) << ")\n";

    constexpr int RUNS = 9; // нечётное → медиана корректна

    // ─────────────────────────────────────────────────────────────────────────
    //  1. INSERT
    // ─────────────────────────────────────────────────────────────────────────
    printSeparator("INSERT — равномерное распределение");
    printHeader();

    for (int N : {10'000, 100'000, 500'000, 1'000'000}) {
        auto pts = makeUniform(N);
        auto r = measure("insert " + std::to_string(N), RUNS, N, [&]() -> u32 {
            QT qt;
            qt.init({0,0,1000,1000}, 6, 8, static_cast<u32>(N*4+64), static_cast<u32>(N*2));
            u32 ok = 0;
            for (u32 i = 0; i < static_cast<u32>(pts.size()); ++i)
                ok += qt.insert(i, pts[i]);
            return ok;
        });
        printRow(r);
    }

    printSeparator("INSERT — кластерное распределение");
    printHeader();

    for (int N : {10'000, 100'000, 500'000, 1'000'000}) {
        auto pts = makeClustered(N);
        auto r = measure("insert (clustered) " + std::to_string(N), RUNS, N, [&]() -> u32 {
            QT qt;
            qt.init({0,0,1000,1000}, 6, 8, static_cast<u32>(N*4+64), static_cast<u32>(N*2));
            u32 ok = 0;
            for (u32 i = 0; i < static_cast<u32>(pts.size()); ++i)
                ok += qt.insert(i, pts[i]);
            return ok;
        });
        printRow(r);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  2. QUERY — разные размеры прямоугольника vs brute-force
    // ─────────────────────────────────────────────────────────────────────────
    printSeparator("QUERY — квадродерево vs brute-force (N=200k, 1000 запросов)");
    printHeader();

    constexpr int QN = 200'000;
    constexpr int QC = 1'000;

    auto uPts = makeUniform(QN);
    auto cPts = makeClustered(QN);

    // Генерируем центры запросов заранее
    auto queryCenters = makeUniform(QC, 99);
    /*
    struct QueryCase { const char* label; F size; };
    for (auto [label, sz] : std::initializer_list<QueryCase>{
            {"qt  query 1% box  (uniform)",  100.f},
            {"qt  query 5% box  (uniform)",  224.f},
            {"qt  query 20% box (uniform)",  447.f},
            {"qt  query 1% box  (clustered)",100.f},
            {"qt  query 5% box  (clustered)",224.f},
    }) {
        const bool clustered = (std::string(label).find("clustered") != std::string::npos);
        const auto& pts = clustered ? cPts : uPts;
        QT qt = buildTree(pts);

        auto r = measure(label, RUNS, QC, [&]() -> u32 {
            u32 cs = 0;
            for (int q = 0; q < QC; ++q) {
                V2 c = queryCenters[q];
                Box rect{c.x - sz*0.5f, c.y - sz*0.5f,
                         c.x + sz*0.5f, c.y + sz*0.5f};
                qt.query(rect, [&](u32 v){ cs ^= v; });
            }
            return cs;
        });
        printRow(r);
    }

    // brute-force baseline
    {
        BruteForce bf; bf.build(uPts);
        for (auto [label, sz] : std::initializer_list<QueryCase>{
                {"bf  query 1% box  (uniform)",  100.f},
                {"bf  query 5% box  (uniform)",  224.f},
                {"bf  query 20% box (uniform)",  447.f},
        }) {
            auto r = measure(label, RUNS, QC, [&]() -> u32 {
                u32 cs = 0;
                for (int q = 0; q < QC; ++q) {
                    V2 c = queryCenters[q];
                    Box rect{c.x - sz*0.5f, c.y - sz*0.5f,
                             c.x + sz*0.5f, c.y + sz*0.5f};
                    bf.query(rect, [&](u32 v){ cs ^= v; });
                }
                return cs;
            });
            printRow(r);
        }
    }
   

    // ─────────────────────────────────────────────────────────────────────────
    //  3. NEAREST — квадродерево vs brute-force
    // ─────────────────────────────────────────────────────────────────────────
    printSeparator("NEAREST — квадродерево vs brute-force (N=200k, 10k запросов)");
    printHeader();

    constexpr int NC = 10'000;
    auto nearestCenters = makeUniform(NC, 55);

    {
        QT qt = buildTree(uPts);
        BruteForce bf; bf.build(uPts);

        for (auto [label, rad] : std::initializer_list<QueryCase>{
                {"qt  nearest r=10", 10.f},
                {"qt  nearest r=50", 50.f},
        }) {
            auto r = measure(label, RUNS, NC, [&]() -> u32 {
                u32 cs = 0;
                for (int q = 0; q < NC; ++q) {
                    const u32* p = qt.nearest(nearestCenters[q], rad);
                    if (p) cs ^= *p;
                }
                return cs;
            });
            printRow(r);
        }

        for (auto [label, rad] : std::initializer_list<QueryCase>{
                {"bf  nearest r=10", 10.f},
                {"bf  nearest r=50", 50.f},
        }) {
            auto r = measure(label, RUNS, NC, [&]() -> u32 {
                u32 cs = 0;
                for (int q = 0; q < NC; ++q) {
                    const u32* p = bf.nearest(nearestCenters[q], rad);
                    if (p) cs ^= *p;
                }
                return cs;
            });
            printRow(r);
        }
    }
    

    // ─────────────────────────────────────────────────────────────────────────
    //  4. ПАРАМЕТРЫ ДЕРЕВА — влияние maxDepth и nodeCapacity
    // ─────────────────────────────────────────────────────────────────────────
    printSeparator("ПАРАМЕТРЫ — maxDepth / nodeCapacity (N=100k, 1000 запросов box=1%)");
    printHeader();

    auto basePts = makeUniform(100'000);
    for (auto [d, cap] : std::initializer_list<std::pair<u32,u32>>{
            {4, 8}, {6, 8}, {8, 8}, {6, 4}, {6, 16}, {6, 32}
    }) {
        QT qt;
        qt.maxDepth = d; qt.nodeCapacity = cap;
        qt.init({0,0,1000,1000}, 100'000*4+64, 100'000*2);
        for (u32 i = 0; i < basePts.size(); ++i) qt.insert(i, basePts[i]);

        std::string lbl = "d=" + std::to_string(d)
                        + " cap=" + std::to_string(cap)
                        + " nodes=" + std::to_string(qt.nodeCount());

        auto r = measure(lbl, RUNS, 1'000, [&]() -> u32 {
            u32 cs = 0;
            for (int q = 0; q < 1'000; ++q) {
                V2 c = queryCenters[q];
                Box rect{c.x-50.f, c.y-50.f, c.x+50.f, c.y+50.f};
                qt.query(rect, [&](u32 v){ cs ^= v; });
            }
            return cs;
        });
        printRow(r);
    }

    printSeparator("МАСШТАБИРОВАНИЕ query при росте N (box=1%, 500 запросов)");
    printHeader();

    for (int N : {1'000, 10'000, 50'000, 200'000, 500'000, 1'000'000}) {
        auto pts = makeUniform(N);
        QT qt = buildTree(pts);
        std::string lbl = "N=" + std::to_string(N)
                        + " nodes=" + std::to_string(qt.nodeCount());
        auto r = measure(lbl, RUNS, 500, [&]() -> u32 {
            u32 cs = 0;
            for (int q = 0; q < 500; ++q) {
                V2 c = queryCenters[q % QC];
                Box rect{c.x-50.f, c.y-50.f, c.x+50.f, c.y+50.f};
                qt.query(rect, [&](u32 v){ cs ^= v; });
            }
            return cs;
        });
        printRow(r);
    }
    */

    std::cout << "\n";
    return 0;
}