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


/*!
This method is called from the main thread (see CTivaIcdi::HandleData()) and intercepts
the packets send from gdb front-end to the ICDI back-end (GDB ==> ICDI) and gives us the
opportunity to change out behavior based on what GDB is asking, eventually fixing or 
extending ICDI protocol.
*/
void CTivaGdbStateMachine::InterceptTransmitLink(const CGdbStateMachine &gdbRequest)
{
	CRawBuffer payload;
	gdbRequest.ExtractPayLoad(payload);

	// Exit if no payload in the request
	if (payload.size() == 0)
		return;

#if OPT_FIX_REGISTER_DUMP
	if (strcmp(payload, "g") == 0)
	{
		m_eBehavior = fixRegisterDump;
	}
	else
#endif
#if OPT_SEND_FEATURES
	if (strstr(payload, "qSupported:") != NULL)
	{
		/*
		**	GDB is telling us what are the supported features from the front-end and back-end (ICDI) 
		**	response tells GDB the protocol features it supports. 
		**	By setting the 'extendedFeatures' state, OnBeforeDispatch() will intercept ICDI response 
		**	and tell GDB we support XML target description ('qXfer:features:read+').
		*/
		m_eBehavior = extendedFeatures;
	}
	else if(strstr(payload, "qXfer:features:read:target.xml:") != NULL)
	{
		/*
		**	GDB is asking us for XML target description, but this is not supported by ICDI. We let
		**	this query go to the ICDI back-end but will eat ICDI response, since it cannot actually
		**	handle it, and insert our correct response (see CTivaGdbStateMachine::OnBeforeDispatch()).
		**	Note This helps GDB to handle Cortex M4F register set as it defaults to the younger 
		**	siblings of the Cortex M family.
		*/
		m_eBehavior = extendedFeatures_2;
	}
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
	CRawBuffer payload;
	CGdbPacket head, tail;
	m_Buffer.ExtractPayLoad(payload, &head, &tail);
	if (head.IsNak())
		return;
	m_Buffer = head;
	if(payload.GetCount())
	{
		// Dump register values
		if(IsDebugLevel())
		{
			size_t reg_cnt = 0;
			CAtlString reg_val;
			for (CGdbPacket::const_iterator it = payload.cbegin(); it != payload.cend(); ++it)
			{
				reg_val += *it;
				if (reg_val.GetLength() == 8)
				{
					Debug(_T("[%02d]: %s\n"), reg_cnt++, (LPCTSTR)reg_val);
					reg_val = _T("");
				}
			}
		}
		// Remove last register value
		payload.resize(16 * 8);
		m_Buffer = head;
		m_Buffer << payload << tail;
		m_eBehavior = byPass;
	}
}
#endif


#if OPT_SEND_FEATURES
void CTivaGdbStateMachine::SendFeaturesSupported()
{
	// ICDI sends: $PacketSize=1828;qXfer:memory-map:read+#83
	CRawBuffer payload;
	CGdbPacket head, tail;
	m_Buffer.ExtractPayLoad(payload, &head, &tail);
	if (head.IsNak())
		return;
	if(payload.GetCount() > 22 && strstr(payload, "qXfer:memory-map:read") != NULL)
	{
		payload << ";qXfer:features:read+";
		(m_Buffer = head) << payload << tail;
	}
	if(payload.GetCount())
		m_eBehavior = byPass;
}


