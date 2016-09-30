#pragma once
#include<string>
class Market;
class SessionState
{
public:
	SessionState(Market*);
	virtual void process()=0;
	virtual void send(const std::string& message) {};
protected:
	Market* market;
};

class LogoutState : public SessionState
{
public:
	LogoutState(Market*);
	void process() override;
};

class HalfLogonState : public SessionState
{
public:
	HalfLogonState(Market*);
	void process() override;
};

class LogonState : public SessionState
{
public:
	LogonState(Market*);
	void process() override;
};