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
	sdListen = 0;
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
}


CGdbLink::~CGdbLink()
{
	Close();
}


//
// Wait for a connection on iPort and return the 
// 
void CGdbLink::Listen(unsigned int iPort)
{
	struct   sockaddr_in sin;
	int so_reuseaddr = 1;

	// Before attempting any network I/O check for parallel thread state
	DWORD err = m_Ctx.GetThreadErrorState();
	if(err)
	{
		Error(_T("USB thread failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(err));
	}

	//
	// Open an internet socket 
	//
	if ((sdListen = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
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

	DWORD dwFlags = 0;
	DWORD dwBytes;
	LPFN_ACCEPTEX lpfnAcceptEx = NULL;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;

	// Load the AcceptEx function into memory using WSAIoctl.
	// The WSAIoctl function is an extension of the ioctlsocket()
	// function that can use overlapped I/O. The function's 3rd
	// through 6th parameters are input and output buffers where
	// we pass the pointer to our AcceptEx function. This is used
	// so that we can call the AcceptEx function directly, rather
	// than refer to the Mswsock.lib library.
	int iResult = WSAIoctl(sdListen, SIO_GET_EXTENSION_FUNCTION_POINTER,
					   &GuidAcceptEx, sizeof(GuidAcceptEx),
					   &lpfnAcceptEx, sizeof(lpfnAcceptEx),
					   &dwBytes, NULL, NULL);
	if (iResult == SOCKET_ERROR)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to WSAIoctl() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// Create an accepting socket
	sdAccept = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sdAccept == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Create accept socket failed!\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// Empty our overlapped structure and accept connections.
	WSAOVERLAPPED op;
	memset(&op, 0, sizeof(op));
	op.hEvent = WSACreateEvent();
	if(op.hEvent == WSA_INVALID_EVENT)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to WSACreateEvent() failed!\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	char lpOutputBuf[2 * (sizeof(SOCKADDR_IN) + 16)];

	BOOL bRetVal = lpfnAcceptEx(sdListen, sdAccept, lpOutputBuf,
						   0,
						   sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
						   &dwBytes, &op);
	err = 0;
	if (bRetVal == FALSE)
	{
		err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
			Error(_T("Call to AcceptEx() failed!\n"));
		else
		{
			err = 0;
			// Wait until a connection comes in
			while (err == 0)
			{
				// Failures on the other thread cancels wait
				err = m_Ctx.GetThreadErrorState();
				if (err == 0)
				{
					DWORD dw = WaitForSingleObject(op.hEvent, 100);
					// Continue waiting until connection arrives
					if (dw == WAIT_TIMEOUT)
						continue;
					WSAResetEvent(op.hEvent);
					// Connection arrived: break wait loop
					if (dw == WAIT_OBJECT_0)
					{
						if (WSAGetOverlappedResult(sdListen, &op, &dwBytes, FALSE, &dwFlags) == FALSE)
						{
							err = WSAGetLastError();
							Error(_T("Call to WSAGetOverlappedResult() failed!\n"));
						}
						break;
					}
					// Copy error to be later handled
					err = dw;
					Error(_T("Error waiting for incomming connection!\n"));
				}
				else
					Error(_T("USB thread failed.\n"));
			}
		}
	}
	// Free local stuff
	WSACloseEvent(op.hEvent);

	//
	// Close the file descriptor that we used for listening
	//
	closesocket(sdListen);
	sdListen = 0;

	// Handle pending errors
	if (err)
	{
		// Cancel overlapped I/O
		closesocket(sdAccept);
		sdAccept = 0;
		AtlThrow(HRESULT_FROM_WIN32(err));
	}
}


void CGdbLink::Close()
{
	if (sdListen)
	{
		closesocket(sdListen);
		sdListen = 0;
	}
	if (sdAccept)
	{
		closesocket(sdAccept);
		sdAccept = 0;
	}
}


bool CGdbLink::ReadGdbMessage()
{
	DWORD err = 0;
	DWORD dwFlags, dwBytes;

	WSAOVERLAPPED op;
	memset(&op, 0, sizeof(op));
	op.hEvent = WSACreateEvent();
	if (op.hEvent == WSA_INVALID_EVENT)
	{
		err = WSAGetLastError();
		Error(_T("Call to WSACreateEvent() failed!\n"));
		AtlThrow(HRESULT_FROM_WIN32(err));
	}

	WSABUF bufs;
	// Allocate buffer
	m_Message.resize(CGdbStateMachine::MSGSIZE);
	bufs.buf = (char*)m_Message.data();
	bufs.len = CGdbStateMachine::MSGSIZE;

	dwFlags = MSG_PARTIAL;
	dwBytes = 0;
	// Start data Receive
	if (WSARecv(sdAccept, &bufs, 1, &dwBytes, &dwFlags, &op, NULL) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		if(err == WSA_IO_PENDING)
		{
			err = 0;
			while (err == 0)
			{
				err = m_Ctx.GetThreadErrorState();
				if (err)
					Error(_T("USB thread failed.\n"));
				else
				{
					DWORD dw = WaitForSingleObject(op.hEvent, 100);
					// Continue waiting until connection arrives
					if (dw == WAIT_TIMEOUT)
						continue;
					WSAResetEvent(op.hEvent);
					// Connection arrived: break wait loop
					if (dw == WAIT_OBJECT_0)
					{
						if (WSAGetOverlappedResult(sdAccept, &op, &dwBytes, FALSE, &dwFlags) != FALSE)
						{
							m_Message.resize(dwBytes);
							Info(_T("%d Bytes received, dwFlags = %d\n"), dwBytes, dwFlags);
						}
						else
						{
							err = WSAGetLastError();
							Error(_T("Call to WSAGetOverlappedResult() failed with error %d!\n"), err);
						}
						break;
					}
					// Copy error to be later handled
					err = dw;
					Error(_T("Call to WaitForSingleObject() failed with error %d!\n"), err);
				}
			}
		}
		else if(err)
		{
			Error(_T("Call to WSARecv() failed with error %d!\n"), err);
		}
	}
	// Free local stuff
	WSACloseEvent(op.hEvent);

	if (err == WSAECONNABORTED || dwBytes == 0)
		return false;

	if(err)
		AtlThrow(HRESULT_FROM_WIN32(err));
	return true;
}


void CGdbLink::DoGdb()
{
	while (1)
	{
		if (!ReadGdbMessage())
			break;
		const BYTE *pBuf = m_Message.data();
		m_Ctx.ParseAndDispatch((const char *)pBuf, m_Message.size());
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

	Info(_T("%-22hs: GDB <-- ICDI: '%hs'\n"), __FUNCTION__, (const char *)gdbCtx);
	send(sdAccept, gdbCtx, (int)gdbCtx.GetCount(), 0);
}


DWORD CGdbLink::OnGetThreadErrorState() const
{
	return 0;
}

