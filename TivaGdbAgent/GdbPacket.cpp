#include "stdafx.h"
#include "GdbPacket.h"


static const char s_itoh[] = "0123456789ABCDEF";


static int hexchartoi(char c)
{
	if ((c >= '0') && (c <= '9'))
	{
		return c - '0';
	}

	if ((c >= 'a') && (c <= 'f'))
	{
		return c - 'a' + 10;
	}

	if ((c >= 'A') && (c <= 'F'))
	{
		return c - 'A' + 10;
	}

	return 0;
}


bool CGdbPacket::HasPayload() const
{
	bool match = false;
	for(const_iterator it = cbegin(); it != cend(); ++it)
	{
		if(*it == '$')
		{
			for(++it; it != cend(); ++it)
			{
				if (*it == '#')
					return true;
			}
			break;
		}
	}
	return false;
}


void CGdbPacket::MakePayload(const char *str, bool fAppend)
{
	if(!fAppend)
		resize(0);
	push_back('$');
	UINT8 chk_sum = 0;
	UINT8 ch;
	for (; (ch = (UINT8)*str) != 0; ++str)
	{
		chk_sum += ch;
		push_back(ch);
	}
	push_back('#');
	push_back(s_itoh[chk_sum >> 4]);
	push_back(s_itoh[chk_sum & 0x0f]);
}


void CGdbPacket::MakeRemoteCmd(const char *cmd)
{
	std::string s("qRcmd,");
	UINT8 ch;
	for(; (ch = *cmd) != 0; ++cmd)
	{
		s.push_back(s_itoh[ch >> 4]);
		s.push_back(s_itoh[ch & 0x0f]);
	}
	(*this) = s.c_str();
}


CAtlString CGdbPacket::GetPrintableString() const
{
	CAtlString out, hex, ascii;
	size_t ctl_cnt = 0;
	size_t ch_cnt = 0;
	bool use_hex = false;
	for (const_iterator it = cbegin(); it != cend(); ++it)
	{
		UINT8 ch = (UINT8)*it;
		++ch_cnt;
		// Hex formatting
		if (!hex.IsEmpty())
			hex += _T(' ');
		hex.AppendFormat(_T("%02x"), ch);
		// ASCI formatting
		if (ch < _T(' ') || ch == _T('\x7f') || ch == _T('\xff'))	// LATIN-1
		{
			ascii.AppendFormat(_T("\\x%02x"), ch);
			++ctl_cnt;
		}
		else
		{
			ascii += ch;
			if (ch == _T(':'))	// vFlashWritePacket use a mix between ASCII and hex
			{
				if (ch_cnt > 8 && 4 * ctl_cnt > ch_cnt)
				{
					use_hex = true;
					out += hex;
					hex = _T("");
					ascii = _T("");
				}
				else
				{
					out += ascii;
					ch_cnt = ctl_cnt = 0;
					hex = _T("");
					ascii = _T("");
				}
			}
		}
	}
	if (!ascii.IsEmpty())
	{
		if (!use_hex)
			use_hex = ch_cnt > 8 && 4 * ctl_cnt > ch_cnt;
		out += (use_hex) ? hex : ascii;
	}
	return out;
}


std::string CGdbPacket::UnhexifyPayload() const
{
	std::string s;
	for (const_iterator it = cbegin(); it != cend(); ++it)
	{
		if(*it == '$')
		{
			++it;
			while (it != cend())
			{
				if (*it == '#')
					break;
				UINT8 ch = hexchartoi(*it++) << 4;
				if (it == cend() || *it == '#')
					break;
				ch += hexchartoi(*it++);
				s.push_back(ch);
			}
			break;
		}
	}
	return s;
}


void CGdbPacket::ExtractPayLoad(CGdbPacket &head, CGdbPacket &payload, CGdbPacket &tail) const
{
	enum MyStates_e
	{
		PACKET_HEAD,
		PACKET_PAYLOAD,
		PACKET_CHK1,
		PACKET_CHK2,
		PACKET_TAIL,
	} state = PACKET_HEAD;

	for (CGdbPacket::const_iterator it = cbegin(); it != cend(); ++it)
	{
		char ch = *it;
		// Don't touch if NAK was found
		switch (state)
		{
		case PACKET_HEAD:
			if (ch == '$')
				state = PACKET_PAYLOAD;
			else
			{
				head.push_back(ch);
				if (ch == '-')
					return;
			}
			break;
		case PACKET_PAYLOAD:
			if (ch == '#')
				state = PACKET_CHK1;
			else
				payload.push_back(ch);
			break;
		case PACKET_CHK1:
		case PACKET_CHK2:
			state = static_cast<MyStates_e>(state + 1);
			break;
		case PACKET_TAIL:
			tail.push_back(ch);
			break;
		}
	}
	// Payload packet is incomplete
	if (state != PACKET_TAIL)
		payload.resize(0);
}

