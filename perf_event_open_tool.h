#ifndef PERF_EVENT_OPEN_TOOL_CLASS_H
#define PERF_EVENT_OPEN_TOOL_CLASS_H

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
};

#endif // PERF_EVENT_OPEN_TOOL_CLASS_H 