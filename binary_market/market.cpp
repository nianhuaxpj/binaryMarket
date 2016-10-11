#include<winsock2.h>

#include "market.h"

#include"session_state.h"
#include <iostream>
#include <string>

#include <fstream>
#include <queue>
#include <map>

#include "utils.h"
#include <boost/asio.hpp>
#include<boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using namespace boost::asio;
using namespace rapidjson;

string build_message(int msg_type, string body)
{
	string header = int2bytes(htonl(msg_type)) + int2bytes(htonl(body.size()));
	string tail = int2bytes(htonl(generateCheckSum(header + body)));
	return header + body + tail;
}

string Market::get_logon()
{
	std::string SenderCompID = config["SenderCompID"].GetString();
	std::string TargetCompID = config["TargetCompID"].GetString();
	std::string DefaultApplVerID = config["DefaultApplVerID"].GetString();
	int HeartBtInt;
	if (config.HasMember("HeartBtInt"))
		HeartBtInt = config["HeartBtInt"].GetInt();
	string Password;
	if (config.HasMember("Password"))
		Password = config["Password"].GetString();
	string body = fill_blank(SenderCompID, 20) + fill_blank(TargetCompID, 20) + int2bytes(htonl(HeartBtInt))
		+ fill_blank(Password, 16) + fill_blank(DefaultApplVerID, 32);

	return build_message(1, body);
}

string Market::get_logout()
{
	int SessionStatus = 102;
	string Text = "Check Sum fail";
	string body = int2bytes(htonl(SessionStatus)) + fill_blank(Text, 200);
	return build_message(2, body);
}

void Market::init()
{
	ep = { ip::address_v4::from_string(config["SocketConnectHost"].GetString()),
		static_cast<unsigned short>(config["SocketConnectPort"].GetInt()) };
	Document doc = read_from_file("tag_number.json");
	for (auto& kv : doc.GetObject())
		tag_number[kv.name.GetString()] = to_string(kv.value.GetInt());
	for (auto& kv : tag_number)
	{
		cout << kv.first << ": " << kv.second << endl;
	}
}
Market::Market()
{
	config = read_from_file("binary_market.json");
	init();
}

Market::Market(const char* str)
{
	config = read_from_string(str);
	init();
}
void Market::start()
{
	while (true)
	{
		state->process();
	}
}

void Market::connect()
{
	socket.close();
	socket.connect(ep);
}

void Market::sendLogon()
{
	write(socket, buffer(get_logon()));
}

void Market::setLogoutState()
{
	state = logoutState;
}

void Market::setLogonState()
{
	state = logonState;
}

void Market::setHalfLogoutState()
{
	state = halfLogonState;
}

void Market::storeMessage(const string& data)
{
	static string message_cache;
	message_cache += data;
	while (message_cache.length() >= 12)
	{
		UINT32 msg_type = ntohl(bytes2int<UINT32>(message_cache.substr(0, 4)));
		int body_length = ntohl(bytes2int<UINT32>(message_cache.substr(4, 8)));
		if (message_cache.length() < body_length + 12)
			return;
		string message = message_cache.substr(0, body_length + 12);
		if (!is_message_valid(message))
		{
			while (!messages.empty())
				messages.pop();
			throw invalid_message("Checksum error");
		}
		messages.push(Message(msg_type, message.substr(8, body_length)));
		message_cache = message_cache.substr(body_length + 12);
	}
}

bool Market::read_orders(UINT32 msg_type)
{
	if (orders.find(msg_type) != orders.end())
		return true;

	string order_file_name = to_string(msg_type) + ".json";
	if (!boost::filesystem::exists(order_file_name))
	{
		event()->error("{} not exist", order_file_name);
		return false;
	}
	orders[msg_type] = read_from_file(order_file_name);
	return true;
}

void Market::recur(const Value& pattern, const string& body, int& pos, Document& output)
{
	if (!pattern.IsObject())
	{
		event()->error("json formmat error");
		return;
	}
	for (auto& obj : pattern.GetObject())
	{
		const string name = tag_number[obj.name.GetString()];
		if (obj.value.IsString())
		{
			string type = obj.value.GetString();
			if (type.substr(0, 4) == "char")
			{
				int size = stoi(type.substr(4));
				output.AddMember(Value(name.c_str(), output.GetAllocator()).Move(),
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
				if (type.find('_') != string::npos)
				{
					string sv = to_string(value);
					if (value)
					{
						int fraction_size = stoi(type.substr(type.find('_') + 1));
						for (int i = sv.size(); i < fraction_size + 1; ++i)
							sv = "0" + sv;
						sv.insert(sv.length() - fraction_size, ".");
					}
					output.AddMember(Value(name.c_str(), output.GetAllocator()).Move(),
						Value(sv.c_str(), output.GetAllocator()).Move(), output.GetAllocator());
				}
				else
					output.AddMember(Value(name.c_str(), output.GetAllocator()).Move(),
						Value(to_string(value).c_str(), output.GetAllocator()).Move(),
						output.GetAllocator());
				pos += size;
			}
			else
			{
				console()->error("{} type is not recognized", name);
				break;
			}
		}

		else if (obj.value.IsObject())
		{
			UINT32 group = ntohl(bytes2int<UINT32>(body.substr(pos, 4)));
			pos += 4;
			string group_name = name;
			output.AddMember(Value(group_name.c_str(), output.GetAllocator()).Move(),
				Value(kArrayType), output.GetAllocator());
			for (int i = 0; i < group; ++i)
			{
				Document one(kObjectType);
				recur(obj.value, body, pos, one);
				output[group_name.c_str()].PushBack(Value(one, output.GetAllocator()).Move(),
					output.GetAllocator());
			}
		}

		else
		{
			console()->error("json formmat error");
			return;
		}
	}
}
static string  msgtype_adapt(const string& number)
{
	if (boost::ends_with(number, "11"))
		return "W";
	else if (boost::ends_with(number, "92"))
		return "UA201";
	else if (boost::ends_with(number, "91"))
		return "UA202";
	else
		return number;
}
static Document adapt(Document&& input, string msg_type)
{
	Document output(kObjectType);
	Value v1(kArrayType);
	v1.PushBack(input, output.GetAllocator());
	Value v2(kObjectType);
	v2.AddMember("changes", v1, output.GetAllocator());
	v2.AddMember("type", Value(msgtype_adapt(msg_type).c_str(), output.GetAllocator()).Move(), output.GetAllocator());
	Value v3(kObjectType);
	v3.AddMember("producer", "szfast", output.GetAllocator());
	v3.AddMember("data", v2, output.GetAllocator());
	v3.AddMember("format", Value(msgtype_adapt(msg_type).c_str(), output.GetAllocator()).Move(), output.GetAllocator());

	output.AddMember("Code", "0", output.GetAllocator());
	output.AddMember("Msg", v3, output.GetAllocator());

	return output;
}
void Market::process_message(const Message& message)
{
	Document origin(kObjectType);
	const Document& pattern = orders[message.msg_type];
	int pos = 0;
	recur(pattern, message.body, pos, origin);
	Document rs = adapt(move(origin), to_string(message.msg_type));

	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	rs.Accept(writer);
	logger()->info(buffer.GetString());
}