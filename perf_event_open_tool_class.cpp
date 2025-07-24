#include "perf_event_open_tool_class.h"
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace {
    int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
        return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    }

    uint64_t eventTypeToConfig(PerfEventOpenTool::EventType type, uint64_t raw_config) {
        using E = PerfEventOpenTool::EventType;
        switch(type) {
            case E::CPU_CYCLES: return PERF_COUNT_HW_CPU_CYCLES;
            case E::INSTRUCTIONS: return PERF_COUNT_HW_INSTRUCTIONS;
            case E::CACHE_MISSES: return PERF_COUNT_HW_CACHE_MISSES;
            case E::CACHE_REFERENCES: return PERF_COUNT_HW_CACHE_REFERENCES;
            case E::BRANCH_MISSES: return PERF_COUNT_HW_BRANCH_MISSES;
            case E::BRANCH_INSTRUCTIONS: return PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
            case E::BUS_CYCLES: return PERF_COUNT_HW_BUS_CYCLES;
            case E::STALLED_CYCLES_FRONTEND: return PERF_COUNT_HW_STALLED_CYCLES_FRONTEND;
            case E::STALLED_CYCLES_BACKEND: return PERF_COUNT_HW_STALLED_CYCLES_BACKEND;
            case E::RAW: return raw_config;
            default: return 0;
        }
    }

    uint32_t eventTypeToType(PerfEventOpenTool::EventType type) {
        return (type == PerfEventOpenTool::EventType::RAW) ? PERF_TYPE_RAW : PERF_TYPE_HARDWARE;
    }
}

std::string PerfEventOpenTool::eventTypeToString(EventType type, uint64_t raw_config) {
    switch(type) {
        case EventType::CPU_CYCLES: return "CPU_CYCLES";
        case EventType::INSTRUCTIONS: return "INSTRUCTIONS";
        case EventType::CACHE_MISSES: return "CACHE_MISSES";
        case EventType::CACHE_REFERENCES: return "CACHE_REFERENCES";
        case EventType::BRANCH_MISSES: return "BRANCH_MISSES";
        case EventType::BRANCH_INSTRUCTIONS: return "BRANCH_INSTRUCTIONS";
        case EventType::BUS_CYCLES: return "BUS_CYCLES";
        case EventType::STALLED_CYCLES_FRONTEND: return "STALLED_CYCLES_FRONTEND";
        case EventType::STALLED_CYCLES_BACKEND: return "STALLED_CYCLES_BACKEND";
        case EventType::RAW: return "RAW_" + std::to_string(raw_config);
        default: return "UNKNOWN";
    }
}

PerfEventOpenTool::PerfEventOpenTool(EventType event, uint64_t raw_config) {
    openEvents({event}, {raw_config});
}

PerfEventOpenTool::PerfEventOpenTool(const std::vector<EventType>& events, const std::vector<uint64_t>& raw_configs) {
    openEvents(events, raw_configs);
}

/**
 * @brief 构造函数，直接监测任意perf_event事件
 * @param perf_type 事件类型（如PERF_TYPE_HARDWARE、PERF_TYPE_HW_CACHE、PERF_TYPE_RAW等）
 *        perf_type决定了你要监控的事件类别，常见取值有：
 *        - PERF_TYPE_HARDWARE：通用硬件事件（如CPU周期、指令数等）
 *        - PERF_TYPE_SOFTWARE：软件事件（如上下文切换、页面错误等）
 *        - PERF_TYPE_HW_CACHE：硬件cache相关事件（如L1/L2/LLC等cache的访问/未命中等）
 *        - PERF_TYPE_RAW：原始事件，需要配合PMU文档指定config
 *        详见 /usr/include/linux/perf_event.h 中 enum perf_type_id
 * @param perf_config 事件配置（具体事件编号或cache三元组等，取决于perf_type）
 *        - 对于PERF_TYPE_HARDWARE，perf_config为PERF_COUNT_HW_*，如PERF_COUNT_HW_CPU_CYCLES等
 *        - 对于PERF_TYPE_HW_CACHE，perf_config为三元组编码（cache类型 | 操作类型<<8 | 结果类型<<16）
 *          例如：L1D读未命中 = PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)
 *        - 对于PERF_TYPE_RAW，perf_config为PMU原始事件编码
 *        详见 /usr/include/linux/perf_event.h 中相关enum定义
 */
PerfEventOpenTool::PerfEventOpenTool(uint32_t perf_type, uint64_t perf_config) {
    events_.clear();
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = perf_type;      // 事件类型，决定监控哪类事件
    pe.size = sizeof(struct perf_event_attr);
    pe.config = perf_config;  // 事件配置，具体事件编号或cache三元组等
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    pe.read_format = 0;
    int fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) throw std::runtime_error("perf_event_open failed");
    uint64_t id = 0;
    // ioctl是Linux系统调用，用于对设备文件（包括perf事件fd）进行各种控制操作。
    // 这里的ioctl(fd, PERF_EVENT_IOC_ID, &id)的作用是：
    //   - 向内核请求获取该perf事件的唯一ID（id），
    //   - 这样在多事件分组读取时，可以通过ID区分每个事件的计数值。
    //   - PERF_EVENT_IOC_ID是perf_event_open接口专用的命令，要求内核把该事件的唯一标识写入id指针指向的内存。
    //   - 这样后续读取分组数据时，可以通过ID匹配到具体的事件。
    ioctl(fd, PERF_EVENT_IOC_ID, &id);
    events_.push_back({fd, EventType::RAW, perf_config, id, 0});
    group_leader_fd_ = fd;
    started_ = false;
    stopped_ = false;
}

