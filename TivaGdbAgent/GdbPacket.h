#pragma once


//! Implements basic GDB data packet support
class CGdbPacket : public std::vector<UINT8>
{
public:
	CGdbPacket() {}

public:
	operator const char *() const { return (const char *)data(); }
	operator const UINT8 *() const { return data(); }
	size_t GetCount() const { return size(); }
	bool HasPayload() const;
	bool IsAck() const { return size() && *data() == '+'; }
	bool IsNak() const { return size() && *data() == '-'; }

	// Normal Assignment operator
	CGdbPacket & operator =(const CGdbPacket &o)
	{
		std::vector<UINT8>::operator=(o); return *this;
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
	//! Append a bytes from another packet
	CGdbPacket & operator <<(const CGdbPacket &o)
	{
		insert(end(), o.begin(), o.end());
		return *this;
	}
	//! Build a GDB packet from an ASCII string
	void MakePayload(const char *str, bool fAppend = false);

	void MakeRemoteCmd(const char *cmd);
	std::string UnhexifyPayload() const;

	//! Extracts the first payload part
	void ExtractPayLoad(CGdbPacket &head, CGdbPacket &payload, CGdbPacket &tail) const;

	//! Returns a escaped string of buffer contents
	CAtlString GetPrintableString() const;
};

