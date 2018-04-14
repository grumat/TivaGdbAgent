#include "stdafx.h"
#include "TivaGdbStateMachine.h"
#include "TheLogger.h"


using namespace Logger;



CTivaGdbStateMachine::CTivaGdbStateMachine(IGdbDispatch &handler)
	: CGdbStateMachine(handler)
{
	m_eBehavior = byPass;
	m_hEatEvent = NULL;
	m_Lock.Init();
}


CTivaGdbStateMachine::~CTivaGdbStateMachine()
{
}


void CTivaGdbStateMachine::SetupEatDataMode(HANDLE hEvent)
{
	m_hEatEvent = hEvent;
	m_eBehavior = (hEvent == NULL) ? byPass : eatAndStore;
	m_TheStore.resize(0);
}

void CTivaGdbStateMachine::InterceptTransmitLink(const CGdbStateMachine &gdbRequest)
{
#if OPT_FIX_REGISTER_DUMP
	if (strstr(gdbRequest, "$g#67") != NULL)
	{
		m_eBehavior = fixRegisterDump;
	}
	else
#endif
#if OPT_SEND_FEATURES
	if (strstr(gdbRequest, "$qSupported:") != NULL)
	{
		m_eBehavior = extendedFeatures;
		//$qXfer:features:read:target.xml:0,fff#7d
	}
	else if()
#endif
	else
	{
		// skip syntax error messages
	}
}


/*!
During initialization we don't want the read thread to pass data to
the GDB link. Instead it should eat everything and store locally to be
analyzed by the initialization routine.
Data such as ICDI version is taken here.
*/
void CTivaGdbStateMachine::EatAndStore()
{
	LocalStore store = GetLocalStoreBuffer();
	store->Append(m_Buffer);
	if (store->m_Buffer.HasPayload())
		SetEvent(m_hEatEvent);
	m_Buffer.resize(0);
}


#if OPT_FIX_REGISTER_DUMP
/*!
Problem here as that ICDI dumps 17 DWORDs for the 'g' protocol command.
It is still unclear what is the extra contents and if it is on the head
or tail of the stream.
The OPT_DISCARDS_AT_HEAD flag controls what part of the stream is considered.
*/
void CTivaGdbStateMachine::FixRegisterDump()
{
	size_t reg_cnt = 0;
	CGdbPacket head, payload, tail;
	m_Buffer.ExtractPayLoad(head, payload, tail);
	if (head.IsNak())
		return;
	m_Buffer = head;
	if(payload.GetCount())
	{
		CAtlStringA fixed_reg_set;
		CAtlString reg_val;
		for (CGdbPacket::const_iterator it = payload.cbegin(); it != payload.cend(); ++it)
		{
			if(fixed_reg_set.GetLength() < 16*8)
				fixed_reg_set += *it;
			if (IsDetailLevel())
			{
				reg_val += *it;
				if (reg_val.GetLength() == 8)
				{
					Detail(_T("[%02d]: %s\n"), reg_cnt++, (LPCTSTR)reg_val);
					reg_val = _T("");
				}
			}
		}
		m_Buffer << fixed_reg_set << tail;
	}
	m_eBehavior = byPass;
}
#endif


#if OPT_SEND_FEATURES
void CTivaGdbStateMachine::SendFeaturesSupported()
{
	// ICDI sends: $PacketSize=1828;qXfer:memory-map:read+#83
	CGdbPacket head, payload, tail;
	m_Buffer.ExtractPayLoad(head, payload, tail);
	if (head.IsNak())
		return;
	if(payload.GetCount() > 22 && strstr(payload, "qXfer:memory-map:read") != NULL)
	{
		CAtlStringA packet((const char *)payload, (int)payload.GetCount());
		packet += ";qXfer:features:read+";
		m_Buffer = head;
		m_Buffer << packet << tail;
	}
	if(payload.GetCount())
		m_eBehavior = byPass;
}
#endif


void CTivaGdbStateMachine::OnBeforeDispatch()
{
	switch (m_eBehavior)
	{
	case eatAndStore:
		EatAndStore();
		break;
#if OPT_FIX_REGISTER_DUMP
	case fixRegisterDump:
		FixRegisterDump();
		break;
#endif
#if OPT_SEND_FEATURES
	case extendedFeatures:
		SendFeaturesSupported();
		break;
#endif
	}
}

