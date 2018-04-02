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


CGdbLink::CGdbLink(IGdbDispatch &link_handler)
	: m_Ctx(link_handler)
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
	m_Message.resize(CGdbStateMachine::MSGSIZE);
	m_XmitQueueLock.Init();
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
	SOCKET sdListen;

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


CGdbLink::RdState_e CGdbLink::ReadGdbMessage()
{
	// Query receive data (MSG_PEEK)
	int rx = recv(sdAccept, (char*)m_Message.data(), CGdbStateMachine::MSGSIZE, MSG_PEEK);
	if(rx <= 0)
	{
		DWORD dw = WSAGetLastError();
		if (dw == WSAECONNRESET || dw == WSAECONNABORTED)
		{
			Info(_T("Call to recv() was closed by remote (%d).\n"), dw);
			return Abort;
		}
		else if (dw)
		{
			Error(_T("Call to recv() failed.\n"));
			AtlThrow(HRESULT_FROM_WIN32(dw));
		}
	}
	// Just read data if actually present
	if (rx)
	{
		// Now read the data effectively
		rx = recv(sdAccept, (char*)m_Message.data(), CGdbStateMachine::MSGSIZE, 0);
		if (rx == SOCKET_ERROR)
		{
			DWORD dw = WSAGetLastError();
			if (dw == WSAECONNRESET || dw == WSAECONNABORTED)
			{
				Info(_T("Call to recv() was closed by remote (%d).\n"), dw);
				return Abort;
			}
			Error(_T("Call to recv() failed.\n"));
			AtlThrow(HRESULT_FROM_WIN32(dw));
		}
		Debug(_T("%hs: recv returned %d\n"), __FUNCTION__, (int)rx);
		m_Message.resize(rx);
		const BYTE *pBuf = m_Message.data();
		m_Ctx.Dispatch(pBuf, rx);
		return Sent;
	}
	return Waiting;
}


void CGdbLink::DoGdb()
{
	while (1)
	{
		DWORD err = m_Ctx.GetThreadErrorState();
		if(err)
		{
			Error(_T("USB Thread is reporting errors.\n"));
			AtlThrow(HRESULT_FROM_WIN32(err));
		}
		RdState_e eRead = ReadGdbMessage();
		if (eRead == Abort)
			break;	// remote connection was closed
		if (m_XmitQueue.size())
		{
			Xmit packet;
			// BLOCK: lock
			{
				CComCritSecLock<CComCriticalSection> lock(m_XmitQueueLock);
				packet = m_XmitQueue.front();
				m_XmitQueue.erase(m_XmitQueue.begin());
			}
			send(sdAccept, packet.buffer, packet.count, 0);
		}
		else if (eRead == Waiting)
			Sleep(0);
	} // (while 1)
}


void CGdbLink::Serve(int iPort)
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
				DoGdb();
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


void CGdbLink::HandleData(CGdbStateMachine &gdbCtx)
{
	//
	// Process whatever data we've RX'ed by invoking the GDB state
	// machine.  When a complete GDB packet has been RX'ed the state
	// machine will call gdb_packet_from_usb.
	//

	Info(_T("%hs: send to gdb: '%hs'\n"), __FUNCTION__, (const char *)gdbCtx);
	Xmit packet;
	packet.count = (int)gdbCtx.GetCount();
	packet.buffer = new char[packet.count];
	memcpy(packet.buffer, (const char *)gdbCtx, packet.count);
	CComCritSecLock<CComCriticalSection> lock(m_XmitQueueLock);
	m_XmitQueue.push_back(packet);
}


DWORD CGdbLink::OnGetThreadErrorState() const
{
	return 0;
}

