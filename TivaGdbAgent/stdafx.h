// stdafx.h : arquivo de inclusão para inclusões do sistema padrões,
// ou inclusões específicas de projeto que são utilizadas frequentemente, mas
// são modificadas raramente
//

#pragma once

#ifndef WINVER
#define WINVER 0x0600
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0502
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <WinSock2.h>
#include <windows.h>

//Pull in support for ATL
#include <atlbase.h>

//Pull in support for STL
#include <string>
#include <vector>

#include <winusb.h>
#include <setupapi.h>
//#include <initguid.h>
#include <usbiodef.h>

#include <comdef.h>
#include <atlstr.h>

#include "WinUSBWrappers/WinUSBWrappers.h"


// TODO: adicionar referências de cabeçalhos adicionais que seu programa necessita
