#include "syspulse/cli.hpp"
#include "syspulse/exporter.hpp"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>
#include <unistd.h>
namespace syspulse {
namespace {
volatile std::sig_atomic_t running=1;
void stop(int){running=0;}
void help(){std::cout<<"SysPulse 1.0.0 - Linux system monitor\nUsage: syspulse <summary|cpu|memory|processes|watch|export> [options]\n\nCommands:\n  summary                 Show system summary\n  cpu                     Show CPU utilization (first sample is 0 until a delta exists)\n  memory                  Show RAM and swap statistics\n  processes [--sort cpu|memory] [--limit N] [--pid N] [--name TEXT] [--state S]\n  watch [--interval SEC] [--limit N]  Refresh system/process view\n  export --format csv|json --output FILE  Export one snapshot\n  --help, -h              Show help\n  --version               Show version\n";}
bool uint_arg(const std::string&s,unsigned long&v){try{std::size_t n=0;v=std::stoul(s,&n);return n==s.size()&&v>0;}catch(...){return false;}}
void processes(Snapshot&s,const std::string&sort,unsigned long limit,int pid,const std::string&name,char state){
 auto& p=s.processes;
 if(pid>0)p.erase(std::remove_if(p.begin(),p.end(),[&](const Process&x){return x.pid!=pid;}),p.end());
 if(!name.empty())p.erase(std::remove_if(p.begin(),p.end(),[&](const Process&x){return x.name.find(name)==std::string::npos;}),p.end());
 if(state!='?')p.erase(std::remove_if(p.begin(),p.end(),[&](const Process&x){return x.state!=state;}),p.end());
 if(sort=="cpu")std::sort(p.begin(),p.end(),[](auto&a,auto&b){return a.cpu_percent>b.cpu_percent;});
 else if(sort=="memory")std::sort(p.begin(),p.end(),[](auto&a,auto&b){return a.rss_bytes>b.rss_bytes;});
 if(p.size()>limit)p.resize(limit);
 std::cout<<std::left<<std::setw(8)<<"PID"<<std::setw(24)<<"NAME"<<std::setw(7)<<"STATE"<<std::setw(10)<<"CPU%"<<std::setw(12)<<"RSS"<<std::setw(8)<<"THREADS"<<"COMMAND\n";
 for(const auto&x:p)std::cout<<std::left<<std::setw(8)<<x.pid<<std::setw(24)<<x.name.substr(0,22)<<std::setw(7)<<x.state<<std::setw(10)<<std::fixed<<std::setprecision(1)<<x.cpu_percent<<std::setw(12)<<human_bytes(x.rss_bytes)<<std::setw(8)<<x.threads<<x.command.substr(0,80)<<'\n';
}
void summary(const Snapshot&s){std::cout<<"SysPulse system summary\nHost: "<<s.system.hostname<<"\nKernel: "<<s.system.kernel<<"\nUptime: "<<std::fixed<<std::setprecision(0)<<s.system.uptime_seconds<<" seconds\nLogical CPUs: "<<s.system.logical_cpus<<"\nLoad average: "<<s.system.load1<<" "<<s.system.load5<<" "<<s.system.load15<<"\nCPU usage: "<<s.cpu.total_percent<<"%\nRAM: "<<human_bytes(s.memory.total_kib*1024)<<" total, "<<human_bytes(s.memory.used_kib*1024)<<" used ("<<s.memory.percent<<"%)\nSwap: "<<human_bytes(s.memory.swap_total_kib*1024)<<" total, "<<human_bytes((s.memory.swap_total_kib>s.memory.swap_free_kib?s.memory.swap_total_kib-s.memory.swap_free_kib:0)*1024)<<" used\n";}
}
int run_cli(int argc,char**argv){
 if(argc<2){help();return 0;} std::string cmd=argv[1]; if(cmd=="--help"||cmd=="-h"||cmd=="help"){help();return 0;} if(cmd=="--version"){std::cout<<"SysPulse 1.0.0\n";return 0;}
 if(cmd!="summary"&&cmd!="cpu"&&cmd!="memory"&&cmd!="processes"&&cmd!="watch"&&cmd!="export"){std::cerr<<"error: unknown command: "<<cmd<<"\n";return 2;}
 unsigned long limit=20,interval=1; int pid=0; std::string sort="cpu",name,format,output; char state='?';
 for(int i=2;i<argc;++i){std::string a=argv[i]; auto val=[&]()->std::string{if(i+1>=argc)throw std::runtime_error("missing value for "+a);return argv[++i];};
  try {if(a=="--limit"){if(!uint_arg(val(),limit)||limit>100000)throw std::runtime_error("invalid --limit");}
   else if(a=="--interval"){if(!uint_arg(val(),interval)||interval>3600)throw std::runtime_error("invalid --interval (1..3600 seconds)");}
   else if(a=="--sort"){sort=val();if(sort!="cpu"&&sort!="memory"&&sort!="pid")throw std::runtime_error("--sort must be cpu, memory, or pid");}
   else if(a=="--pid"){unsigned long v;if(!uint_arg(val(),v)||v>2147483647UL)throw std::runtime_error("invalid --pid");pid=static_cast<int>(v);}
   else if(a=="--name")name=val();
   else if(a=="--state"){auto v=val();if(v.size()!=1 || std::string("RSDZTtXxKWPI").find(v[0])==std::string::npos)throw std::runtime_error("--state must be a recognized Linux process state");state=v[0];}
   else if(a=="--format"){format=val();if(format!="csv"&&format!="json")throw std::runtime_error("--format must be csv or json");}
   else if(a=="--output")output=val();
   else throw std::runtime_error("unknown option: "+a);
  } catch(const std::exception&e){std::cerr<<"error: "<<e.what()<<"\n";return 2;}
 }
 if(cmd=="export"){if(format.empty()||output.empty()){std::cerr<<"error: export requires --format csv|json and --output FILE\n";return 2;} auto s=collect_snapshot();std::string err;bool ok=format=="csv"?export_csv(s,output,err):export_json(s,output,err);if(!ok){std::cerr<<"error: "<<err<<"\n";return 1;}std::cout<<"Exported "<<format<<" snapshot to "<<output<<"\n";return 0;}
 if(cmd=="watch"){std::signal(SIGINT,stop);std::signal(SIGTERM,stop);std::vector<Process> previous; auto last=std::chrono::steady_clock::now();while(running){auto s=collect_snapshot();auto now=std::chrono::steady_clock::now();double elapsed=std::chrono::duration<double>(now-last).count();update_process_cpu(s.processes,previous,elapsed,sysconf(_SC_CLK_TCK));previous=s.processes;last=now;std::cout<<"\033[2J\033[H";summary(s);processes(s,sort,limit,pid,name,state);std::cout<<"\nRefreshing every "<<interval<<" second(s); press Ctrl+C to stop.\n"<<std::flush;for(unsigned long n=0;n<interval*10&&running;++n)std::this_thread::sleep_for(std::chrono::milliseconds(100));}std::cout<<"\nStopped.\n";return 0;}
 auto s=collect_snapshot();
 if(cmd=="summary"){summary(s);return 0;}
 if(cmd=="cpu"){std::cout<<"CPU utilization: "<<s.cpu.total_percent<<"%\n";for(auto&c:s.cpu.cores)std::cout<<c.name<<": "<<c.percent<<"%\n";return 0;}
 if(cmd=="memory"){std::cout<<"RAM total: "<<human_bytes(s.memory.total_kib*1024)<<"\nRAM available: "<<human_bytes(s.memory.available_kib*1024)<<"\nRAM used: "<<human_bytes(s.memory.used_kib*1024)<<" ("<<s.memory.percent<<"%)\nBuffers/cache: "<<human_bytes((s.memory.buffers_kib+s.memory.cached_kib)*1024)<<"\nSwap total: "<<human_bytes(s.memory.swap_total_kib*1024)<<"\nSwap free: "<<human_bytes(s.memory.swap_free_kib*1024)<<"\n";return 0;}
 if(sort=="pid") std::sort(s.processes.begin(),s.processes.end(),[](const auto&a,const auto&b){return a.pid<b.pid;});
 processes(s,sort,limit,pid,name,state); return 0;
}
}
