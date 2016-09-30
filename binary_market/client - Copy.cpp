#include <iostream>
#include <string>
#include <fstream>
#include <queue>
#include <map>
#include<winsock2.h>
#include <boost/asio.hpp>
#include<boost/filesystem.hpp>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "utils.h"
#include"session_state.h"

//#pragma comment(lib,"ws2_32.lib")

using namespace std;
using namespace rapidjson;

static string SenderCompID;
static string TargetCompID;
static int HeartBtInt = 30;
static string Password;
static string DefaultApplVerID;
static queue<string> que;

map<UINT32, Document> orders;
void static read_config()
{
	Document document = read_json_file("tgw.json");
	SenderCompID = document["SenderCompID"].GetString();
	TargetCompID = document["TargetCompID"].GetString();
	DefaultApplVerID = document["DefaultApplVerID"].GetString();
	if (document.HasMember("HeartBtInt"))
		HeartBtInt = document["HeartBtInt"].GetInt();
	if (document.HasMember("Password"))
		Password = document["Password"].GetString();
}

string get_logon()
{
	string body = fill_blank(SenderCompID, 20) + fill_blank(TargetCompID, 20) + int2bytes(htonl(HeartBtInt))
		+ fill_blank(Password, 16) + fill_blank(DefaultApplVerID, 32);
	string header = int2bytes(htonl(1)) + int2bytes(htonl(body.size()));
	string tail = int2bytes(htonl(generateCheckSum(header + body)));
	return header + body + tail;
}

string get_heartbeat()
{
	string header = int2bytes(htonl(3)) + int2bytes(htonl(0));
	string tail = int2bytes(htonl(generateCheckSum(header)));
	return header + tail;
}

void store_message(const string& data)
{
	static string message_cache;
	message_cache += data;
	cout << "message_cache: " << message_cache.length() << endl;
	while (message_cache.length() >= 12)
	{
		cout << "msg_type: " << ntohl(bytes2int<UINT32>(message_cache.substr(0, 4))) << endl;
		int body_length = ntohl(bytes2int<UINT32>(message_cache.substr(4, 8)));
		cout << "body_length: " << body_length << endl;
		if (message_cache.length() < body_length + 12)
			return;
		string message = message_cache.substr(0, body_length + 12);
		if (!is_message_valid(message))
		{
			cout << "message_invalid" << endl;
			while (!que.empty())
				que.pop();
			throw invalid_message("Checksum error");
		}
		que.push(message);
		message_cache = message_cache.substr(body_length + 12);
	}
	cout << "message_cache over: " << message_cache.length() << endl;
}

bool read_orders(UINT32 msg_type)
{
	auto i = orders.find(msg_type);
	if (i != orders.end())
		return true;

	string order_file_name = to_string(msg_type) + ".json";
	if (!boost::filesystem::exists(order_file_name))
	{
		console()->error("{} not exist", order_file_name);
		return false;
	}
	orders[msg_type] = read_json_file(order_file_name);
	console()->info("read_json_file success");
	return true;
}


