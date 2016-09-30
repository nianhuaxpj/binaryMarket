#pragma once
#include"session_state.h"
#include<memory>
#include<map>
#include<queue>
#include<string>
#include <boost/asio.hpp>
#include<boost/filesystem.hpp>
#include "rapidjson/document.h"

struct Message
{
	UINT32 msg_type;
	std::string body;
	Message(UINT32 type, const std::string& body_) :msg_type(type), body(body_) {}
};

class Market
{
	friend class LogoutState;
	friend class HalfLogonState;
	friend class LogonState;

public:
	Market();
	void start();

private:
	void connect();
	void sendLogon();
	void setLogoutState();
	void setLogonState();
	void setHalfLogoutState();
	void storeMessage(const std::string& data);
	bool read_orders(UINT32 msg_type);
	void process_message(const Message& message);
	std::string Market::get_logon();
	std::string Market::get_logout();

	std::shared_ptr<SessionState>  logoutState { std::make_shared<LogoutState>(this) };
	std::shared_ptr<SessionState>  logonState{ std::make_shared<LogonState>(this) };
	std::shared_ptr<SessionState>  halfLogonState{ std::make_shared<HalfLogonState>(this) };
	std::shared_ptr<SessionState> state{ logoutState };

	boost::asio::io_service iosev;
	boost::asio::ip::tcp::socket socket{ iosev };
	boost::asio::ip::tcp::endpoint ep;
	std::queue<Message> messages;
	std::map<UINT32, rapidjson::Document> orders;
	rapidjson::Document config;
};
