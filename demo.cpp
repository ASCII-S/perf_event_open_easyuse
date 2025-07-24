#include "perf_event_open_tool_class.h"
#include <iostream>
#include <fstream>
#include <map>
void my_code() {
    const int N = 1024;
    static double A[N][N], B[N][N], C[N][N];
    // 初始化A、B
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j) {
            A[i][j] = i + j;
            B[i][j] = i - j;
            C[i][j] = 0.0;
        }
    // 简单MMM（矩阵乘法）
    for (int i = 0; i < N; ++i)
        for (int k = 0; k < N; ++k)
            for (int j = 0; j < N; ++j)
                C[i][j] += A[i][k] * B[k][j];
}

int main() {
    // 单事件统计
    PerfEventOpenTool tool(PerfEventOpenTool::EventType::RAW);
    tool.start();
    my_code();
    tool.stop();
    tool.printResults();

    // 多事件统计
    std::vector<PerfEventOpenTool::EventType> events = {
        PerfEventOpenTool::EventType::CPU_CYCLES,
        PerfEventOpenTool::EventType::INSTRUCTIONS,
        PerfEventOpenTool::EventType::CACHE_MISSES,
        PerfEventOpenTool::EventType::CACHE_REFERENCES,
        PerfEventOpenTool::EventType::BRANCH_MISSES,
        PerfEventOpenTool::EventType::BRANCH_INSTRUCTIONS,
        PerfEventOpenTool::EventType::BUS_CYCLES,
    };
    PerfEventOpenTool tool2(events);
    tool2.start();
    my_code();
    tool2.stop();
    std::string log_path = "perf.log";
    std::ofstream ofs(log_path, std::ios::trunc);
    tool2.logResults(log_path);

    std::map<std::string, uint64_t> results = tool2.getResults();
    std::cout << "Cache miss rate:" << 100.0*results["CACHE_MISSES"] / results["CACHE_REFERENCES"] << "%" << std::endl;
    std::cout << "Branch miss rate:" << 100.0*results["BRANCH_MISSES"] / results["BRANCH_INSTRUCTIONS"] << "%" << std::endl;

    // 新增：直接传type和config，采集L1D cache read miss
    PerfEventOpenTool tool3(PERF_TYPE_HW_CACHE,
        PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    tool3.start();
    my_code();
    tool3.stop();
    tool3.printResults();
}