void recur(const Value& pattern, const string& body, int& pos, Document& output)
{
	if (!pattern.IsObject())
	{
		event()->error("json formmat error");
		return;
	}
	for (auto& obj : pattern.GetObject())
	{
		const string& name = obj.name.GetString();
		console()->info(name);
		if (obj.value.IsString())
		{
			string type = obj.value.GetString();
			if (type.substr(0, 4) == "char")
			{
				int size = stoi(type.substr(4));
				cout << wipe_blank(body.substr(pos, size)) << endl;
				output.AddMember(StringRef(name.c_str()),
					Value(wipe_blank(body.substr(pos, size)).c_str(), output.GetAllocator()).Move(),
					output.GetAllocator());
				pos += size;
			}
			else if (type.substr(0, 3) == "Int" || type.substr(0, 4) == "uInt")
			{
				int size;
				if (type.substr(0, 3) == "Int")
					size = stoi(type.substr(3)) / 8;
				else
					size = stoi(type.substr(4)) / 8;

				UINT64 value;
				switch (size)
				{
				case 1:
					value = bytes2int<UINT8>(body.substr(pos, size));
					break;
				case 2:
					value = ntohs(bytes2int<UINT16>(body.substr(pos, size)));
					break;
				case 4:
					value = ntohl(bytes2int<UINT32>(body.substr(pos, size)));
					break;
				case 8:
					value = ntohll(bytes2int<UINT64>(body.substr(pos, size)));
				}
				cout << value << endl;
				if (type.find('_') != string::npos)
				{
					string sv = to_string(value);
					sv.insert(sv.length() - stoi(type.substr(type.find('_') + 1)), ".");
					output.AddMember(StringRef(name.c_str()),
						Value(sv.c_str(), output.GetAllocator()).Move(), output.GetAllocator());
				}
				else
					output.AddMember(StringRef(name.c_str()), value, output.GetAllocator());
				pos += size;
			}
			else
			{
				event()->error("{} type is not recognized", name);
				break;
			}
		}

		else if (obj.value.IsObject())
		{
			UINT32 group = ntohl(bytes2int<UINT32>(body.substr(pos, 4)));
			pos += 4;
			output.AddMember(StringRef(name.c_str()), Value(kArrayType), output.GetAllocator());
			for (int i = 0; i < group; ++i)
			{
				Document one(kObjectType);
				recur(obj.value, body, pos, one);
				output[name.c_str()].PushBack(one, output.GetAllocator());
			}
		}

		else
		{
			event()->error("json formmat error");
			return;
		}
	}
}

void process_message(UINT32 msg_type, const string& body)
{
	//cout << body << endl;
	Document output(kObjectType);
	const Document& pattern = orders[msg_type];
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	pattern.Accept(writer);
	console()->info("pattern: {}", buffer.GetString());
	int pos = 0;
	recur(pattern, body, pos, output);

	/*StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	output.Accept(writer);
	console()->info(buffer.GetString());*/
}

int main(int argc, char* argv[])
{
	read_config();
	boost::asio::io_service iosev;
	boost::asio::ip::tcp::socket socket(iosev);
	boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address_v4::from_string("192.168.15.104"), 9999);

	char buf[1500];
	for (;;)
	{
		static bool logon = false;
		if (!logon)
		{
			try
			{
				socket.connect(ep);
				auto send_num = write(socket, boost::asio::buffer(get_logon()));
				cout << "send num " << send_num << endl;
				while (!logon)
				{
					size_t len;
					while (!que.empty() || (len = socket.read_some(boost::asio::buffer(buf, 1500))))
					{
						console()->info(len);
						store_message(string(buf, len));
						string expected_logon = move(que.front());
						que.pop();
						UINT32 msg_type = ntohl(bytes2int<UINT32>(expected_logon.substr(0, 4)));
						if (msg_type == 1)
						{
							console()->info("receive logon");
							logon = true;
							break;
						}
					}
				}
			}
			catch (const boost::system::system_error& err)
			{
				console()->info(err.what());
				break;
			}
			catch (const invalid_message& err)
			{
				console()->info(err.what());
				break;
			}
		}

		size_t len = 0;
		while (!que.empty() || (len = socket.read_some(boost::asio::buffer(buf, 1500))))
		{
			console()->info(len);
			store_message(string(buf, len));
			while (!que.empty())
			{
				string message = move(que.front());
				que.pop();
				console()->info("message.length: {}", message.length());
				UINT32 msg_type = ntohl(bytes2int<UINT32>(message.substr(0, 4)));
				if (msg_type == 3)
					write(socket, boost::asio::buffer(get_heartbeat()));
				console()->info("msg_type: {}", msg_type);
				if (!read_orders(msg_type))
					continue;
				int body_length = ntohl(bytes2int<UINT32>(message.substr(4, 8)));
				process_message(msg_type, message.substr(8, body_length));
			}
		}
	}
	return 0;
}