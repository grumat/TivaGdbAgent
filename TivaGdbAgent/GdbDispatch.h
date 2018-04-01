#pragma once


class IGdbDispatch;


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

	void Dispatch(const BYTE *pBuf, size_t len);

protected:
	void Dispatch(bool fValid = false);

protected:
	//! A target to receive parsed GDB data
	IGdbDispatch &m_Handler;
	//! Current state machine status
	GDB_STATE m_eState;
	//! Pointer to the data
	BYTE *m_pData;
	//! Valid bytes on buffer
	size_t m_iRd;
	//! Checksum
	BYTE m_ChkSum;
	//! Count of ACK
	size_t m_nAckCount;
	//! Count of NAK
	size_t m_nNakCount;
};


class IGdbDispatch
{

public:
	virtual void HandleData(CGdbStateMachine &gdbCtx) = 0;
};

