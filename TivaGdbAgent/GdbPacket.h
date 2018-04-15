#pragma once


//! Raw data buffer (stores non escaped bytes)
class CRawBuffer : public std::vector<UINT8>
{
public:
	CRawBuffer() { }
	CRawBuffer(size_t reserve_bytes) { __super::reserve(reserve_bytes); }

// Operators
public:
	explicit operator CAtlStringA() const { return CAtlStringA((const char*)__super::data(), (int)__super::size()); }
	explicit operator std::string() const { return std::string((const char*)__super::data(), (int)__super::size()); }

	// Normal Assignment operator
	CRawBuffer & operator =(const CRawBuffer &o)
	{
		__super::operator=(o);
		MakeCStrCompat();
		return *this;
	}
	//! Assignment of an ASCII string
	CRawBuffer & operator =(const char *str)
	{
		__super::resize(0);
		return (*this) << str;
	}
	//! Append a single character to the buffer
	CRawBuffer & operator <<(const char ch)
	{
		push_back(ch);
		MakeCStrCompat();
		return *this;
	}
	//! Append a bytes from another packet
	CRawBuffer & operator <<(const CRawBuffer &o)
	{
		__super::insert(end(), o.begin(), o.end());
		MakeCStrCompat();
		return *this;
	}
	//! Append a bytes from another packet
	CRawBuffer & operator <<(const char *str)
	{
		while (*str)
			push_back(*str++);
		MakeCStrCompat();
		return *this;
	}

public:
	operator const char *() const { return (const char *)__super::data(); }
	operator const UINT8 *() const { return __super::data(); }
	size_t GetCount() const { return __super::size(); }

	//! Returns a escaped string of buffer contents
	CAtlString GetPrintableString() const;

	void MakeCStrCompat()
	{
		__super::reserve(size() + 1);
		__super::data()[size()] = 0;
	}
};


//! Implements basic GDB data packet support (which contents are already escaped)
class CGdbPacket : public CRawBuffer
{
public:
	CGdbPacket() : CRawBuffer(MSGSIZE) {}

	enum
	{
		MSGSIZE = 8192,
	};

	class PayloadBuilder
	{
	public:
		PayloadBuilder(CGdbPacket &buf) : m_Buf(buf)
		{
			m_ChkSum = 0;
			m_Buf.push_back('$');
		}
		~PayloadBuilder();

		PayloadBuilder & operator<<(UINT8 ch)
		{
			m_Buf.push_back(ch);
			m_ChkSum += ch;
			return *this;
		}
		PayloadBuilder & operator<<(const char *str);

	protected:
		CGdbPacket & m_Buf;
		UINT8 m_ChkSum;
	};

public:
	bool HasPayload() const;
	bool IsAck() const { return size() && *data() == '+'; }
	bool IsNak() const { return size() && *data() == '-'; }

	// Normal Assignment operator
	CGdbPacket & operator =(const CGdbPacket &o)
	{
		__super::operator=((CRawBuffer &)o);
		MakeCStrCompat();
		return *this;
	}
	//! Build a GDB payload packet from an ASCII string
	CGdbPacket & operator =(const char *str)
	{
		MakePayload(str);
		return *this;
	}
	//! Append a GDB payload packet from an ASCII string
	CGdbPacket & operator <<(const char *str)
	{
		MakePayload(str, true);
		return *this;
	}
	//! Append a CRawBuffer applying transformations (i.e. escaping)
	CGdbPacket & operator <<(const CRawBuffer &o)
	{
		MakePayload(o, true);
		return *this;
	}
	//! Append a CGdbPacket to this one without applying transformations (i.e. no escaping)
	CGdbPacket & operator <<(const CGdbPacket &o)
	{
		__super::insert(end(), o.begin(), o.end());
		MakeCStrCompat();
		return *this;
	}
	//! Build a GDB packet from an ASCII string
	void MakePayload(const char *str, bool fAppend = false);

	void MakeRemoteCmd(const char *cmd);
	std::string UnhexifyPayload() const;

	//! Extracts the first payload part (tail could theoretically hold more packets)
	bool ExtractPayLoad(CRawBuffer &payload, CGdbPacket *head = NULL, CGdbPacket *tail = NULL) const;

private:
	bool ExtractPayLoad(CGdbPacket &payload, CGdbPacket *head = NULL, CGdbPacket *tail = NULL) const
	{
		return ExtractPayLoad((CRawBuffer&)payload, head, tail);
	}
};

