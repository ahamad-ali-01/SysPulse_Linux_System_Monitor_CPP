#pragma once
#include "syspulse/monitor.hpp"
#include <string>
namespace syspulse {
bool export_csv(const Snapshot&, const std::string& path, std::string& error);
bool export_json(const Snapshot&, const std::string& path, std::string& error);
std::string json_escape(const std::string&);
std::string csv_escape(const std::string&);
}
