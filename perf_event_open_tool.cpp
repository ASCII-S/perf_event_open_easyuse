#include "perf_event_open_tool.h"
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

PerfEventOpenTool::PerfEventOpenTool(const std::vector<uint32_t>& perf_types, const std::vector<uint64_t>& perf_configs) {
    events_.clear();
    int group_fd = -1;
    for (size_t i = 0; i < perf_types.size(); ++i) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = perf_types[i];
        pe.size = sizeof(struct perf_event_attr);
        pe.config = (i < perf_configs.size() ? perf_configs[i] : 0);
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        pe.read_format = (perf_types.size() > 1) ? (PERF_FORMAT_GROUP | PERF_FORMAT_ID) : 0;
        int fd = perf_event_open(&pe, 0, -1, group_fd, 0);
        if (fd == -1) throw std::runtime_error("perf_event_open failed");
        uint64_t id = 0;
        ioctl(fd, PERF_EVENT_IOC_ID, &id);
        if (group_fd == -1) group_fd = fd;
        events_.push_back({fd, EventType::RAW, (i < perf_configs.size() ? perf_configs[i] : 0), id, 0});
    }
    group_leader_fd_ = group_fd;
    started_ = false;
    stopped_ = false;
}

PerfEventOpenTool::PerfEventOpenTool(uint32_t perf_type, uint64_t perf_config) :
    PerfEventOpenTool(std::vector<uint32_t>{perf_type}, std::vector<uint64_t>{perf_config}) {}

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
    started_ = false;
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
