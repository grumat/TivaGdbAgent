// TivaGdbAgent.cpp : Define o ponto de entrada para a aplicação de console.
//

#include "stdafx.h"
#include "TivaIcdi.h"
#include "GdbLink.h"
#include "TheLogger.h"


using namespace Logger;


static CTivaIcdi *s_pTivaObj = NULL;
static BOOL WINAPI CloseTiva(DWORD dwCtrlType)
{
	if (s_pTivaObj)
	{
		s_pTivaObj->Close();
		s_pTivaObj = NULL;
	}
	// Return FALSE to continue with other handlers
	return FALSE;
}


static int Usage(bool full)
{
	puts(
		"TivaGdbAgent 0.1\n"
		"================\n"
		"A GDB stub to interface with Tiva Stellaris ICDI debugger.\n"
		"The default listen port is 7777.\n"
	);
	if (full == false)
	{
		puts("Use '-h' switch for help.\n");
		return EXIT_FAILURE;
	}
	puts(
		"\n"
		"USAGE: TivaGdbAgent [-v] [-p nnn]\n"
		"WHERE\n"
		"   -h     : This help message\n"
		"   -p nnn : Listen in the given port number\n"
		"   -v     : Increase verbose level\n"
	);
	return EXIT_FAILURE;
}


int main(int argc, char *argv[])
{
#ifdef _DEBUG
	int log_level = DETAIL_LEVEL;
#else
	int log_level = WARN_LEVEL;
#endif
	int port = 7777;
	enum CmdState
	{
		kNormal,
		kGetPort,
	} state = kNormal;
	for (int i = 1; i < argc; ++i)
	{
		const char *arg = argv[i];
		if (*arg == '-' || *arg == '/')
		{
			// Get switch
			char sw = *++arg;
			// No support for compound switches
			if (*++arg != 0)
				goto unknown_switch;
			if (sw == 'h')
			{
				// Help
				return Usage(true);
			}
			else if (sw == 'p')
			{
				// Port
				state = kGetPort;
			}
			else if (sw == 'v')
			{
				// Log level
				++log_level;
			}
			else
			{
unknown_switch:
				// Unknown switch found
				fprintf(stderr, "Unknown switch '%s'! Use '-h' for Help!", argv[i]);
				return EXIT_FAILURE;
			}
		}
		else if (state == kGetPort)
		{
			// Get port number
			state = kNormal;
			char *end;
			port = strtoul(arg, &end, 0);
			// Validate
			if (*end || port == 0)
			{
				fprintf(stderr, "Unknown value '%s' for port number! It should be an integer value!", arg);
				return EXIT_FAILURE;
			}
		}
		else
		{
			// Unknown token on command line
			Usage(false);
			fprintf(stderr, "Unknown parameter '%s'!", argv[i]);
			return EXIT_FAILURE;
		}
	}
	// Ensure log level is in a valid range
	if (log_level > DEBUG_LEVEL)
		log_level = DEBUG_LEVEL;

	try
	{
		TheLogger().SetLevel((Level_e)log_level);
		if(IsInfoLevel())
			puts("Starting TivaGdbAgent...\n");

		CTivaIcdi tiva;
		s_pTivaObj = &tiva;
		CGdbLink gdb_link(tiva);

		tiva.Open(gdb_link);
		SetConsoleCtrlHandler(CloseTiva, TRUE);
		if (IsInfoLevel())
			printf("Listening port %d\n", port);
		gdb_link.Serve(port);
		s_pTivaObj = NULL;	// object is going out of scope
		tiva.Close();
	}
	catch (CAtlException &e)
	{
		s_pTivaObj = NULL;	// object does not exists anymore
		_com_error err(e.m_hr);
		_ftprintf(stderr, _T("%s\n"), err.ErrorMessage());
		ATLTRACE(_T("%s\n"), err.ErrorMessage());
	}
	return EXIT_SUCCESS;
}

