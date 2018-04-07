#pragma once

namespace Logger
{
enum Level_e
{
	OFF,
	ERROR_LEVEL,
	WARN_LEVEL,
	INFO_LEVEL,
	DEBUG_LEVEL,
};


class CLogger
{
public:
	CLogger();
	virtual ~CLogger();

public:
	void Log(Level_e level, const TCHAR *msg, va_list vargs);
	Level_e GetLevel() { return m_nLevel; }
	void SetLevel(Level_e n) { m_nLevel = n; }

public:
	//! Replaces logger by a custom implementation
	static void SetNewLogger(CLogger *logger)
	{
		if (CLogger::m_pLogger) delete m_pLogger;
		m_pLogger = logger;
	}
	static CLogger & GetLogger();
	virtual bool OnTestLevel(Level_e l) const
	{
		return l <= m_nLevel;
	}

protected:
	virtual void OnLog(Level_e level, const TCHAR *msg) = 0;

protected:
	static CLogger *m_pLogger;
	Level_e m_nLevel;
	DWORD m_dwStartTick;
	DWORD m_dwMainThreadID;

private:
	static void Dispose();
};


//! Returns current logger
inline CLogger & TheLogger()
{
	return CLogger::GetLogger();
}


//! Debug messages
void Debug(const TCHAR *msg, ...);
inline bool IsDebugLevel() { return TheLogger().OnTestLevel(DEBUG_LEVEL); }
//! Info messages
void Info(const TCHAR *msg, ...);
inline bool IsInfoLevel() { return TheLogger().OnTestLevel(INFO_LEVEL); }
//! Warning messages
void Warning(const TCHAR *msg, ...);
inline bool IsWarnLevel() { return TheLogger().OnTestLevel(WARN_LEVEL); }
//! Error messages
void Error(const TCHAR *msg, ...);
inline bool IsErrorLevel() { return TheLogger().OnTestLevel(ERROR_LEVEL); }
inline bool IsOff() { return TheLogger().GetLevel() == OFF; }


class CStdioLogger : public CLogger
{
protected:
	void OnLog(Level_e level, const TCHAR *msg) override;
};

}


