#pragma once


class CGdbStateMachine;
class IGdbDispatch
{
public:
	virtual void HandleData(CGdbStateMachine &gdbCtx) = 0;
	virtual DWORD OnGetThreadErrorState() const = 0;
};


class CGdbStateMachine
{
public:
	CGdbStateMachine(IGdbDispatch &handler);
	~CGdbStateMachine();

	enum GDB_STATE { GDB_IDLE, GDB_PAYLOAD, GDB_CSUM1, GDB_CSUM2 };
	enum
	{
		MSGSIZE = 8192,
	};

public:
	operator const BYTE *() const { return m_pData; }
	operator const char *() const { return (const char *)m_pData; }
	size_t GetCount() const { return m_iRd; }

	void ParseAndDispatch(const char *pBuf, size_t len);
	DWORD GetThreadErrorState() const { return m_Handler.OnGetThreadErrorState(); }

protected:
	void Dispatch();

protected:
	//! A target to receive parsed GDB data
	IGdbDispatch &m_Handler;
	//! Current state machine status
	GDB_STATE m_eState;
	//! Pointer to the data
	BYTE *m_pData;
	//! Valid bytes on buffer
	size_t m_iRd;
	//! Payload packet start
	size_t m_iStart;
	//! Checksum
	BYTE m_ChkSum;
};

