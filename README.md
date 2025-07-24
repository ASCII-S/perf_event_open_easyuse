# PerfEventOpen EasyUse

> 基于 Linux perf_event_open 的 C++ 硬件性能计数器工具

## 项目简介

PerfEventOpen EasyUse 是一个基于 Linux 内核 perf_event_open 系统调用的 C++ 性能计数器工具类，支持像 `std::chrono` 一样简单地在关键代码段前后插桩，精准统计 CPU 周期数、指令数、Cache Miss 等硬件事件。支持单事件和多事件分组统计，结果可输出到标准输出或日志文件，适合 HPC 程序优化、系统性能分析、教学演示等场景。

## 主要特性
- **易用接口**：类 chrono 风格，start/stop 即可统计
- **多事件分组**：支持同时统计多项硬件事件，便于分析 IPC、miss 率等
- **标准输出/日志**：结果可直接打印或写入日志文件
- **Doxygen 注释**：代码自带详细注释，便于二次开发和学习
- **原生 Linux 支持**：无第三方依赖，直接调用内核接口
- **适用范围广**：HPC、系统优化、微基准、教学等

## 快速开始

### 依赖
- Linux 内核 2.6.31 及以上（需支持 perf_event_open）
- g++ 或 clang++ 支持 C++11 及以上

### 编译
直接使用 Makefile：
```bash
make
```
或手动编译：
```bash
g++ demo.cpp perf_event_open_tool_class.cpp -o demo
```

### 示例代码
详见本项目中的 [demo.cpp](./demo.cpp)

## 典型用法
- 单事件统计
- 多事件分组统计
- 日志输出
- 原始事件（RAW）采集

详见 [demo.cpp](./demo.cpp) 示例。

## 支持的事件类型
- CPU_CYCLES
- INSTRUCTIONS
- CACHE_MISSES
- CACHE_REFERENCES
- BRANCH_MISSES
- BRANCH_INSTRUCTIONS
- BUS_CYCLES
- STALLED_CYCLES_FRONTEND
- STALLED_CYCLES_BACKEND
- RAW（自定义event code，需查阅CPU手册）
- 也支持PERF_TYPE_SOFTWARE、PERF_TYPE_HW_CACHE等自定义类型

## 事件支持性说明
- **不同CPU/内核/平台支持的事件不同**，部分cache/tlb事件（如L1I、ITLB）在部分平台上不可用。
- 建议用 `perf list` 查看本机支持的事件，用 `perf stat -e <event> <cmd>` 验证事件能否采集。
- 工具遇到不支持的事件会抛出异常，建议用try-catch捕获并友好提示。

## 常见问题与建议
- **事件不支持**：部分事件（如L1I/ITLB）在部分平台上不支持，采集时会报错。建议只采集本机支持的事件。
- **异常处理**：建议用try-catch捕获`perf_event_open`失败，输出友好提示。
- **跨平台兼容**：不同CPU/内核/虚拟化环境支持的事件差异大，建议动态检测和容错。
- **性能计数器数量有限**：一次分组采集事件数不宜过多，超出硬件支持会失败。
- **更多用法**：详见 [demo.cpp](./demo.cpp) 和头文件注释。

## 适用场景
- HPC/科学计算程序优化
- 系统/内核/驱动性能分析
- 算法微基准测试
- 教学演示与课程实验
- 自动化性能回归测试

## 参考资料/致谢
- [man 2 perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)
- [LinuxEvents C++模板](https://gist.github.com/lemire/7838881cc01df5023bb55a2653f793bc)
- [perf_event_open ARM官方教程](https://learn.arm.com/learning-paths/servers-and-cloud-computing/arm_pmu/perf_event_open/)
- [perf-cpp](https://github.com/jmuehlig/perf-cpp)

## License
MIT
