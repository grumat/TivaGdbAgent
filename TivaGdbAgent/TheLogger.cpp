#include "stdafx.h"
#include "TheLogger.h"

namespace Logger
{

CLogger *CLogger::m_pLogger = NULL;


CLogger::CLogger()
{
#ifdef _DEBUG
	m_nLevel = DEBUG_LEVEL;
#else
	m_nLevel = WARN_LEVEL;
#endif
}


CLogger::~CLogger()
{
}


void CLogger::Dispose()
{
	delete m_pLogger;
	m_pLogger = nullptr;
}

CLogger & CLogger::GetLogger()
{
	static bool registered = false;
	if (!m_pLogger)
	{
		m_pLogger = new CStdioLogger();
		if(!registered)
		{
			registered = true;
			atexit(Dispose);
		}
	}
	return *m_pLogger;
}


void Debug(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(DEBUG_LEVEL, msg, vargs);
	va_end(vargs);
}


void Error(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(ERROR_LEVEL, msg, vargs);
	va_end(vargs);
}


void Warning(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(WARN_LEVEL, msg, vargs);
	va_end(vargs);
}


void Info(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(INFO_LEVEL, msg, vargs);
	va_end(vargs);
}


void CLogger::Log(Level_e level, const TCHAR *msg, va_list vargs)
{
	if(OnTestLevel(level))
	{
		CAtlString s;
		s.Format(msg, vargs);
		OnLog(level, s);
	}
}


void CStdioLogger::OnLog(Level_e level, const TCHAR *msg)
{
	_fputts(msg, (level >= ERROR_LEVEL) ? stderr : stdout);
}


}	// namespace Logger
