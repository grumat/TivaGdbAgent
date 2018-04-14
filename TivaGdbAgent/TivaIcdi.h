#pragma once

#include "TivaGdbStateMachine.h"


class CTivaIcdi : public IGdbDispatch
{
public:
	CTivaIcdi();
	virtual ~CTivaIcdi();
	enum
	{
		VID = 0x1cbe,
		PID = 0xfd,
		READ_BUFFER_BYTES = CGdbStateMachine::MSGSIZE,
	};

public:
	//! "Stellaris ICDI JTAG/SWD Interface"
	static const GUID m_guidICDI;
	//! "Stellaris ICDI DFU Device"
	static const GUID m_guidDFU;
	//! "Stellaris Virtual Serial Port"
	static const GUID m_guidCOM;

public:
	WinUSB::String GetTivaIcdiDevice();
	void Open(IGdbDispatch &rReadPackets);
	void Close();

// Interface class overrides
protected:
	//! Handles data coming from the Inet link
	void HandleData(CGdbStateMachine &gdbCtx) override;
	DWORD OnGetThreadErrorState() const override;

protected:
	WinUSB::String GetStringDescriptor(UCHAR index);
	void LoadIcdiAttr();
	//! Synchronous send and receive during "Eat Data Mode"
	bool SendReceive(const CGdbPacket &send, CGdbPacket &receive);
	void WritePipe(const CGdbPacket &data);

protected:
	static void __cdecl ReadThread(LPVOID pThis);
	void ReadThread();

protected:
	WinUSB::CDevice m_Device;
	USB_DEVICE_DESCRIPTOR m_DeviceDescriptor;
	USB_CONFIGURATION_DESCRIPTOR m_ConfigDescriptor;
	UCHAR m_AlternateSetting;
	WinUSB::String m_StringDescriptor;
	WinUSB::String m_StringMfg;
	WinUSB::String m_StringSerial;
	UCHAR m_PipeIn;
	UCHAR m_PipeOut;

protected:
	volatile DWORD m_dwThreadExitCode;
	volatile uintptr_t m_hReadThread;
	volatile bool m_fRunning;
	HANDLE m_hReady;
	CTivaGdbStateMachine *m_pStateMachine;
};

