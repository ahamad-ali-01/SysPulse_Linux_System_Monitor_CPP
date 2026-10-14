#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace syspulse {
struct Process {
 int pid=0, ppid=0, threads=0, priority=0, nice=0;
 char state='?';
 std::string name, user, command;
 std::uint64_t utime=0, stime=0, vsize_bytes=0, rss_bytes=0, start_ticks=0;
 double cpu_percent=0.0;
};
struct CpuCore { std::string name; double percent=0.0; };
struct CpuStats { double total_percent=0.0; std::vector<CpuCore> cores; };
struct MemoryStats {
 std::uint64_t total_kib=0, available_kib=0, free_kib=0, used_kib=0;
 std::uint64_t buffers_kib=0, cached_kib=0, swap_total_kib=0, swap_free_kib=0;
 double percent=0.0;
};
struct SystemInfo {
 std::string hostname, kernel, timestamp;
 double uptime_seconds=0.0;
 double load1=0.0, load5=0.0, load15=0.0;
 unsigned logical_cpus=0;
};
struct Snapshot { CpuStats cpu; MemoryStats memory; SystemInfo system; std::vector<Process> processes; };
bool parse_process_stat(const std::string&, Process&, long page_size, long clock_ticks);
CpuStats sample_cpu();
// Deterministic parsers used by the procfs reader and fixture-based tests.
CpuStats parse_cpu_stat(const std::string& current_text, const std::string& previous_text = {});
MemoryStats parse_meminfo(const std::string& text);
MemoryStats read_memory();
SystemInfo read_system_info();
std::vector<Process> read_processes();
Snapshot collect_snapshot();
void update_process_cpu(std::vector<Process>& current, const std::vector<Process>& previous, double elapsed_seconds, long clock_ticks);
std::string human_bytes(std::uint64_t bytes);
}
