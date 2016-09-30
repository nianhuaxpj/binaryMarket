#include"session_state.h"
#include"utils.h"
#include"market.h"


using namespace std;
using namespace rapidjson;

SessionState::SessionState(Market* market_) :market(market_)
{
}
HalfLogonState::HalfLogonState(Market* market) : SessionState(market)
{
}

LogonState::LogonState(Market* market) : SessionState(market)
{
}

LogoutState::LogoutState(Market* market) : SessionState(market)
{
}

void LogoutState::process()
{
	console()->info("LogoutState::process");
	market->connect();
	market->sendLogon();
	market->setHalfLogoutState();
}

void HalfLogonState::process()
{
	console()->info("HalfLogonState::process");
	for (;;)
	{
		while (!market->messages.empty())
		{
			Message expected_logon = move(market->messages.front());
			market->messages.pop();
			if (expected_logon.msg_type == 1)
			{
				console()->info("receive logon");
				market->setLogonState();
				return;
			}
		}
		char buf[1500];
		size_t len = market->socket.read_some(boost::asio::buffer(buf, 1500));
		try {
			market->storeMessage(string(buf, len));
		}
		catch (const invalid_message& err)
		{
			console()->error(err.what());
			market->setLogoutState();
			return;
		}
	}
}


void LogonState::process()
{
	console()->info("LogonState::process");
	for (;;)
	{
		while (!market->messages.empty())
		{
			Message message = move(market->messages.front());
			market->messages.pop();
			console()->info("msg_type: {}", message.msg_type);
			if (!market->read_orders(message.msg_type))
				continue;
			market->process_message(message);
		}
		char buf[1500];
		size_t len = market->socket.read_some(boost::asio::buffer(buf, 1500));
		try {
			market->storeMessage(string(buf, len));
		}
		catch (const invalid_message& err)
		{
			console()->error(err.what());
			market->setLogoutState();
			return;
		}
	}
}