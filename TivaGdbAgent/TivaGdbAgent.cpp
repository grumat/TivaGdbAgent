// TivaGdbAgent.cpp : Define o ponto de entrada para a aplicação de console.
//

#include "stdafx.h"
#include "TivaIcdi.h"
#include "GdbLink.h"


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


int main(int argc, char *argv[])
{
	try
	{
		CTivaIcdi tiva;
		s_pTivaObj = &tiva;
		CGdbLink gdb_link(tiva);

		tiva.Open(gdb_link);
		SetConsoleCtrlHandler(CloseTiva, TRUE);
		gdb_link.Serve(7777);
		s_pTivaObj = NULL;	// object is going out of scope
		tiva.Close();
	}
	catch (CAtlException &e)
	{
		s_pTivaObj = NULL;	// object does not exists anymore
		_com_error err(e.m_hr);
		_tprintf(_T(" %s\n"), err.ErrorMessage());
	}

	return EXIT_SUCCESS;
}

