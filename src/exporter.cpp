#include "syspulse/exporter.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
namespace syspulse {
std::string json_escape(const std::string& s) {
 std::ostringstream o; for(unsigned char c:s) { switch(c) {case '"':o<<"\\\"";break;case '\\':o<<"\\\\";break;case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;default: if(c<32)o<<"?";else o<<static_cast<char>(c);} } return o.str();
}
std::string csv_escape(const std::string& s) {
 if(s.find_first_of(",\"\r\n")==std::string::npos) return s;
 std::string r="\""; for(char c:s){if(c=='"')r+="\"\"";else r+=c;} return r+"\"";
}
bool export_csv(const Snapshot& s,const std::string& path,std::string& error) {
 std::ofstream f(path,std::ios::out|std::ios::trunc); if(!f){error="cannot open output file: "+path;return false;}
 f<<"timestamp,hostname,cpu_percent,mem_total_kib,mem_used_kib,swap_total_kib,swap_free_kib,load1,load5,load15,pid,ppid,name,state,cpu_process_percent,rss_bytes,threads,command\n";
 if(s.processes.empty()) f<<csv_escape(s.system.timestamp)<<','<<csv_escape(s.system.hostname)<<','<<s.cpu.total_percent<<','<<s.memory.total_kib<<','<<s.memory.used_kib<<','<<s.memory.swap_total_kib<<','<<s.memory.swap_free_kib<<','<<s.system.load1<<','<<s.system.load5<<','<<s.system.load15<<",,,,,,,,\n";
 for(const auto&p:s.processes) f<<csv_escape(s.system.timestamp)<<','<<csv_escape(s.system.hostname)<<','<<s.cpu.total_percent<<','<<s.memory.total_kib<<','<<s.memory.used_kib<<','<<s.memory.swap_total_kib<<','<<s.memory.swap_free_kib<<','<<s.system.load1<<','<<s.system.load5<<','<<s.system.load15<<','<<p.pid<<','<<p.ppid<<','<<csv_escape(p.name)<<','<<p.state<<','<<p.cpu_percent<<','<<p.rss_bytes<<','<<p.threads<<','<<csv_escape(p.command)<<'\n';
 if(!f){error="failed while writing output file: "+path;return false;} return true;
}
bool export_json(const Snapshot&s,const std::string& path,std::string& error) {
 std::ofstream f(path,std::ios::out|std::ios::trunc); if(!f){error="cannot open output file: "+path;return false;}
 f<<std::fixed<<std::setprecision(2)<<"{\n \"timestamp\":\""<<json_escape(s.system.timestamp)<<"\",\n \"system\":{\"hostname\":\""<<json_escape(s.system.hostname)<<"\",\"kernel\":\""<<json_escape(s.system.kernel)<<"\",\"uptime_seconds\":"<<s.system.uptime_seconds<<",\"load_average\":["<<s.system.load1<<','<<s.system.load5<<','<<s.system.load15<<"],\"logical_cpus\":"<<s.system.logical_cpus<<"},\n";
 f<<" \"cpu\":{\"total_percent\":"<<s.cpu.total_percent<<",\"cores\":[";
 for(std::size_t i=0;i<s.cpu.cores.size();++i){if(i)f<<',';f<<"{\"name\":\""<<json_escape(s.cpu.cores[i].name)<<"\",\"percent\":"<<s.cpu.cores[i].percent<<'}';} f<<"]},\n";
 f<<" \"memory\":{\"total_kib\":"<<s.memory.total_kib<<",\"available_kib\":"<<s.memory.available_kib<<",\"free_kib\":"<<s.memory.free_kib<<",\"used_kib\":"<<s.memory.used_kib<<",\"percent\":"<<s.memory.percent<<",\"swap_total_kib\":"<<s.memory.swap_total_kib<<",\"swap_free_kib\":"<<s.memory.swap_free_kib<<"},\n \"processes\":[";
 for(std::size_t i=0;i<s.processes.size();++i){const auto&p=s.processes[i];if(i)f<<',';f<<"{\"pid\":"<<p.pid<<",\"ppid\":"<<p.ppid<<",\"name\":\""<<json_escape(p.name)<<"\",\"state\":\""<<p.state<<"\",\"cpu_percent\":"<<p.cpu_percent<<",\"rss_bytes\":"<<p.rss_bytes<<",\"threads\":"<<p.threads<<",\"command\":\""<<json_escape(p.command)<<"\"}";}
 f<<"]\n}\n"; if(!f){error="failed while writing output file: "+path;return false;} return true;
}
}
