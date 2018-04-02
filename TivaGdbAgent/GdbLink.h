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
	enum RdState_e
	{
		Abort,
		Waiting,
		Sent,
	};
	RdState_e ReadGdbMessage();
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
	DWORD OnGetThreadErrorState() const override;

protected:
	SOCKET sdAccept;
	std::vector<BYTE> m_Message;
	CGdbStateMachine m_Ctx;
	struct Xmit
	{
		int count;
		char *buffer;
	};
	std::vector<Xmit> m_XmitQueue;
	CComCriticalSection m_XmitQueueLock;
};
