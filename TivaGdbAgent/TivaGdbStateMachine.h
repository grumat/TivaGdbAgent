#pragma once

#include "GdbDispatch.h"

//! This option fixes the 'g' command response so it returns 16 registers instead of 17
#define OPT_FIX_REGISTER_DUMP	1
//! THis option extends ICDI by sending the Cortex M4 features XML back to the GDB
#define OPT_SEND_FEATURES		1

//! Fixes differences between ICDI link and GDB
class CTivaGdbStateMachine : public CGdbStateMachine
{
public:
	CTivaGdbStateMachine(IGdbDispatch &handler);
	~CTivaGdbStateMachine();

	class LocalStore_
	{
	public:
		LocalStore_(CTivaGdbStateMachine &owner)
			: m_Buffer(owner.m_TheStore), m_Locker(owner.m_Lock)
		{
		}

	public:
		CGdbPacket & m_Buffer;
		void Append(const CGdbPacket &o) { m_Buffer.insert(m_Buffer.end(), o.begin(), o.end()); }

	protected:
		CComCritSecLock<CComCriticalSection> m_Locker;
	};
	typedef std::shared_ptr<LocalStore_> LocalStore;


	enum Behavior_e
	{
		eatAndStore = -1,		//!< Used during init: Stores and eats the contents; send a signal to an event
		byPass,
#if OPT_FIX_REGISTER_DUMP
		fixRegisterDump,
#endif
#if OPT_SEND_FEATURES
		extendedFeatures,
#endif
	};

public:
	//! Sets up the eat data mode (NULL to cancel eat-mode)
	void SetupEatDataMode(HANDLE hEvent);
	//! Returns the handle of the synhronization object
	HANDLE GetReceiveEventHandle() const { return m_hEatEvent; }
	//! Intercepts transmit link to know how to process ICDI responses
	void InterceptTransmitLink(const CGdbStateMachine &gdbRequest);

	LocalStore GetLocalStoreBuffer() { return std::make_shared<LocalStore_>(*this); }

protected:
	void EatAndStore();
#if OPT_FIX_REGISTER_DUMP
	void FixRegisterDump();
#endif
#if OPT_SEND_FEATURES
	void SendFeaturesSupported();
#endif

protected:
	//! Fixes protocol differences between ICDI and GDB
	void OnBeforeDispatch() override;

private:
	Behavior_e m_eBehavior;
	//! Handle to set an event when data has arrived
	HANDLE m_hEatEvent;
	//! Where eaten data is stored
	CGdbPacket m_TheStore;
	//! Serializes access to m_TheStore
	CComCriticalSection m_Lock;
};

