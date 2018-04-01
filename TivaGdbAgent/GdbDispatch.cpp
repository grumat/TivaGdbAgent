#include "stdafx.h"
#include "GdbDispatch.h"
#include "TheLogger.h"


using namespace Logger;


CGdbStateMachine::CGdbStateMachine(IGdbDispatch &handler)
	: m_Handler(handler)
{
	m_eState = GDB_IDLE;
	m_pData = new BYTE[MSGSIZE];
	m_iRd = 0;
	m_ChkSum = 0;
	m_nAckCount = 0;
	m_nNakCount = 0;
}


CGdbStateMachine::~CGdbStateMachine()
{
	delete[] m_pData;
}


void CGdbStateMachine::Dispatch(bool fValid)
{
	m_pData[m_iRd] = 0;	// nul-terminates strings
	m_Handler.HandleData(*this);
}


static int hexchartoi(char c)
{
	if ((c >= '0') && (c <= '9'))
	{
		return '0' - c;
	}

	if ((c >= 'a') && (c <= 'f'))
	{
		return 'a' - c + 10;
	}

	if ((c >= 'a') && (c <= 'f'))
	{
		return 'A' - c + 10;
	}

	return 0;
}


static int gdb_validate(const BYTE *pRes, BYTE csum)
{
	//
	// TODO: Take payload and calculate checksum...
	// 
	return 0;
}


void CGdbStateMachine::Dispatch(const BYTE *pBuf, size_t len)
{
	CAtlString hex;
	CAtlString ascii;
	while (len--)
	{
		switch (m_eState)
		{
		case GDB_IDLE:
			Debug(_T("GDB_IDLE: '%c'\n"), *pBuf);
			if (*pBuf == '$')
			{
				m_eState = GDB_PAYLOAD;
				hex.Empty();
				ascii.Empty();
			}
			if (*pBuf == '+')
			{
				m_nAckCount++;
			}
			if (*pBuf == '-')
			{
				m_nNakCount++;
			}
			m_pData[m_iRd++] = *pBuf;
			if (*pBuf == 0x03)
			{
				/* GDB Ctrl-C */
				Dispatch();
				m_iRd = 0;
			}
			pBuf++;
			break;
		case GDB_PAYLOAD:
			ascii += (TCHAR)(isprint(*pBuf) ? *pBuf : '.');
			if (!hex.IsEmpty())
				hex += _T(' ');
			hex.AppendFormat(_T("%02x"), *pBuf);
			if (ascii.GetLength() >= 16)
			{
				Debug(_T("GDB_PAYLOAD: %-48s  %s\n"), (LPCTSTR)hex, (LPCTSTR)ascii);
				hex.Empty();
				ascii.Empty();
			}

			m_pData[m_iRd++] = *pBuf;
			if (*pBuf == '#')
			{
				if (!hex.IsEmpty())
				{
					Debug(_T("GDB_PAYLOAD: %-48s  %s\n"), (LPCTSTR)hex, (LPCTSTR)ascii);
					hex.Empty();
					ascii.Empty();
				}
				m_eState = GDB_CSUM1;
			}
			pBuf++;
			break;
		case GDB_CSUM1:
			Debug(_T("GDB_CSUM1: '%c'\n"), *pBuf);
			m_ChkSum = hexchartoi(*pBuf) << 4;
			m_eState = GDB_CSUM2;
			m_pData[m_iRd++] = *pBuf;
			pBuf++;
			break;
		case GDB_CSUM2:
			Debug(_T("GDB_CSUM2: '%c'\n"), *pBuf);
			m_ChkSum |= hexchartoi(*pBuf);
			m_pData[m_iRd++] = *pBuf;
			if (gdb_validate(m_pData, m_ChkSum) == 0)
			{
				Dispatch();
			}
			else
			{
				Dispatch(false);
			}
			m_iRd = 0;
			m_eState = GDB_IDLE;
			pBuf++;
			break;
		}
	}
}