void PerfEventOpenTool::openEvents(const std::vector<EventType>& events, const std::vector<uint64_t>& raw_configs) {
    events_.clear();
    int group_fd = -1; // 分组leader的fd，单事件时为自身，多事件时第一个事件为leader
    for (size_t i = 0; i < events.size(); ++i) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = eventTypeToType(events[i]); // 事件类型（硬件/RAW）
        pe.size = sizeof(struct perf_event_attr);
        pe.config = eventTypeToConfig(events[i], (i < raw_configs.size() ? raw_configs[i] : 0)); // 事件编号
        pe.disabled = 1; // 创建时先禁用，等start时再启用
        pe.exclude_kernel = 1; // 只统计用户态
        pe.exclude_hv = 1;     // 不统计hypervisor
        // 多事件时设置分组读取格式，便于一次性读取所有事件
        if (events.size() > 1) {
            pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
        } else {
            pe.read_format = 0; // 单事件直接读取数值
        }
        // perf_event_open系统调用，返回事件fd
        int fd = perf_event_open(&pe, 0, -1, group_fd, 0);
        if (fd == -1) throw std::runtime_error("perf_event_open failed");
        uint64_t id = 0;
        ioctl(fd, PERF_EVENT_IOC_ID, &id); // 获取事件唯一id，便于分组读取时匹配
        if (group_fd == -1) group_fd = fd; // 第一个事件为分组leader
        events_.push_back({fd, events[i], (i < raw_configs.size() ? raw_configs[i] : 0), id, 0});
    }
    group_leader_fd_ = group_fd;
    started_ = false;
    stopped_ = false;
}

void PerfEventOpenTool::start() {
    if (started_) return;
    // 启动计数器：
    // 单事件时直接对fd操作，多事件时对group leader并加PERF_IOC_FLAG_GROUP
    ioctl((events_.size() == 1 ? events_[0].fd : group_leader_fd_), PERF_EVENT_IOC_RESET, (events_.size() > 1) ? PERF_IOC_FLAG_GROUP : 0); // 复位计数器
    ioctl((events_.size() == 1 ? events_[0].fd : group_leader_fd_), PERF_EVENT_IOC_ENABLE, (events_.size() > 1) ? PERF_IOC_FLAG_GROUP : 0); // 启用计数器
    started_ = true;
    stopped_ = false;
}

void PerfEventOpenTool::stop() {
    if (!started_ || stopped_) return;
    // 停止计数器
    ioctl((events_.size() == 1 ? events_[0].fd : group_leader_fd_), PERF_EVENT_IOC_DISABLE, (events_.size() > 1) ? PERF_IOC_FLAG_GROUP : 0);
    if (events_.size() == 1) {
        // 单事件直接读取计数值
        uint64_t value = 0;
        if (read(events_[0].fd, &value, sizeof(value)) == sizeof(value)) {
            events_[0].value = value;
        } else {
            events_[0].value = 0;
        }
    } else {
        // 多事件时，分组读取所有事件的计数值
        struct {
            uint64_t nr; // 事件数量
            struct { uint64_t value; uint64_t id; } values[16]; // 每个事件的值和id
        } data;
        if (read(group_leader_fd_, &data, sizeof(data)) > 0) {
            for (size_t i = 0; i < data.nr; ++i) {
                for (auto& e : events_) {
                    if (e.id == data.values[i].id) {
                        e.value = data.values[i].value;
                        break;
                    }
                }
            }
        } else {
            for (auto& e : events_) e.value = 0;
        }
    }
    stopped_ = true;
}

std::map<std::string, uint64_t> PerfEventOpenTool::getResults() const {
    std::map<std::string, uint64_t> res;
    for (const auto& e : events_) {
        res[eventTypeToString(e.type, e.raw_config)] = e.value;
    }
    return res;
}

void PerfEventOpenTool::printResults() const {
    for (const auto& e : events_) {
        std::cout << eventTypeToString(e.type, e.raw_config) << ": " << e.value << std::endl;
    }
}

void PerfEventOpenTool::logResults(const std::string& log_path) const {
    std::ofstream ofs(log_path, std::ios::app);
    for (const auto& e : events_) {
        ofs << eventTypeToString(e.type, e.raw_config) << ": " << e.value << std::endl;
    }
}

PerfEventOpenTool::~PerfEventOpenTool() {
    for (auto& e : events_) {
        if (e.fd != -1) close(e.fd);
    }
}
