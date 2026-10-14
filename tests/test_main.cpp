#include "syspulse/monitor.hpp"
#include "syspulse/exporter.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace syspulse;
int main() {
    int failed = 0, passed = 0;
    auto check = [&](bool ok, const char* name) {
        if (ok) { ++passed; std::cout << "PASS " << name << "\n"; }
        else { ++failed; std::cerr << "FAIL " << name << "\n"; }
    };
    Process p;
    // Fields after state correspond to proc stat fields 4..; create a complete fixture.
    const std::string stat = "123 (a process (worker)) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 2 18 19 4096 3";
    check(parse_process_stat(stat, p, 4096, 100) && p.pid == 123 &&
          p.name == "a process (worker)" && p.ppid == 1 && p.state == 'S',
          "process stat with spaces and parentheses");
    check(p.utime == 11 && p.stime == 12 && p.threads == 2 &&
          p.vsize_bytes == 4096 && p.rss_bytes == 12288,
          "process stat field mapping and RSS pages");
    check(!parse_process_stat("bad record", p, 4096, 100), "reject malformed stat");
    check(!parse_process_stat("0 (bad) S 1 2", p, 4096, 100), "reject invalid PID/short stat");
    const std::string huge_rss = "123 (overflow) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 2 18 19 4096 9223372036854775807";
    check(!parse_process_stat(huge_rss, p, std::numeric_limits<long>::max(), 100),
          "reject RSS byte multiplication overflow");
    check(human_bytes(1024) == "1.0 KiB" && human_bytes(1024*1024) == "1.0 MiB",
          "human-readable byte units");
    check(csv_escape("a,b") == "\"a,b\"" && csv_escape("a\"b") == "\"a\"\"b\"",
          "CSV escaping");
    check(json_escape("a\"b") == "a\\\"b" && json_escape("a\nb") == "a\\nb",
          "JSON escaping");
    const std::string cpu1 = "cpu  100 0 50 850 0 0 0 0\ncpu0 50 0 25 425 0 0 0 0\n";
    auto first = parse_cpu_stat(cpu1);
    check(first.total_percent == 0.0 && first.cores.size() == 1,
          "CPU parser accepts fixture and baseline");
    const std::string cpu_bad = "cpu 1 2 3 nope\ncpu0 1 2 3 4\n";
    check(parse_cpu_stat(cpu_bad).cores.size() == 1,
          "CPU parser skips malformed counter row");
    const std::string mem = "MemTotal: 1000 kB\nMemAvailable: 250 kB\nMemFree: 100 kB\nBuffers: 20 kB\nCached: 80 kB\nSwapTotal: 500 kB\nSwapFree: 300 kB\n";
    auto m = parse_meminfo(mem);
    check(m.total_kib == 1000 && m.available_kib == 250 && m.used_kib == 750 &&
          m.swap_total_kib == 500 && m.swap_free_kib == 300,
          "meminfo units and accounting");
    auto fallback = parse_meminfo("MemTotal: 1000 kB\nMemFree: 100 kB\nBuffers: 20 kB\nCached: 80 kB\n");
    check(fallback.available_kib == 200 && fallback.used_kib == 800,
          "MemAvailable fallback");
    auto zero_available = parse_meminfo("MemTotal: 1000 kB\nMemAvailable: 0 kB\nMemFree: 100 kB\n");
    check(zero_available.available_kib == 0 && zero_available.used_kib == 1000,
          "zero MemAvailable remains zero");
    auto live_cpu = sample_cpu();
    check(live_cpu.total_percent >= 0 && live_cpu.total_percent <= 100,
          "live CPU sampling bounded");
    auto live_mem = read_memory();
    check(live_mem.total_kib >= live_mem.available_kib && live_mem.percent >= 0 &&
          live_mem.percent <= 100, "live memory values sane");
    auto sys = read_system_info();
    check(!sys.hostname.empty() && !sys.kernel.empty() && sys.logical_cpus > 0,
          "system info fields");
    auto procs = read_processes();
    check(!procs.empty(), "live process enumeration");
    bool valid = true;
    for (const auto& proc : procs) if (proc.pid <= 0 || proc.name.empty()) valid = false;
    check(valid, "enumerated process records are valid");
    Process oldp; oldp.pid=42; oldp.start_ticks=100; oldp.utime=10; oldp.stime=10;
    Process newp=oldp; newp.utime=12; newp.stime=11;
    std::vector<Process> cur{newp}, prev{oldp};
    update_process_cpu(cur, prev, 1.0, 100);
    check(cur[0].cpu_percent >= 0.0 && cur[0].cpu_percent < 100.0,
          "process CPU delta calculation");
    newp.utime=1; newp.stime=1; cur={newp};
    update_process_cpu(cur, prev, 1.0, 100);
    check(cur[0].cpu_percent == 0.0, "process CPU counter regression handled");
    Snapshot snapshot = collect_snapshot();
    std::string error;
    const std::string json_path = "/tmp/syspulse-audit-test.json";
    const std::string csv_path = "/tmp/syspulse-audit-test.csv";
    check(export_json(snapshot, json_path, error), "JSON export writes file");
    check(export_csv(snapshot, csv_path, error), "CSV export writes file");
    std::ifstream jf(json_path); std::string json((std::istreambuf_iterator<char>(jf)), {});
    std::ifstream cf(csv_path); std::string csv((std::istreambuf_iterator<char>(cf)), {});
    check(json.find("\"processes\":[") != std::string::npos &&
          json.find("\"memory\":{") != std::string::npos, "JSON structure contains required sections");
    check(csv.find("timestamp,hostname,cpu_percent") == 0 &&
          csv.find("\n") != std::string::npos, "CSV header and rows");
    std::remove(json_path.c_str()); std::remove(csv_path.c_str());
    std::string ignored;
    check(!export_json(snapshot, "/no-such-dir/syspulse.json", ignored),
          "export reports unwritable path");
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
