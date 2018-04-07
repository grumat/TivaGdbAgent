#include "stdafx.h"
#include "TivaGdbStateMachine.h"


CTivaGdbStateMachine::CTivaGdbStateMachine(IGdbDispatch &handler)
	: CGdbStateMachine(handler)
{
	m_fDumpRegisters = false;
}


CTivaGdbStateMachine::~CTivaGdbStateMachine()
{
}


void CTivaGdbStateMachine::InterceptTransmitLink(const CGdbStateMachine &gdbRequest)
{
	m_fDumpRegisters = (strstr(gdbRequest, "$g#67") != NULL);
}



void CTivaGdbStateMachine::OnBeforeDispatch()
{
	static char itoh[] = "0123456789ABCDEF";

	if(m_fDumpRegisters)
	{
		enum
		{
			PACKET_HEAD,
			PACKET_PAYLOAD,
			PACKET_CHK1,
			PACKET_CHK2,
			PACKET_TAIL,
		} state = PACKET_HEAD;
		BYTE chksum = 0;
		std::string head, payload, tail;
		for(std::string::const_iterator it = m_Buffer.cbegin(); it != m_Buffer.cend(); ++it)
		{
			char ch = *it;
			// Don't touch if NAK was found
			switch (state)
			{
			case PACKET_HEAD:
				if (ch == '-')
					return;
				if (ch == '$')
				{
					state = PACKET_PAYLOAD;
					payload.push_back(ch);
				}
				else
					head.push_back(ch);
				break;
			case PACKET_PAYLOAD:
				if (ch == '#')
					state = PACKET_CHK1;
				if (payload.size() < (16*8 + 1))
				{
					chksum += (BYTE)ch;
					payload.push_back(ch);
				}
				break;
			case PACKET_CHK1:
				state = PACKET_CHK2;
				break;
			case PACKET_CHK2:
				state = PACKET_TAIL;
				payload.push_back('#');
				payload.push_back(itoh[chksum >> 4]);
				payload.push_back(itoh[chksum & 0xf]);
				break;
			case PACKET_TAIL:
				tail.push_back(ch);
				break;
			}
		}
		m_Buffer = head.c_str();	// assign without reassigning internal buffers
		if (! payload.empty())
		{
			m_Buffer.append(payload);
			m_Buffer.append(tail);
			m_fDumpRegisters = false;
		}
	}
}

