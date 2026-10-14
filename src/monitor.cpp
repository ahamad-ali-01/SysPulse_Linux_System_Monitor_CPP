#include "syspulse/monitor.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <thread>
#include <sys/utsname.h>
#include <unistd.h>

namespace syspulse {
namespace {
std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream out;
    out << f.rdbuf();
    return out.str();
}
std::string now_string() {
    const auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
    return out.str();
}
bool parse_u64(const std::string& s, std::uint64_t& out) {
    try {
        std::size_t used = 0;
        auto value = std::stoull(s, &used);
        if (used != s.size()) return false;
        out = value;
        return true;
    } catch (...) { return false; }
}
struct CpuCounters {
    std::string name;
    std::uint64_t total = 0;
    std::uint64_t idle = 0;
};
std::vector<CpuCounters> parse_cpu_counters(const std::string& text) {
    std::vector<CpuCounters> out;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream row(line);
        CpuCounters c;
        if (!(row >> c.name)) continue;
        if (c.name.rfind("cpu", 0) != 0) {
            if (!out.empty()) break;
            continue;
        }
        if (c.name.size() > 3 && !std::all_of(c.name.begin() + 3, c.name.end(),
                [](unsigned char ch) { return ch >= '0' && ch <= '9'; })) {
            if (c.name != "cpu") continue;
        }
        std::vector<std::uint64_t> values;
        std::string token;
        bool malformed = false;
        while (row >> token) {
            std::uint64_t value = 0;
            if (!parse_u64(token, value)) { malformed = true; break; }
            values.push_back(value);
        }
        if (malformed || values.size() < 4) continue;
        std::uint64_t total = 0;
        bool overflow = false;
        for (auto value : values) {
            if (value > UINT64_MAX - total) { overflow = true; break; }
            total += value;
        }
        if (overflow) continue;
        const auto iowait = values.size() > 4 ? values[4] : 0;
        if (iowait > UINT64_MAX - values[3]) continue;
        out.push_back({c.name, total, values[3] + iowait});
    }
    return out;
}
double utilization(std::uint64_t total, std::uint64_t idle,
                   std::uint64_t old_total, std::uint64_t old_idle) {
    if (total < old_total || idle < old_idle || total == old_total) return 0.0;
    const auto total_delta = total - old_total;
    const auto idle_delta = idle - old_idle;
    if (idle_delta > total_delta) return 0.0;
    const double pct = 100.0 * static_cast<double>(total_delta - idle_delta) /
                       static_cast<double>(total_delta);
    return std::clamp(pct, 0.0, 100.0);
}
}
bool parse_process_stat(const std::string& line, Process& p, long page_size, long /*clock_ticks*/) {
    const auto left = line.find('(');
    const auto right = line.rfind(')');
    if (left == std::string::npos || right == std::string::npos ||
        right <= left || right + 2 >= line.size() || page_size <= 0) return false;

    int parsed_pid = 0;
    try {
        const std::string pid_part = line.substr(0, left);
        std::size_t used = 0;
        parsed_pid = std::stoi(pid_part, &used);
        if (parsed_pid <= 0) return false;
        for (std::size_t i = used; i < pid_part.size(); ++i)
            if (!std::isspace(static_cast<unsigned char>(pid_part[i]))) return false;
    } catch (...) { return false; }

    std::istringstream fields_stream(line.substr(right + 1));
    std::vector<std::string> fields;
    std::string token;
    while (fields_stream >> token) fields.push_back(token);
    // fields[0] is state (proc stat field 3), fields[21] is RSS (field 24).
    if (fields.size() < 22 || fields[0].size() != 1) return false;

    Process parsed;
    parsed.pid = parsed_pid;
    parsed.name = line.substr(left + 1, right - left - 1);
    try {
        parsed.state = fields[0][0];
        parsed.ppid = std::stoi(fields[1]);
        parsed.utime = std::stoull(fields[11]);
        parsed.stime = std::stoull(fields[12]);
        parsed.priority = std::stoi(fields[15]);
        parsed.nice = std::stoi(fields[16]);
        parsed.threads = std::stoi(fields[17]);
        parsed.start_ticks = std::stoull(fields[19]);
        parsed.vsize_bytes = std::stoull(fields[20]);
        const auto rss_pages = std::stoll(fields[21]);
        if (rss_pages > 0) {
            const auto pages = static_cast<std::uint64_t>(rss_pages);
            const auto page_size_u = static_cast<std::uint64_t>(page_size);
            if (pages > UINT64_MAX / page_size_u) return false;
            parsed.rss_bytes = pages * page_size_u;
        } else {
            parsed.rss_bytes = 0;
        }
    } catch (...) { return false; }
    p = std::move(parsed);
    return true;
}

CpuStats parse_cpu_stat(const std::string& current_text, const std::string& previous_text) {
    CpuStats result;
    const auto current = parse_cpu_counters(current_text);
    const auto previous = parse_cpu_counters(previous_text);
    for (const auto& c : current) {
        const auto it = std::find_if(previous.begin(), previous.end(),
            [&](const CpuCounters& old) { return old.name == c.name; });
        const double pct = it == previous.end() ? 0.0 :
            utilization(c.total, c.idle, it->total, it->idle);
        if (c.name == "cpu") result.total_percent = pct;
        else result.cores.push_back({c.name, pct});
    }
    return result;
}

