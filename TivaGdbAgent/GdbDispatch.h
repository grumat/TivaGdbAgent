#pragma once


struct GDBCTX
{
public:
	enum GDB_STATE { GDB_IDLE, GDB_PAYLOAD, GDB_CSUM1, GDB_CSUM2 };
	enum
	{
		MSGSIZE = 8192,
	};

	GDB_STATE gdb_state;
	BYTE *pResp;
	size_t iRd;
	BYTE csum;
	size_t iAckCount;
	size_t iNakCount;

	GDBCTX()
	{
		gdb_state = GDB_IDLE;
		pResp = new BYTE[MSGSIZE];
		iRd = 0;
		csum = 0;
		iAckCount = 0;
		iNakCount = 0;
	}
	~GDBCTX()
	{
		delete[] pResp;
	}
};


class IGdbDispatch
{

public:
	virtual void HandleData(GDBCTX &gdbCtx, size_t count) = 0;
};


class IUsbDispatch
{
public:
	virtual void HandleData(BYTE *buf, size_t count) = 0;
};

