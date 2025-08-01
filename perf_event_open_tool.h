#ifndef PERF_EVENT_OPEN_TOOL_H
#define PERF_EVENT_OPEN_TOOL_H
#ifndef NO_PERF_MONITOR

#include <vector>
#include <string>
#include <linux/perf_event.h>
#include <stdint.h>
#include <sys/types.h>
#include <map>
#include <memory>

/**
 * @brief 硬件性能计数器工具类，基于perf_event_open系统调用。
 *
 * 支持单事件和多事件统计，接口类似chrono，可用于代码段前后插桩。
 * 支持输出到标准输出或日志文件。
 */
class PerfEventOpenTool {
public:
    /**
     * @brief 支持的事件类型
     */
    enum class EventType {
        CPU_CYCLES,
        INSTRUCTIONS,
        CACHE_MISSES,
        CACHE_REFERENCES,
        BRANCH_MISSES,
        BRANCH_INSTRUCTIONS,
        BUS_CYCLES,
        STALLED_CYCLES_FRONTEND,
        STALLED_CYCLES_BACKEND,
        RAW // 原始事件，需指定event_code
    };

    /**
     * @brief 默认构造函数,监控cache miss和branch miss
     */
    PerfEventOpenTool();

    /**
     * @brief 构造函数，单事件
     * @param event 事件类型
     * @param raw_config RAW事件时的event_code
     */
    PerfEventOpenTool(EventType event, uint64_t raw_config = 0);

    /**
     * @brief 构造函数，多事件
     * @param events 事件类型数组
     * @param raw_configs RAW事件时的event_code数组（可选）
     */
    PerfEventOpenTool(const std::vector<EventType>& events, const std::vector<uint64_t>& raw_configs = {});

    /**
     * @brief 构造函数，直接监测任意perf_event事件（单事件）
     * @param perf_type 事件类型（如PERF_TYPE_HARDWARE、PERF_TYPE_SOFTWARE、PERF_TYPE_HW_CACHE、PERF_TYPE_RAW等）
     * @param perf_config 事件配置（如PERF_COUNT_HW_CPU_CYCLES、PERF_COUNT_SW_PAGE_FAULTS_MIN等）
     */
    PerfEventOpenTool(uint32_t perf_type, uint64_t perf_config);

    /**
     * @brief 构造函数，支持多个事件类型和配置（分组统计）
     * @param perf_types 事件类型数组
     * @param perf_configs 事件配置数组
     */
    PerfEventOpenTool(const std::vector<uint32_t>& perf_types, const std::vector<uint64_t>& perf_configs);

    /**
     * @brief 支持自定义事件名字的构造函数
     * @param perf_types 事件类型数组
     * @param perf_configs 事件配置数组
     * @param event_names 事件名字数组，和类型、配置一一对应
     */
    PerfEventOpenTool(const std::vector<uint32_t>& perf_types, const std::vector<uint64_t>& perf_configs, const std::vector<std::string>& event_names);

    /**
     * @brief 启动计数器（在关键代码前调用）
     */
    void start();

    /**
     * @brief 停止计数器（在关键代码后调用）
     */
    void stop();

    /**
     * @brief 获取所有事件的计数结果
     * @return 事件名到计数值的映射
     */
    std::map<std::string, uint64_t> getResults() const;

    /**
     * @brief 结果输出到标准输出
     */
    void printResults() const;

    /**
     * @brief 结果输出到日志文件
     * @param log_path 日志文件路径
     */
    void logResults(const std::string& log_path) const;

    /**
     * @brief 获取Cache Miss次数
     */
    uint64_t getCacheMissCount() const;

    /**
     * @brief 获取Cache Reference次数
     */
    uint64_t getCacheReferenceCount() const;

    /**
     * @brief 获取Branch Miss次数
     */
    uint64_t getBranchMissCount() const;

    /**
     * @brief 获取Branch Instruction次数
     */
    uint64_t getBranchInstructionCount() const;

    /**
     * @brief 获取Cache Miss率（如未采集相关事件则返回0.0）
     */
     double getCacheMissRate() const;
     
    /**
     * @brief 获取Branch Miss率（如未采集相关事件则返回0.0）
     */
    double getBranchMissRate() const;

    /**
     * @brief 通过事件名字获取计数值
     * @param name 事件名字
     * @return 计数值
     */
    uint64_t getResultByName(const std::string& name) const;
    /**
     * @brief 获取所有自定义名字的事件计数结果
     * @return 名字到计数的映射
     */
    std::map<std::string, uint64_t> getResultsByName() const;

    /**
     * @brief 析构函数，自动关闭fd
     */
    ~PerfEventOpenTool();

private:
    struct EventInfo {
        int fd;
        EventType type;
        uint64_t raw_config;
        uint64_t id;
        uint64_t value;
    };
    std::vector<EventInfo> events_;
    bool started_ = false;
    bool stopped_ = false;
    int group_leader_fd_ = -1;
    void openEvents(const std::vector<EventType>& events, const std::vector<uint64_t>& raw_configs);
    static std::string eventTypeToString(EventType type, uint64_t raw_config = 0);
    std::vector<std::string> event_names_;
    std::map<std::string, size_t> name2idx_;
};

#else

// 空实现（no-op）
#include <vector>
#include <string>
#include <map>
#include <stdint.h>

class PerfEventOpenTool {
public:
    enum class EventType {
        CPU_CYCLES,
        INSTRUCTIONS,
        CACHE_MISSES,
        CACHE_REFERENCES,
        BRANCH_MISSES,
        BRANCH_INSTRUCTIONS,
        BUS_CYCLES,
        STALLED_CYCLES_FRONTEND,
        STALLED_CYCLES_BACKEND,
        RAW
    };
    PerfEventOpenTool(EventType event, uint64_t raw_config = 0) {}
    PerfEventOpenTool(const std::vector<EventType>& events, const std::vector<uint64_t>& raw_configs = {}) {}
    PerfEventOpenTool(uint32_t perf_type, uint64_t perf_config) {}
    PerfEventOpenTool(const std::vector<uint32_t>& perf_types, const std::vector<uint64_t>& perf_configs) {}
    void start() {}
    void stop() {}
    std::map<std::string, uint64_t> getResults() const { return {}; }
    void printResults() const {}
    void logResults(const std::string& log_path) const {}
    uint64_t getCacheMissCount() const { return 0; }
    uint64_t getCacheReferenceCount() const { return 0; }
    uint64_t getBranchMissCount() const { return 0; }
    uint64_t getBranchInstructionCount() const { return 0; }
    double getCacheMissRate() const { return 0.0; }
    double getBranchMissRate() const { return 0.0; }
    uint64_t getResultByName(const std::string& name) const { return 0; }
    std::map<std::string, uint64_t> getResultsByName() const { return {}; }
    ~PerfEventOpenTool() {}
};

#endif

#endif // PERF_EVENT_OPEN_TOOL_H 