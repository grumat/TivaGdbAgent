#include "stdafx.h"
#include "GdbLink.h"
#include "TheLogger.h"
#include "TivaIcdi.h"

#pragma comment(lib, "Ws2_32")

using namespace Logger;


static void cleanup()
{
	WSACleanup();
}


CGdbLink::CGdbLink()
{
	static bool init = false;
	sdAccept = 0;
	if(!init)
	{
		WORD wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(2, 2);
		if (WSAStartup(wVersionRequested, &wsaData))
		{
			DWORD dw = WSAGetLastError();
			Error(_T("Call to socket() failed.\n"));
			AtlThrow(HRESULT_FROM_WIN32(dw));
		}
		atexit(cleanup);

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			/* Tell the user that we could not find a usable */
			/* WinSock DLL.                                  */
			Error(_T("Could not find a usable version of Winsock.dll\n"));
			AtlThrow(HRESULT_FROM_WIN32(WSAESOCKTNOSUPPORT));
		}
		else
			Debug(_T("The Winsock 2.2 dll was found okay\n"));
	}
	// Allocate buffer
	m_Message.resize(GDBCTX::MSGSIZE);
}


CGdbLink::~CGdbLink()
{
	if (IsListening())
		Close();
}


//
// Wait for a connection on iPort and return the 
// 
void CGdbLink::Listen(unsigned int iPort)
{
	int addrlen;
	struct   sockaddr_in sin;
	struct   sockaddr_in pin;
	int so_reuseaddr = 1;
	int sdListen;

	//
	// Open an internet socket 
	//
	if ((sdListen = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to socket() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// Attempt to set SO_REUSEADDR to avoid "address already in use" errors
	setsockopt(sdListen,
			   SOL_SOCKET,
			   SO_REUSEADDR,
			   (const char *)&so_reuseaddr,
			   sizeof(so_reuseaddr));

	//
	// Set up to listen on all interfaces/addresses and port iPort
	//
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(iPort);

	//
	// Bind out file descriptor to the port/address
	//
	Info(_T("Bind to port %d\n"), iPort);
	if (bind(sdListen, (struct sockaddr *) &sin, sizeof(sin)) == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to bind() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	//
	// Put the file descriptor in a state where it can listen for an 
	// incoming connection
	//
	Info(_T("Listen\n"));
	if (listen(sdListen, 1) == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to listen() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// 
	// Accept() will block until someone connects.  In return it gives
	// us a new file descriptor back.
	//
	Info(_T("Accept...\n"));
	addrlen = sizeof(pin);
	if ((sdAccept = accept(sdListen, (struct sockaddr *)  &pin, &addrlen)) == -1)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to accept() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	//
	// Close the file descriptor that we used for listening
	//
	closesocket(sdListen);
}


int CGdbLink::ReadGdbMessage()
{
	WSAPOLLFD fdarray[1];
	fdarray[0].fd = sdAccept;
	fdarray[0].events = POLLIN;
	fdarray[0].revents = POLLRDNORM | POLLRDBAND;
	int res = WSAPoll(fdarray, 1, 10);
	if (res == SOCKET_ERROR)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to WSAPoll() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}
	// Receive data
	int rx = recv(sdAccept, (char*)m_Message.data(), GDBCTX::MSGSIZE, 0);
	if(rx == SOCKET_ERROR)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to recv() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}
	Debug(_T("%hs: recv returned %d\n"), __FUNCTION__, (int)rx);
	// 
	// if we RX 0 bytes it usually means that the other 
	// side closed the connection
	//
	return rx;
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


void CGdbLink::StateMachine(IGdbDispatch *iface)
{
	CAtlString hex;
	CAtlString ascii;
	int len = m_Message.size();
	const BYTE *pBuf = m_Message.data();
	while (len--)
	{
		switch (m_Ctx.gdb_state)
		{
		case GDBCTX::GDB_IDLE:
			Debug(_T("GDB_IDLE: '%c'\n"), *pBuf);
			if (*pBuf == '$')
			{
				m_Ctx.gdb_state = GDBCTX::GDB_PAYLOAD;
				hex.Empty();
				ascii.Empty();
			}
			if (*pBuf == '+')
			{
				m_Ctx.iAckCount++;
			}
			if (*pBuf == '-')
			{
				m_Ctx.iNakCount++;
			}
			m_Ctx.pResp[m_Ctx.iRd++] = *pBuf;
			if (*pBuf == 0x03)
			{
				/* GDB Ctrl-C */
				if (iface)
				{
					iface->HandleData(m_Ctx, 1);
					m_Ctx.iRd = 0;
				}
			}
			pBuf++;
			break;
		case GDBCTX::GDB_PAYLOAD:
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

			m_Ctx.pResp[m_Ctx.iRd++] = *pBuf;
			if (*pBuf == '#')
			{
				if (!hex.IsEmpty())
				{
					Debug(_T("GDB_PAYLOAD: %-48s  %s\n"), (LPCTSTR)hex, (LPCTSTR)ascii);
					hex.Empty();
					ascii.Empty();
				}
				m_Ctx.gdb_state = GDBCTX::GDB_CSUM1;
			}
			pBuf++;
			break;
		case GDBCTX::GDB_CSUM1:
			Debug(_T("GDB_CSUM1: '%c'\n"), *pBuf);
			m_Ctx.csum = hexchartoi(*pBuf) << 4;
			m_Ctx.gdb_state = GDBCTX::GDB_CSUM2;
			m_Ctx.pResp[m_Ctx.iRd++] = *pBuf;
			pBuf++;
			break;
		case GDBCTX::GDB_CSUM2:
			Debug(_T("GDB_CSUM2: '%c'\n"), *pBuf);
			m_Ctx.csum |= hexchartoi(*pBuf);
			m_Ctx.pResp[m_Ctx.iRd++] = *pBuf;
			if (iface)
			{
				if (gdb_validate(m_Ctx.pResp, m_Ctx.csum) == 0)
				{
					iface->HandleData(m_Ctx, 1);
				}
				else
				{
					iface->HandleData(m_Ctx, 0);
				}
			}
			m_Ctx.iRd = 0;
			m_Ctx.gdb_state = GDBCTX::GDB_IDLE;
			pBuf++;
			break;
		}
	}
}


void CGdbLink::DoGdb(IGdbDispatch *iface)
{
	while (1)
	{
		int rx = ReadGdbMessage();
		if (rx == 0)
		{
			// 
			// if we RX 0 bytes it usually means that the other 
			// side closed the connection
			//
			break;
		}
		m_Message.resize(rx);
		StateMachine(iface);
	} // (while 1)
}


void CGdbLink::Serve(int iPort, IGdbDispatch *iface)
{
	while (1)
	{
		Listen(iPort);
		if (IsListening())
		{
			try
			{
				//
				// Do the bridging between the socket and the usb bulk device
				//
				DoGdb(iface);
			}
			catch (...)
			{
				Close();
				throw;
			}
			Close();
		}
	}
}


void CGdbLink::HandleData(BYTE *buf, size_t count)
{
	//
	// Process whatever data we've RX'ed by invoking the GDB state
	// machine.  When a complete GDB packet has been RX'ed the state
	// machine will call gdb_packet_from_usb.
	//
	gdb_statemachine(pTrans->user_data, pTrans->buffer,
					 pTrans->actual_length, gdb_packet_from_usb);

}

