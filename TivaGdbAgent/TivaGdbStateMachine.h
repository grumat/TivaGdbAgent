#pragma once

#include "GdbDispatch.h"


//! Fixes differences between ICDI link and GDB
class CTivaGdbStateMachine : public CGdbStateMachine
{
public:
	CTivaGdbStateMachine(IGdbDispatch &handler);
	~CTivaGdbStateMachine();

public:
	//! Intercepts transmit link to know how to process ICDI responses
	void InterceptTransmitLink(const CGdbStateMachine &gdbRequest);

protected:
	//! Fixes protocol differences between ICDI and GDB
	void OnBeforeDispatch() override;

private:
	bool m_fDumpRegisters;
};

