#pragma once
#include "GdbDispatch.h"


class CGdbLink : public IGdbDispatch
{
public:
	CGdbLink(IGdbDispatch &link_handler);
	virtual ~CGdbLink();

public:
	void Serve(int iPort);

protected:
	void Listen(unsigned int iPort);
	bool IsListening() const { return sdAccept > 0; }
	void DoGdb();
	bool ReadGdbMessage();
	const BYTE *GetMessage() const
	{
		return m_Message.data();
	}
	void Close();

	void HandleData(CGdbStateMachine &gdbCtx) override;
	DWORD OnGetThreadErrorState() const override;

protected:
	SOCKET sdListen;
	SOCKET sdAccept;
	std::vector<BYTE> m_Message;
	CGdbStateMachine m_Ctx;
};