CpuStats sample_cpu() {
    static std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> previous;
    CpuStats result;
    const auto text = read_file("/proc/stat");
    const auto counters = parse_cpu_counters(text);
    for (const auto& c : counters) {
        auto it = previous.find(c.name);
        const double pct = it == previous.end() ? 0.0 :
            utilization(c.total, c.idle, it->second.first, it->second.second);
        if (c.name == "cpu") result.total_percent = pct;
        else result.cores.push_back({c.name, pct});
        previous[c.name] = {c.total, c.idle};
    }
    for (auto it = previous.begin(); it != previous.end();) {
        const bool exists = std::any_of(counters.begin(), counters.end(),
            [&](const CpuCounters& c) { return c.name == it->first; });
        if (!exists) it = previous.erase(it); else ++it;
    }
    return result;
}

MemoryStats parse_meminfo(const std::string& text) {
    MemoryStats m;
    bool has_available = false;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream row(line);
        std::string key, unit;
        std::uint64_t value = 0;
        if (!(row >> key >> value)) continue;
        row >> unit;
        if (!unit.empty() && unit != "kB") continue;
        if (key == "MemTotal:") m.total_kib = value;
        else if (key == "MemAvailable:") { m.available_kib = value; has_available = true; }
        else if (key == "MemFree:") m.free_kib = value;
        else if (key == "Buffers:") m.buffers_kib = value;
        else if (key == "Cached:") m.cached_kib = value;
        else if (key == "SwapTotal:") m.swap_total_kib = value;
        else if (key == "SwapFree:") m.swap_free_kib = value;
    }
    if (!has_available) {
        const auto sum = m.free_kib + m.buffers_kib + m.cached_kib;
        m.available_kib = std::min(m.total_kib, sum < m.free_kib ? UINT64_MAX : sum);
    } else {
        m.available_kib = std::min(m.available_kib, m.total_kib);
    }
    m.used_kib = m.total_kib > m.available_kib ? m.total_kib - m.available_kib : 0;
    m.percent = m.total_kib ? 100.0 * static_cast<double>(m.used_kib) /
                              static_cast<double>(m.total_kib) : 0.0;
    return m;
}
MemoryStats read_memory() { return parse_meminfo(read_file("/proc/meminfo")); }

SystemInfo read_system_info() {
    SystemInfo s;
    s.timestamp = now_string();
    char host[256]{};
    if (gethostname(host, sizeof(host) - 1) == 0) s.hostname = host;
    struct utsname u{};
    if (uname(&u) == 0) s.kernel = std::string(u.sysname) + " " + u.release;
    std::ifstream up("/proc/uptime");
    up >> s.uptime_seconds;
    std::ifstream load("/proc/loadavg");
    load >> s.load1 >> s.load5 >> s.load15;
    s.logical_cpus = std::max(1u, std::thread::hardware_concurrency());
    return s;
}

std::vector<Process> read_processes() {
    std::vector<Process> out;
    long page_size = sysconf(_SC_PAGESIZE), ticks = sysconf(_SC_CLK_TCK);
    if (page_size <= 0) page_size = 4096;
    if (ticks <= 0) ticks = 100;
    std::error_code ec;
    std::filesystem::directory_iterator it("/proc", ec), end;
    if (ec) return out;
    for (; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        std::error_code item_ec;
        if (!it->is_directory(item_ec) || item_ec) continue;
        const auto dir_name = it->path().filename().string();
        if (dir_name.empty() || !std::all_of(dir_name.begin(), dir_name.end(),
                [](unsigned char c) { return c >= '0' && c <= '9'; })) continue;

        Process p;
        const auto base = it->path().string();
        if (!parse_process_stat(read_file(base + "/stat"), p, page_size, ticks)) continue;
        // A PID may vanish/reuse between directory enumeration and reads; validate path PID.
        try { if (std::stoi(dir_name) != p.pid) continue; } catch (...) { continue; }

        const auto status = read_file(base + "/status");
        std::istringstream si(status);
        std::string line;
        while (std::getline(si, line)) {
            if (line.rfind("Uid:", 0) == 0) {
                std::istringstream row(line.substr(4));
                unsigned uid = 0;
                if (row >> uid) p.user = std::to_string(uid);
                break;
            }
        }
        p.command = read_file(base + "/cmdline");
        std::replace(p.command.begin(), p.command.end(), '\0', ' ');
        while (!p.command.empty() && p.command.back() == ' ') p.command.pop_back();
        if (p.command.empty()) p.command = p.name;
        out.push_back(std::move(p));
    }
    return out;
}

Snapshot collect_snapshot() {
    Snapshot s;
    s.cpu = sample_cpu();
    s.memory = read_memory();
    s.system = read_system_info();
    s.processes = read_processes();
    return s;
}

void update_process_cpu(std::vector<Process>& current, const std::vector<Process>& previous,
                        double elapsed_seconds, long clock_ticks) {
    if (elapsed_seconds <= 0.0 || clock_ticks <= 0) return;
    for (auto& p : current) {
        const auto it = std::find_if(previous.begin(), previous.end(), [&](const Process& old) {
            return old.pid == p.pid && old.start_ticks == p.start_ticks;
        });
        if (it == previous.end()) continue;
        const auto old_ticks = it->utime + it->stime;
        const auto new_ticks = p.utime + p.stime;
        if (new_ticks < old_ticks) { p.cpu_percent = 0.0; continue; }
        const auto delta = new_ticks - old_ticks;
        p.cpu_percent = 100.0 * static_cast<double>(delta) /
                        static_cast<double>(clock_ticks) / elapsed_seconds;
        if (!std::isfinite(p.cpu_percent) || p.cpu_percent < 0.0) p.cpu_percent = 0.0;
    }
}

std::string human_bytes(std::uint64_t bytes) {
    std::ostringstream out;
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) { value /= 1024.0; ++unit; }
    out << std::fixed << std::setprecision(unit ? 1 : 0) << value << ' ' << units[unit];
    return out.str();
}
}
