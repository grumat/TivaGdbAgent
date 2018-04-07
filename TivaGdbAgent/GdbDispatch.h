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
	virtual ~CGdbStateMachine();

	enum GDB_STATE { GDB_IDLE, GDB_PAYLOAD, GDB_CSUM1, GDB_CSUM2 };
	enum
	{
		MSGSIZE = 8192,
	};

public:
	operator const BYTE *() const { return (const BYTE *)m_Buffer.data(); }
	operator const char *() const { return m_Buffer.data(); }
	size_t GetCount() const { return m_Buffer.size(); }
	//! Returns a escaped string of buffer contents
	CAtlString GetPrintableString() const;

	void ParseAndDispatch(const char *pBuf, size_t len);
	DWORD GetThreadErrorState() const { return m_Handler.OnGetThreadErrorState(); }

protected:
	//! Prepare buffer and dispatch data to other link
	void Dispatch();
	//! Let override intercept packet buffer before sending to other link
	virtual void OnBeforeDispatch() {/* Nothing to do at the base*/ }

protected:
	//! A target to receive parsed GDB data
	IGdbDispatch &m_Handler;
	//! Current state machine status
	GDB_STATE m_eState;
	//! Data buffer
	std::string m_Buffer;
	//! Payload packet start
	size_t m_iStart;
	//! Checksum
	BYTE m_ChkSum;
};

