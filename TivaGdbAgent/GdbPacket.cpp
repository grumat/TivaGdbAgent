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


CAtlString CRawBuffer::GetPrintableString() const
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


CGdbPacket::PayloadBuilder::~PayloadBuilder()
{
	m_Buf.push_back('#');
	m_Buf.push_back(s_itoh[m_ChkSum >> 4]);
	m_Buf.push_back(s_itoh[m_ChkSum & 0x0f]);
	m_Buf.MakeCStrCompat();
};


CGdbPacket::PayloadBuilder & CGdbPacket::PayloadBuilder::operator<<(const char *str)
{
	UINT8 ch;
	for (; (ch = (UINT8)*str) != 0; ++str)
	{
		if(strchr("#$*}", ch) != NULL)
		{
			m_ChkSum += '}';
			m_Buf.push_back('}');
			ch ^= 0x20;
		}
		m_ChkSum += ch;
		m_Buf.push_back(ch);
	}
	return *this;
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
	PayloadBuilder builder(*this);
	builder << str;
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


/*!
\param payload: The un-escaped payload data, without frame begin '$' and tail checksum '#nn'.
\param head: optional parameter that holds all symbols found before the start of payload '$'. This
	typically holds ACK/NAK symbols. Note that the un-escape algorithm is not applied to this part.
\param tail: optional parameter that holds all data after the end of the payload '#nn' without applying
	un-escape rules. Since communication data stores data on buffers this part could theoretically store 
	another payload. In practice, GDB implements a synchronous protocol and this should never happen.
\return	true if payload contains validated data or if no data, head may store Ack or Nak symbols; false
	indicates that all data found in payload, although extracted from the input, could not be validated.
	It may indicate a communication error or an incomplete frame.
*/
bool CGdbPacket::ExtractPayLoad(CRawBuffer &payload, CGdbPacket *head, CGdbPacket *tail) const
{
	enum MyStates_e
	{
		PACKET_HEAD,
		PACKET_PAYLOAD,
		PACKET_CHK1,
		PACKET_CHK2,
		PACKET_TAIL,
	} state = PACKET_HEAD;

	CGdbPacket stuff;

	if (!head)
		head = &stuff;
	if (!tail)
		tail = &stuff;

	UINT8 chk_sum = 0;
	UINT8 cur_chk_sum = 0;
	bool fEscapeNext = false;

	for (CGdbPacket::const_iterator it = cbegin(); it != cend(); ++it)
	{
		UINT8 ch = *it;
		// Don't touch if NAK was found
		switch (state)
		{
		case PACKET_HEAD:
			if (ch == '$')
				state = PACKET_PAYLOAD;
			else
			{
				head->push_back(ch);
				if (ch == '-')
					return true;
			}
			break;
		case PACKET_PAYLOAD:
			if (ch == '#')
				state = PACKET_CHK1;
			else
			{
				cur_chk_sum += ch;
				if (fEscapeNext)
				{
					fEscapeNext = false;
					payload.push_back(ch ^ 0x20);
				}
				else if (ch == '{')
					fEscapeNext = true;
				else
					payload.push_back(ch);
			}
			break;
		case PACKET_CHK1:
		case PACKET_CHK2:
			chk_sum = (chk_sum << 4) | hexchartoi(ch);
			state = static_cast<MyStates_e>(state + 1);
			break;
		case PACKET_TAIL:
			tail->push_back(ch);
			break;
		}
	}
	payload.MakeCStrCompat();
	return (state != PACKET_TAIL) ? false : (chk_sum == cur_chk_sum);
}

