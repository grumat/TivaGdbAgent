/*
Notes:
Dump of ICDI firmware revealed the following tokens that seems to extend GDB protocol:
	- debug
		"debug clock \0"
		"debug sreset"
		"debug creset"
		"debug hreset"
		"debug disable"
	- set
		"set vectorcatch 0"
	- version
	- "X\x1c"
	- dfu-update
	- mfg
	- mode

	- disable
	- speed
	- trace
	- unlock
	- sreset
	- creset
	- hreset
	- resettype
	- vectorcatch
	- stepirq
	- xtal
*/


#include "stdafx.h"
#include "TivaIcdi.h"
#include "TheLogger.h"

using namespace Logger;

// CC5D8A93-48F4-40c0-8AA8-832720890812
const GUID CTivaIcdi::m_guidICDI = { 0xCC5D8A93L, 0x48F4, 0x40c0, { 0x8a, 0xa8, 0x83, 0x27, 0x20, 0x89, 0x08, 0x12 } };
// D17C772B-AF45-4041-9979-AAFE96BF6398
const GUID CTivaIcdi::m_guidDFU = { 0xD17C772BL, 0xAF45, 0x4041, { 0x99, 0x79, 0xaa, 0xfe, 0x96, 0xbf, 0x63, 0x98 } };
// 4D36E978-E325-11CE-BFC1-08002BE10318
const GUID CTivaIcdi::m_guidCOM = { 0x4D36E978L, 0xE325, 0x11CE,{ 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } };


CTivaIcdi::CTivaIcdi()
{
	m_AlternateSetting = 0;
	m_PipeIn = 0;
	m_PipeOut = 0;
	m_dwThreadExitCode = 0;
	m_hReadThread = NULL;
	m_fRunning = false;
	m_pStateMachine = NULL;
	m_hReady = NULL;
}


CTivaIcdi::~CTivaIcdi()
{
	Close();
}


WinUSB::String CTivaIcdi::GetTivaIcdiDevice()
{
	WinUSB::StringArray deviceNames;
	Info(_T("Opening ICDI JTAG interface\n"));
	Debug(_T("Enumerating ICDI JTAG devices...\n"));
	if(! WinUSB::CDevice::EnumerateDevices(&m_guidICDI, deviceNames))
		AtlThrowLastWin32();
	Debug(_T("  Found %d ICDI JTAG devices\n"), deviceNames.size());
	if (deviceNames.size() == 0)
	{
		Error(_T("No ICDI device found VID=%04X/PID=%04X\n"), VID, PID);
		AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
	}
	Debug(_T("Using device '%s'\n"), deviceNames[0].c_str());
	return deviceNames[0];
}


WinUSB::String CTivaIcdi::GetStringDescriptor(UCHAR index)
{
	WinUSB::String res;
	UCHAR stringDescriptor[1024];
	ULONG nTransferred = 0;
	if (m_Device.GetDescriptor(USB_STRING_DESCRIPTOR_TYPE, index, 0, stringDescriptor, sizeof(stringDescriptor), &nTransferred) != 0
		&& (nTransferred >= sizeof(USB_STRING_DESCRIPTOR)))
	{
		USB_STRING_DESCRIPTOR* pStringDescriptor = reinterpret_cast<USB_STRING_DESCRIPTOR*>(&stringDescriptor);
		pStringDescriptor->bString[(pStringDescriptor->bLength - 2) / sizeof(WCHAR)] = L'\0';
		res = CT2WEX<512>(pStringDescriptor->bString);
	}
	return res;
}



