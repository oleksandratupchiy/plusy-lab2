// Компілятор: MSVC

#include <iostream>
#include <vector>
#include <thread>
#include <iomanip>
#include <chrono>
#include <functional>
#include <algorithm>
#include <numeric>
#include <execution>
#include <limits>
#include <cmath>
#include <string>
#include <fstream> 
#include "random.h"

using namespace std;

template <typename Function>
double MeasureExecutionTime(Function&& f) {
    auto start = chrono::high_resolution_clock::now();
    f();
    auto end = chrono::high_resolution_clock::now();
    return chrono::duration<double>(end - start).count();
}

const int TARGET_VALUE = 2;
bool IsTarget(int x) { return x == TARGET_VALUE; }

bool ParallelAnyOf(const vector<int>& vec, int K, function<bool(int)> predicate) {
    size_t dataSize = vec.size();
    if (dataSize == 0) return false;

    vector<bool> results(K, false);
    vector<thread> threads;
    size_t chunkSize = (dataSize + K - 1) / K;

    for (int i = 0; i < K; ++i) {
        size_t start = (size_t)i * chunkSize;
        size_t end = min(start + chunkSize, dataSize);
        if (start >= end) continue;

        threads.emplace_back([&, start, end, i]() {
            results[i] = any_of(vec.begin() + start, vec.begin() + end, predicate);
            });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    return any_of(results.begin(), results.end(), [](bool res) { return res; });
}

void FillRandomNonTarget(vector<int>& vec) {

    std::uniform_int_distribution<> distrib(1, 3);

    for (int& x : vec) {
        do {
            x = distrib(Random::engine());
        } while (x == TARGET_VALUE);
    }
}

vector<int> InitializeWorstCase(size_t dataSize) {
    vector<int> vec(dataSize);
    FillRandomNonTarget(vec);
    return vec;
}

vector<int> InitializeBestCase(size_t dataSize) {
    vector<int> vec(dataSize);
    FillRandomNonTarget(vec);
    if (dataSize > 0) vec[0] = TARGET_VALUE;
    return vec;
}

vector<int> InitializeAverageCase(size_t dataSize) {
    vector<int> vec(dataSize);
    FillRandomNonTarget(vec);
    if (dataSize > 0) vec[dataSize / 2] = TARGET_VALUE;
    return vec;
}

void AnalyzePerformanceForCase(const string& caseName, const vector<int>& vec) {
    size_t dataSize = vec.size();
    int maxThreads = thread::hardware_concurrency();
    int maxK = max(maxThreads * 4, 8);

    cout << "\n--- Standard Library any_of (" << caseName << ", N=" << dataSize << ") ---\n";

    double time_no_policy = MeasureExecutionTime([&]() { volatile bool r = std::any_of(vec.begin(), vec.end(), IsTarget); });
    cout << "Time without policy: " << time_no_policy << " seconds\n";

    double time_seq = MeasureExecutionTime([&]() { volatile bool r = std::any_of(std::execution::seq, vec.begin(), vec.end(), IsTarget); });
    cout << "Time with sequential policy: " << time_seq << " seconds\n";

    double time_par = MeasureExecutionTime([&]() { volatile bool r = std::any_of(std::execution::par, vec.begin(), vec.end(), IsTarget); });
    cout << "Time with parallel policy: " << time_par << " seconds\n";

    double time_par_unseq = MeasureExecutionTime([&]() { volatile bool r = std::any_of(std::execution::par_unseq, vec.begin(), vec.end(), IsTarget); });
    cout << "Time with parallel-unsequenced policy: " << time_par_unseq << " seconds\n";

    vector<int> K_values = { 1 };
    for (int K = 2; K <= maxThreads; ++K) K_values.push_back(K);
    if (maxThreads > 1) {
        K_values.push_back(maxThreads * 2);
        K_values.push_back(maxK);
    }
    sort(K_values.begin(), K_values.end());
    K_values.erase(unique(K_values.begin(), K_values.end()), K_values.end());

    double bestTime = numeric_limits<double>::max();
    int bestK = 0;

    cout << "\n--- Custom Parallel any_of (K analysis) ---\n";

    cout << "K values (Time in seconds):\n";
    for (int K : K_values) {
        if ((size_t)K > vec.size() && K > maxThreads * 2) continue;

        double minTime = numeric_limits<double>::max();
        for (int run = 0; run < 3; ++run) {
            double currentTime = MeasureExecutionTime([&]() { volatile bool res = ParallelAnyOf(vec, K, IsTarget); });
            minTime = min(minTime, currentTime);
        }

        cout << "K=" << K << ": " << fixed << setprecision(10) << minTime << "\n";

        if (minTime < bestTime) {
            bestTime = minTime;
            bestK = K;
        }
    }

    cout << "\nBest K found: " << bestK << " (Time: " << bestTime << " seconds)\n";
    cout << "Processor threads (P): " << maxThreads << "\n";
    cout << "Best K/P ratio: " << (double)bestK / maxThreads << "\n";
}

int main() {
    std::ofstream file_out("results.txt");
    if (!file_out.is_open()) {
        cerr << "Error: Could not open results.txt for writing.\n";
        return 1;
    }
    std::streambuf* cout_buffer_original = std::cout.rdbuf();
    std::cout.rdbuf(file_out.rdbuf());

    cout << fixed << setprecision(10);

    const size_t dataSize = 50000000;

    // Worst Case
    vector<int> vec_worst = InitializeWorstCase(dataSize);
    AnalyzePerformanceForCase("Worst Case", vec_worst);

    // Best Case
    vector<int> vec_best = InitializeBestCase(dataSize);
    AnalyzePerformanceForCase("Best Case", vec_best);

    // Average Case
    vector<int> vec_avg = InitializeAverageCase(dataSize);
    AnalyzePerformanceForCase("Average Case", vec_avg);

    std::cout.rdbuf(cout_buffer_original);
    file_out.close();

    cout << "All results have been successfully written to results.txt\n";

    return 0;
}