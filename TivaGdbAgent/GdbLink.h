#pragma once
#include "GdbDispatch.h"


class CGdbLink : public IUsbDispatch
{
public:
	CGdbLink();
	~CGdbLink();

public:
	void Serve(int iPort, IGdbDispatch *iface);

protected:
	void Listen(unsigned int iPort);
	bool IsListening() const { return sdAccept > 0; }
	void DoGdb(IGdbDispatch *iface);
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
	void StateMachine(IGdbDispatch *iface);

	void HandleData(BYTE *buf, size_t count) override;

protected:
	int sdAccept;
	std::vector<BYTE> m_Message;
	GDBCTX m_Ctx;
};
