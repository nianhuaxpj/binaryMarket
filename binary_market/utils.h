#pragma once
#include<string>
#include "spdlog/spdlog.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

std::shared_ptr<spdlog::logger> console();
std::shared_ptr<spdlog::logger> logger();
std::shared_ptr<spdlog::logger> event();

template<typename T> std::string int2bytes(T i)
{
	return std::string((char*)&i, sizeof(T));
}

template<typename T> T bytes2int(const std::string& s)
{
	return *(T*)s.c_str();
}

bool is_little_endian();
std::string& fill_blank(std::string& s, int n);
std::string wipe_blank(const std::string& s);
UINT32 generateCheckSum(const std::string& s);
bool is_message_valid(const std::string& message);
rapidjson::Document read_from_file(const std::string& filename);
rapidjson::Document read_from_string(const std::string& str);


struct invalid_message : public std::exception
{
	std::string s;
	invalid_message(const std::string& ss = "") : s(ss) {}
	const char* what() const throw() { return s.c_str(); }
};
