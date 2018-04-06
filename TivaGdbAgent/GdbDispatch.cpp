#include "stdafx.h"
#include "GdbDispatch.h"
#include "TheLogger.h"

#define OPT_CUT_NULS	1

using namespace Logger;


CGdbStateMachine::CGdbStateMachine(IGdbDispatch &handler)
	: m_Handler(handler)
{
	m_eState = GDB_IDLE;
	m_pData = new BYTE[MSGSIZE];
	m_iStart = m_iRd = 0;
	m_ChkSum = 0;
}


CGdbStateMachine::~CGdbStateMachine()
{
	delete[] m_pData;
}


void CGdbStateMachine::Dispatch()
{
	m_pData[m_iRd] = 0;	// nul-terminates strings
	m_Handler.HandleData(*this);
	m_iStart = m_iRd = 0;
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
				m_iRd = m_iStart;
				break;
			}
#endif
			Debug(_T("  GDB_IDLE: '%hc'\n"), isprint(ch) ? ch : '.');
			m_pData[m_iRd++] = ch;
			m_iStart = m_iRd;
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
				m_iRd = m_iStart;
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

			m_pData[m_iRd++] = ch;
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
			m_pData[m_iRd++] = ch;
			break;
		case GDB_CSUM2:
			Debug(_T("  GDB_CSUM2: '%hc'\n"), ch);
			m_ChkSum |= hexchartoi(ch);
			m_pData[m_iRd++] = ch;
			// Transmit the packet to the other device
			Dispatch();
			break;
		}
	}
	// Still data on buffer in Idle state
	if(m_iRd && m_eState == GDB_IDLE)
	{
		if(nNakCount)
		{
			m_pData[0] = '-';
			m_iRd = 1;
		}
		else if(nAckCount)
		{
			m_pData[0] = '+';
			m_iRd = 1;
		}
		Dispatch();
	}
}

