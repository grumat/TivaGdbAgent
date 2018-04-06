#include "stdafx.h"
#include "GdbDispatch.h"
#include "TheLogger.h"

#define OPT_CUT_NULS	1

using namespace Logger;


CGdbStateMachine::CGdbStateMachine(IGdbDispatch &handler)
	: m_Handler(handler)
{
	m_eState = GDB_IDLE;
	m_Buffer.reserve(MSGSIZE);
	m_iStart = 0;
	m_ChkSum = 0;
}


CGdbStateMachine::~CGdbStateMachine()
{
}


void CGdbStateMachine::Dispatch()
{
	OnBeforeDispatch();
	m_Handler.HandleData(*this);
	m_Buffer.resize(0);
	m_iStart = 0;
	m_eState = GDB_IDLE;
}


static int hexchartoi(char c)
{
	if ((c >= '0') && (c <= '9'))
	{
		return c - '0';
	}

	if ((c >= 'a') && (c <= 'f'))
	{
		return c - 'a' + 10;
	}

	if ((c >= 'A') && (c <= 'F'))
	{
		return c - 'A' + 10;
	}

	return 0;
}


void CGdbStateMachine::ParseAndDispatch(const char *pBuf, size_t len)
{
	// Can't continue on errors
	if (GetThreadErrorState() != 0)
		return;

	size_t nAckCount = 0;
	size_t nNakCount = 0;

	CAtlString hex;
	CAtlString ascii;
	BYTE ch;
	while (len--)
	{
		ch = *pBuf++;
		switch (m_eState)
		{
		case GDB_IDLE:
#if OPT_CUT_NULS
			if(ch == 0)
			{
				m_Buffer.resize(m_iStart);
				break;
			}
#endif
			Debug(_T("  GDB_IDLE: '%hc'\n"), isprint(ch) ? ch : '.');
			m_Buffer.push_back(ch);
			m_iStart = m_Buffer.size();
			if (ch == '$')
			{
				--m_iStart;	// where to cut an invalid packet
				m_eState = GDB_PAYLOAD;
				hex.Empty();
				ascii.Empty();
			}
			else if (ch == '+')
				nAckCount++;
			else if (ch == '-')
				nNakCount++;
			else if (ch == 0x03)
			{
				/* GDB Ctrl-C */
				Dispatch();
			}
			else if(ch != 0)
				nNakCount++;
			break;
		case GDB_PAYLOAD:
#if OPT_CUT_NULS
			if (ch == 0)
			{
				// Somehow NUL's arrive here. Just cut and preserve what is valid until now
				m_Buffer.resize(m_iStart);
				m_eState = GDB_IDLE;
				break;
			}
#endif
			if(IsDebugLevel())
			{
				ascii += (char)(isprint(ch) ? ch : '.');
				if (!hex.IsEmpty())
					hex += _T(' ');
				hex.AppendFormat(_T("%02x"), ch);
				if (ascii.GetLength() >= 16)
				{
					Debug(_T("  GDB_PAYLOAD: %-48s  %s\n"), (LPCTSTR)hex, (LPCTSTR)ascii);
					hex.Empty();
					ascii.Empty();
				}
			}

			m_Buffer.push_back(ch);
			if (ch == '#')
			{
				if (IsDebugLevel())
				{
					if (!hex.IsEmpty())
					{
						Debug(_T("  GDB_PAYLOAD: %-48s  %s\n"), (LPCTSTR)hex, (LPCTSTR)ascii);
						hex.Empty();
						ascii.Empty();
					}
				}
				m_eState = GDB_CSUM1;
			}
			break;
		case GDB_CSUM1:
			Debug(_T("  GDB_CSUM1: '%hc'\n"), ch);
			m_ChkSum = hexchartoi(ch) << 4;
			m_eState = GDB_CSUM2;
			m_Buffer.push_back(ch);
			break;
		case GDB_CSUM2:
			Debug(_T("  GDB_CSUM2: '%hc'\n"), ch);
			m_ChkSum |= hexchartoi(ch);
			m_Buffer.push_back(ch);
			// Transmit the packet to the other device
			Dispatch();
			break;
		}
	}
	// Still data on buffer in Idle state
	if(m_Buffer.size() != 0 && m_eState == GDB_IDLE)
	{
		if(nNakCount)
		{
			m_Buffer[0] = '-';
			m_Buffer.resize(1);
		}
		else if(nAckCount)
		{
			m_Buffer[0] = '+';
			m_Buffer.resize(1);
		}
		Dispatch();
	}
}

