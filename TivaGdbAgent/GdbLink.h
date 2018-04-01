#pragma once
#include "GdbDispatch.h"


class CGdbLink : public IGdbDispatch
{
public:
	CGdbLink(IGdbDispatch &link_handler);
	~CGdbLink();

public:
	void Serve(int iPort);

protected:
	void Listen(unsigned int iPort);
	bool IsListening() const { return sdAccept > 0; }
	void DoGdb();
	int ReadGdbMessage();
	const BYTE *GetMessage() const
	{
		return m_Message.data();
	}
	void Close()
	{
		closesocket(sdAccept);
		sdAccept = 0;
	}

	void HandleData(CGdbStateMachine &gdbCtx) override;

protected:
	SOCKET sdAccept;
	std::vector<BYTE> m_Message;
	CGdbStateMachine m_Ctx;
};