void CTivaGdbStateMachine::SendFeaturesXml()
{
#if 0
	static const char target_description_M4F[] =
		"<?xml version=\"1.0\"?>"
		"<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
		"<target>"
		"	<feature name=\"org.gnu.gdb.arm.core\">"
		"		<reg name=\"r0\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r1\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r2\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r3\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r4\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r5\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r6\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r7\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r8\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r9\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r10\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r11\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"r12\" bitsize=\"32\" type=\"uint32\"/>"
		"		<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>"
		"		<reg name=\"lr\" bitsize=\"32\"/>"
		"		<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
		"		<reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\"/>"
		"	</feature>"
		"	<feature name=\"org.gnu.gdb.arm.fpa\">"
		"		<reg name=\"f0\" bitsize=\"96\" type=\"arm_fpa_ext\" regnum=\"16\"/>"
		"		<reg name=\"f1\" bitsize=\"96\" type=\"arm_fpa_ext\"/>"
		"		<reg name=\"f2\" bitsize=\"96\" type=\"arm_fpa_ext\"/>"
		"		<reg name=\"f3\" bitsize=\"96\" type=\"arm_fpa_ext\"/>"
		"		<reg name=\"f4\" bitsize=\"96\" type=\"arm_fpa_ext\"/>"
		"		<reg name=\"f5\" bitsize=\"96\" type=\"arm_fpa_ext\"/>"
		"		<reg name=\"f6\" bitsize=\"96\" type=\"arm_fpa_ext\"/>"
		"		<reg name=\"f7\" bitsize=\"96\" type=\"arm_fpa_ext\"/>"
		"		<reg name=\"fps\" bitsize=\"32\"/>"
		"	</feature>"
		"</target>"
#endif

	static const char target_description_M4F[] =
		"<?xml version=\"1.0\"?>"
		"<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
		"<target version=\"1.0\">"
			"<architecture>arm</architecture>"
			"<feature name=\"org.gnu.gdb.arm.m-profile\">"
				"<reg name=\"r0\" bitsize=\"32\"/>"
				"<reg name=\"r1\" bitsize=\"32\"/>"
				"<reg name=\"r2\" bitsize=\"32\"/>"
				"<reg name=\"r3\" bitsize=\"32\"/>"
				"<reg name=\"r4\" bitsize=\"32\"/>"
				"<reg name=\"r5\" bitsize=\"32\"/>"
				"<reg name=\"r6\" bitsize=\"32\"/>"
				"<reg name=\"r7\" bitsize=\"32\"/>"
				"<reg name=\"r8\" bitsize=\"32\"/>"
				"<reg name=\"r9\" bitsize=\"32\"/>"
				"<reg name=\"r10\" bitsize=\"32\"/>"
				"<reg name=\"r11\" bitsize=\"32\"/>"
				"<reg name=\"r12\" bitsize=\"32\"/>"
				"<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>"
				"<reg name=\"lr\" bitsize=\"32\"/>"
				"<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
				"<reg name=\"xpsr\" bitsize=\"32\" regnum=\"25\"/>"
				"<reg name=\"msp\" bitsize=\"32\" regnum=\"26\" type=\"data_ptr\" group=\"general\" />"
				"<reg name=\"psp\" bitsize=\"32\" regnum=\"27\" type=\"data_ptr\" group=\"general\" />"
				"<reg name=\"control\" bitsize=\"8\" regnum=\"28\" type=\"int\" group=\"general\" />"
				"<reg name=\"faultmask\" bitsize=\"8\" regnum=\"29\" type=\"int\" group=\"general\" />"
				"<reg name=\"basepri\" bitsize=\"8\" regnum=\"30\" type=\"int\" group=\"general\" />"
				"<reg name=\"primask\" bitsize=\"8\" regnum=\"31\" type=\"int\" group=\"general\" />"
				"<reg name=\"s0\" bitsize=\"32\" regnum=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s1\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s2\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s3\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s4\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s5\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s6\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s7\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s8\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s9\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s10\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s11\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s12\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s13\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s14\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s15\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s16\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s17\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s18\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s19\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s20\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s21\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s22\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s23\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s24\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s25\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s26\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s27\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s28\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s29\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s30\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"s31\" bitsize=\"32\" type=\"float\" group=\"float\" />"
				"<reg name=\"fpscr\" bitsize=\"32\" type=\"int\" group=\"float\" />"
			"</feature>"
		"</target>";

	CRawBuffer payload;
	CGdbPacket head, tail;
	m_Buffer.ExtractPayLoad(payload, &head, &tail);
	if (head.IsNak())
		return;
	if(payload.GetCount())
	{
		// Ignore original packet and use a fresh payload buffer
		CGdbPacket payload;
		// Build a suitable response
		{
			CGdbPacket::PayloadBuilder b(payload);
			b << 'l' << target_description_M4F;
		}
		// Replace contents with our info
		(m_Buffer = head) << payload << tail;
		m_eBehavior = byPass;
	}
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
	case extendedFeatures_2:
		SendFeaturesXml();
		break;
#endif
	}
}