void CTivaIcdi::Open(IGdbDispatch &rReadPackets)
{
	WinUSB::String s;

	m_PipeIn = 0;
	m_PipeOut = 0;
	m_pStateMachine = new CTivaGdbStateMachine(rReadPackets);

	WinUSB::String name = GetTivaIcdiDevice();
	if (!m_Device.Initialize(name.c_str()))
	{
		DWORD dw = GetLastError();
		if (dw == E_INVALIDARG)
			Error(_T("This utility requires a WinUSB driver for the ICDI device\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// Let's query for the device descriptor
	Detail(_T("Getting device descriptor\n"));
	memset(&m_DeviceDescriptor, 0, sizeof(m_DeviceDescriptor));
	ULONG nTransferred = 0;
	if (!m_Device.GetDescriptor(USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, reinterpret_cast<PUCHAR>(&m_DeviceDescriptor), sizeof(m_DeviceDescriptor), &nTransferred))
		AtlThrowLastWin32();
	if (IsDebugLevel())
	{
		Debug(_T("  bLength: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bLength));
		Debug(_T("  bDescriptorType: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDescriptorType));
		Debug(_T("  bcdUSB: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.bcdUSB));
		Debug(_T("  bDeviceClass: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDeviceClass));
		Debug(_T("  bDeviceSubClass: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDeviceSubClass));
		Debug(_T("  bDeviceProtocol: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDeviceProtocol));
		Debug(_T("  bMaxPacketSize0: 0x%02X bytes\n"), static_cast<int>(m_DeviceDescriptor.bMaxPacketSize0));
		Debug(_T("  idVendor: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.idVendor));
		Debug(_T("  idProduct: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.idProduct));
		Debug(_T("  bcdDevice: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.bcdDevice));
		Debug(_T("  iManufacturer: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.iManufacturer));
		if (m_DeviceDescriptor.iManufacturer)
			m_StringMfg = GetStringDescriptor(m_DeviceDescriptor.iManufacturer);
		if (m_StringMfg.empty())
			Warning(_T("    No manufacturer name was provided\n"));
		else
			Debug(_T("    Manufacturer name: %s\n"), m_StringMfg.c_str());
		Debug(_T("  iProduct: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.iProduct));
		if (m_DeviceDescriptor.iProduct)
			m_StringDescriptor = GetStringDescriptor(m_DeviceDescriptor.iProduct);
		if (m_StringDescriptor.empty())
			Warning(_T("    No product name was provided\n"));
		else
			Debug(_T("    Product name: %s\n"), m_StringDescriptor.c_str());
		Debug(_T("  iSerialNumber: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.iSerialNumber));
		if (m_DeviceDescriptor.iSerialNumber)
			m_StringSerial = GetStringDescriptor(m_DeviceDescriptor.iSerialNumber);
		if (m_StringSerial.empty())
			Warning(_T("    No serial number was provided\n"));
		else
			Debug(_T("    Serial Number: %s\n"), m_StringSerial.c_str());
	}

	// Let's query for the configuration descriptor
	Detail(_T("Getting configuration descriptor\n"));
	memset(&m_ConfigDescriptor, 0, sizeof(m_ConfigDescriptor));
	nTransferred = 0;
	if (!m_Device.GetDescriptor(USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, reinterpret_cast<PUCHAR>(&m_ConfigDescriptor), sizeof(m_ConfigDescriptor), &nTransferred))
		AtlThrowLastWin32();
	if (IsDebugLevel())
	{
		Debug(_T("  bLength: %d\n"), static_cast<int>(m_ConfigDescriptor.bLength));
		Debug(_T("  bDescriptorType: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bDescriptorType));
		Debug(_T("  wTotalLength: 0x%04X\n"), static_cast<int>(m_ConfigDescriptor.wTotalLength));
		Debug(_T("  bNumInterfaces: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bNumInterfaces));
		Debug(_T("  bConfigurationValue: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bConfigurationValue));
		Debug(_T("  iConfiguration: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.iConfiguration));
		if (m_ConfigDescriptor.iConfiguration)
		{
			s = GetStringDescriptor(m_ConfigDescriptor.iConfiguration);
			Debug(_T("    Configuration name: %s\n"), s.c_str());
		}
		Debug(_T("  bmAttributes: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bmAttributes));
		Debug(_T("  MaxPower: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.MaxPower));

	}

	// Let's query the device for info
	if (IsDebugLevel())
	{
		Debug(_T("Querying device speed\n"));
		UCHAR deviceSpeed = 0;
		ULONG nBufferLen = sizeof(deviceSpeed);
		if (!m_Device.QueryDeviceInformation(DEVICE_SPEED, &nBufferLen, &deviceSpeed))
			AtlThrowLastWin32();
		switch (deviceSpeed)
		{
		case LowSpeed:
			Debug(_T("  The device is operating at low speed\n"));
			break;
		case FullSpeed:
			Debug(_T("  The device is operating at full speed\n"));
			break;
		case HighSpeed:
			Debug(_T("  The device is operating at high speed\n"));
			break;
		default:
			Debug(_T("  The device is operating at unknown %d speed\n"), (int)deviceSpeed);
			break;
		}
	}
	// Current alternate setting
	m_AlternateSetting = 0;
	if (!m_Device.GetCurrentAlternateSetting(&m_AlternateSetting))
		AtlThrowLastWin32();
	Debug(_T("Current alternate setting %d\n"), static_cast<int>(m_AlternateSetting));

	//Let's query for interface and pipe information
	Detail(_T("Querying device interfaces and pipes\n"));
	for (UCHAR interfaceNumber = 0; interfaceNumber <= 0xFF; ++interfaceNumber)
	{
		USB_INTERFACE_DESCRIPTOR interfaceDescriptor;
		memset(&interfaceDescriptor, 0, sizeof(interfaceDescriptor));
		if (!m_Device.QueryInterfaceSettings(interfaceNumber, &interfaceDescriptor))
		{
			DWORD err = GetLastError();
			if (err == ERROR_NO_MORE_ITEMS)
				break;
			AtlThrow(HRESULT_FROM_WIN32(err));
		}
		else
		{
			Debug(_T("  Interface %d details\n"), static_cast<int>(interfaceNumber));
			Debug(_T("    bLength: %d\n"), static_cast<int>(interfaceDescriptor.bLength));
			Debug(_T("    bDescriptorType: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bDescriptorType));
			Debug(_T("    bInterfaceNumber: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceNumber));
			Debug(_T("    bAlternateSetting: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bAlternateSetting));
			Debug(_T("    bNumEndpoints: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bNumEndpoints));
			Debug(_T("    bInterfaceClass: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceClass));
			Debug(_T("    bInterfaceSubClass: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceSubClass));
			Debug(_T("    bInterfaceProtocol: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceProtocol));
			Debug(_T("    iInterface: 0x%02X\n"), static_cast<int>(interfaceDescriptor.iInterface));

		}
		for (UCHAR pipeNumber = 0; pipeNumber <= 0xFF; ++pipeNumber)
		{
			WINUSB_PIPE_INFORMATION pipeInformation;
			memset(&pipeInformation, 0, sizeof(pipeInformation));
			if (!m_Device.QueryPipe(interfaceNumber, pipeNumber, &pipeInformation))
			{
				DWORD err = GetLastError();
				if (err == ERROR_NO_MORE_ITEMS)
					break;
				AtlThrow(HRESULT_FROM_WIN32(err));
			}
			else
			{
				WINUSB_PIPE_INFORMATION_EX pipeInformationEx;
				memset(&pipeInformationEx, 0, sizeof(pipeInformationEx));
				if (! (m_Device.QueryPipEx(interfaceNumber, pipeNumber, &pipeInformationEx)))
					AtlThrowLastWin32();
				switch (pipeInformation.PipeType)
				{
				case UsbdPipeTypeControl:
					Debug(_T("      Control Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				case UsbdPipeTypeIsochronous:
					Debug(_T("      Isochronous Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				case UsbdPipeTypeBulk:
					if (USB_ENDPOINT_DIRECTION_IN(pipeInformation.PipeId))
					{
						Debug(_T("      Bulk IN Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
						if (m_PipeIn)
						{
							Error(_T("Too many input pipes found. Only one was expected.\n"));
							AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
						}
						m_PipeIn = pipeInformation.PipeId;
					}
					if (USB_ENDPOINT_DIRECTION_OUT(pipeInformation.PipeId))
					{
						Debug(_T("      Bulk OUT Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
						if (m_PipeOut)
						{
							Error(_T("Too many input pipes found. Only one was expected.\n"));
							AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
						}
						m_PipeOut = pipeInformation.PipeId;
					}
					break;
				case UsbdPipeTypeInterrupt:
					Debug(_T("      Interrupt Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				default:
					Debug(_T("      Unknown [%d] Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeType), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				}
			}
		}
	}
	// Validate pipes
	if(m_PipeIn == 0 || m_PipeOut == 0)
	{
		Error(_T("Communication pipes could not be found. Device can't be used!\n"));
		AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
	}
	// Creates the read thread
	m_fRunning = true;
	m_dwThreadExitCode = 0;
	m_hReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hReadThread = _beginthread(ReadThread, 128 * 1024, (LPVOID)this);
	DWORD err = WaitForSingleObject(m_hReady, 5000);
	CloseHandle(m_hReady);
	m_hReady = NULL;
	if (err != WAIT_OBJECT_0)
	{
		Error(_T("Read thread failed to be ready with error code %d\n"), err);
		AtlThrow(HRESULT_FROM_WIN32(err));
	}
	LoadIcdiAttr();
}


void CTivaIcdi::Close()
{
	// Disconnect thread and wait until it stops (with timeout)
	m_fRunning = false;
	int retry = 1000;
	do
	{
		::Sleep(1);
	}
	while (m_hReadThread != 0 && --retry);
	// Free the state machine
	delete m_pStateMachine;
	m_pStateMachine = NULL;

	// Clear everything
	ZeroMemory(&m_DeviceDescriptor, sizeof(m_DeviceDescriptor));
	ZeroMemory(&m_ConfigDescriptor, sizeof(m_ConfigDescriptor));
	m_StringDescriptor.clear();
	m_StringMfg.clear();
	m_StringSerial.clear();
	m_AlternateSetting = 0;
	m_PipeIn = 0;
	m_PipeOut = 0;
	m_Device.Free();
}


void CTivaIcdi::WritePipe(const CGdbPacket &data)
{
	const char *pChar = data;

	Detail(_T("%-22hs: GDB --> ICDI: '%s'\n"), __FUNCTION__, (LPCTSTR)data.GetPrintableString());

	ULONG xfered;
	if (!m_Device.WritePipe(m_PipeOut, (PUCHAR)pChar, (ULONG)data.GetCount(), &xfered, NULL))
	{
		DWORD err = GetLastError();
		Error(_T("Failed to write to USB Pipe\n"));
		AtlThrow(HRESULT_FROM_WIN32(err));
	}
}


bool CTivaIcdi::SendReceive(const CGdbPacket &send, CGdbPacket &receive)
{
	HANDLE hEvent = m_pStateMachine->GetReceiveEventHandle();
	ResetEvent(hEvent);
	WritePipe(send);
	DWORD err = WaitForSingleObject(hEvent, 200);
	if (err != WAIT_OBJECT_0 && err != WAIT_TIMEOUT)
	{
		Error(_T("Command '%s' returned %d\n"), (LPCTSTR)send.GetPrintableString(), err);
		AtlThrow(HRESULT_FROM_WIN32(err));
	}
	CTivaGdbStateMachine::LocalStore res = m_pStateMachine->GetLocalStoreBuffer();
	if(res->m_Buffer.GetCount())
	{
		receive = res->m_Buffer;
		Debug(_T("Response: '%s'\n"), (LPCTSTR)res->m_Buffer.GetPrintableString());
	}
	if (err == WAIT_TIMEOUT)
		return false;
	return true;
}


void CTivaIcdi::LoadIcdiAttr()
{
	CGdbPacket packet, response;

	HANDLE hRead = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	try
	{
		m_pStateMachine->SetupEatDataMode(hRead);
		packet.MakeRemoteCmd("version");
		if(SendReceive(packet, response))
		{
			Info(_T("Response: '%hs'\n"), response.UnhexifyPayload().c_str());
		}
	}
	catch(...)
	{
		m_pStateMachine->SetupEatDataMode(NULL);
		CloseHandle(hRead);
		throw;
	}
	m_pStateMachine->SetupEatDataMode(NULL);
	CloseHandle(hRead);
}


/*!
This method sends the parsed inet data directly to the USB output pipe.

This routine runs on the main thread.
*/
void CTivaIcdi::HandleData(CGdbStateMachine &gdbCtx)
{
	if (m_dwThreadExitCode != 0)
		AtlThrow(HRESULT_FROM_WIN32(m_dwThreadExitCode));

	// Updates the state machine so we can rework ICDI responses
	m_pStateMachine->InterceptTransmitLink(gdbCtx);

	WritePipe(gdbCtx);
}


DWORD CTivaIcdi::OnGetThreadErrorState() const
{
	return m_dwThreadExitCode;
}


void __cdecl CTivaIcdi::ReadThread(LPVOID pThis)
{
	((CTivaIcdi*)pThis)->ReadThread();
	_endthread();
}


void CTivaIcdi::ReadThread()
{
	BYTE buffer[READ_BUFFER_BYTES + 1];
	DWORD dwStartTime = GetTickCount();
	bool fLockout = true;
	ULONG xfered;
	OVERLAPPED op;
	memset(&op, 0, sizeof(op));
	op.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	while (m_fRunning)
	{
		bool fPending = false;
		if (!m_Device.ReadPipe(m_PipeIn, buffer, READ_BUFFER_BYTES, &xfered, &op))
		{
			DWORD err = GetLastError();
			if (err != ERROR_IO_PENDING)
			{
				m_fRunning = false;
				m_dwThreadExitCode = err;
				break;
			}
			fPending = true;
		}
		// Wait for pending I/O
		while (fPending)
		{
			// If other thread wants us to close, we need to cancel I/O now
			if (!m_fRunning)
			{
				CancelIoEx(m_Device, &op);
				fPending = false;
				Warning(_T("%hs: Read thread was cancelled\n"), __FUNCTION__);
				break;
			}

			if (fLockout)
			{
				// delay data transfer start to throw any buffered data away
				if ((GetTickCount() - dwStartTime) >= 90)
				{
					if (m_hReady)
						SetEvent(m_hReady);
					fLockout = false;
				}
			}

			DWORD dw = WaitForSingleObject(op.hEvent, 100);
			if (dw == WAIT_TIMEOUT)
				continue;
			ResetEvent(op.hEvent);
			if (dw != WAIT_OBJECT_0)
			{
				Error(_T("%hs: Error waiting for USB packet read\n"), __FUNCTION__);
				fPending = false;
				m_fRunning = false;
				m_dwThreadExitCode = dw;
			}
			else
				break;	// fPending is true!!!
		}
		// Read if not canceled or lockout
		if (m_fRunning && !fLockout)
		{
			if (fPending)
				m_Device.GetOverlappedResult(&op, &xfered, FALSE);
			buffer[xfered] = 0;
			Debug(_T("%hs: USB Pipe IN:'%hs'\n"), __FUNCTION__, buffer);
			if (m_pStateMachine)
				m_pStateMachine->ParseAndDispatch((const char *)buffer, xfered);
		}
	}
	CloseHandle(op.hEvent);
	m_hReadThread = 0;
}

