#include"utils.h"
#include<string>
#include<numeric>
#include<fstream>
#include<winsock2.h>



using std::string;
//#pragma comment(lib,"ws2_32.lib")

std::once_flag once;
std::shared_ptr<spdlog::logger> console()
{
	static auto cons = spdlog::stdout_logger_mt("console", true);
	std::call_once(once, [] {cons->set_pattern("[%T.%e] %v"); });
	return cons;
}

std::shared_ptr<spdlog::logger> logger()
{
	static auto my_logger = spdlog::basic_logger_mt("logger", "log.txt");
	std::call_once(once, [] {my_logger->set_pattern("[%Y-%M-%d %T.%e] [%l] %v"); });
	return my_logger;
}

std::shared_ptr<spdlog::logger> event()
{
	static auto my_logger = spdlog::basic_logger_mt("event", "event.txt");
	std::call_once(once, [] {my_logger->set_pattern("[%Y-%M-%d %T.%e] [%l] %v"); });
	return my_logger;
}

// 本机大端返回1，小端返回0
bool is_little_endian()
{
	int num = 42;
	return (*(char *)&num == 42);
}



string& fill_blank(string& s, int n)
{
	int fill_size = n - s.size();
	for (int i = 0; i < fill_size; ++i)
		s += " ";
	return s;
}

string wipe_blank(const string& s)
{
	auto i = s.find_last_not_of(' ');
	if (i != string::npos)
		return s.substr(0, i + 1);
	else
		return s;
}

UINT32 generateCheckSum(const string& s)
{
	return std::accumulate(s.begin(), s.end(), 0U) % 256;
}

bool is_message_valid(const string& message)
{
	return generateCheckSum(message.substr(0, message.length() - 4)) == ntohl(bytes2int<UINT32>(message.substr(message.length() - 4)));
}

rapidjson::Document read_from_string(const string& str)
{
	rapidjson::Document document;
	document.Parse(str.c_str());
	return document;
}

rapidjson::Document read_from_file(const string& filename)
{
	using namespace rapidjson;
	std::ifstream stream(filename);
	string json;
	string line;
	while (stream >> line)
		json += line;
	rapidjson::Document document;

	document.Parse(json.c_str());
	return document;
}

