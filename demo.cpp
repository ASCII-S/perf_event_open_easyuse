#include "perf_event_open_tool.h"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>


void my_code() {
    const int N = 2048;
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

void multi_event_test(){
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
    PerfEventOpenTool tool(events);
    tool.start();
    my_code();
    tool.stop();

    // 输出到文件
    std::string log_path = "perf.log";
    std::ofstream ofs(log_path, std::ios::trunc);
    tool.logResults(log_path);

    // 输出到控制台
    tool.printResults();

    // 计算miss rate
    std::map<std::string, uint64_t> results = tool.getResults();
    std::cout << "Cache miss rate:" << 100.0*results["CACHE_MISSES"] / results["CACHE_REFERENCES"] << "%" << std::endl;
    std::cout << "Branch miss rate:" << 100.0*results["BRANCH_MISSES"] / results["BRANCH_INSTRUCTIONS"] << "%" << std::endl;
}

void multi_raw_event_test(){
    using std::vector;
    using std::string;

    // 构造所有需要的事件
    vector<uint32_t> types;
    vector<uint64_t> configs;
    vector<string> names;

    // L1D
    types.push_back(PERF_TYPE_HW_CACHE);
    configs.push_back(PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    names.push_back("L1D_access");
    types.push_back(PERF_TYPE_HW_CACHE);
    configs.push_back(PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    names.push_back("L1D_miss");

    // L1I
    // types.push_back(PERF_TYPE_HW_CACHE);
    // configs.push_back(PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    // names.push_back("L1I_access");
    // types.push_back(PERF_TYPE_HW_CACHE);
    // configs.push_back(PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    // names.push_back("L1I_miss");

    // DTLB
    types.push_back(PERF_TYPE_HW_CACHE);
    configs.push_back(PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    names.push_back("DTLB_access");
    types.push_back(PERF_TYPE_HW_CACHE);
    configs.push_back(PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    names.push_back("DTLB_miss");

    // ITLB
    // types.push_back(PERF_TYPE_HW_CACHE);
    // configs.push_back(PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16));
    // names.push_back("ITLB_access");
    // types.push_back(PERF_TYPE_HW_CACHE);
    // configs.push_back(PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
    // names.push_back("ITLB_miss");

    try {
        PerfEventOpenTool tool(types, configs);

        tool.start();
        my_code();
        tool.stop();

        std::cout << "--------------------------------" << std::endl;
        auto results = tool.getResults();
        std::map<std::string, uint64_t> value_map;

        for (size_t i = 0; i < names.size(); ++i) {
            std::string key = "RAW_" + std::to_string(configs[i]);
            value_map[names[i]] = results[key];
            std::cout << names[i] << ": " << results[key] << " key: " << key << std::endl;
        }   
        // 自动输出 miss rate
        for (const auto& kv : value_map) {
            const std::string& name = kv.first;
            if (name.size() > 5 && name.substr(name.size() - 5) == "_miss") {
                std::string prefix = name.substr(0, name.size() - 5);
                std::string access_name = prefix + "_access";
                if (value_map.count(access_name) && value_map[access_name] > 0) {
                    double rate = 100.0 * value_map[name] / value_map[access_name];
                    std::cout << std::fixed << std::setprecision(4)
                            << prefix << " miss rate: " << rate << "%" << std::endl;
                } else {
                    std::cout << prefix << " miss rate: N/A" << std::endl;
                }
            }
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "perf_event_open failed: " << e.what() << std::endl;
        std::cerr << "部分事件可能不被本机支持，请用 perf list 检查。" << std::endl;
    }
}

int main() {

    // my_code();

    // 单事件统计
    // PerfEventOpenTool tool(PerfEventOpenTool::EventType::CPU_CYCLES);
    // tool.start();
    // my_code();
    // tool.stop();
    // tool.printResults();

    multi_event_test();

    // multi_raw_event_test();
}
