
#include <Drivers/ScuGicDriver/ScuGicDriver.hpp>
#include <FreeRTOSConfig.h>

#define ERR_NO_ERROR 0

XScuGic ScuGicDriver::xInterruptController;

extern "C" uint32_t ConnectPsInterruptmFromC(Xil_InterruptHandler handler, void *CallBackRef, uint32_t int_id, E_SCUGIC_TRIGGER_TYPE trigger_type);
extern "C" void EnablePsInteruptFromC(uint32_t int_id);
extern "C" void DisablePsInterruptFromC(uint32_t int_id);

uint32_t ConnectPsInterruptmFromC(Xil_InterruptHandler handler, void *CallBackRef, uint32_t int_id, E_SCUGIC_TRIGGER_TYPE trigger_type)
{
	return ScuGicDriver::ConnectPsInterrupt(handler, CallBackRef, int_id, trigger_type);
}

void EnablePsInteruptFromC(uint32_t int_id)
{
	ScuGicDriver::EnablePsInterupt(int_id);
}

void DisablePsInterruptFromC(uint32_t int_id)
{
	ScuGicDriver::DisablePsInterrupt(int_id);
}

uint32_t ScuGicDriver::Init()
{
	XScuGic_Config* pxGICConfig = XScuGic_LookupConfig( XPAR_SCUGIC_SINGLE_DEVICE_ID );

	int32_t xStatus = XScuGic_CfgInitialize(&xInterruptController, pxGICConfig, pxGICConfig->CpuBaseAddress);
	if(XST_SUCCESS != xStatus){
		return 0;
	}

	xStatus = XScuGic_SelfTest(&xInterruptController);
	if(XST_SUCCESS != xStatus){
		return 0;
	}

	return ERR_NO_ERROR;
}

uint32_t ScuGicDriver::ConnectPsInterrupt(Xil_InterruptHandler handler, void *CallBackRef, uint32_t int_id, E_SCUGIC_TRIGGER_TYPE trigger_type)
{
	XScuGic_SetPriorityTriggerType( &xInterruptController, int_id, ((configMAX_API_CALL_INTERRUPT_PRIORITY + 1) << 3), trigger_type);

	int32_t xStatus = XScuGic_Connect( &xInterruptController, int_id, handler, CallBackRef);
	if(XST_SUCCESS != xStatus){
		return 0;
	}

	return ERR_NO_ERROR;
}

uint32_t ScuGicDriver::DisConnectInterrupt(uint32_t int_id)
{
	XScuGic_SetPriorityTriggerType(&xInterruptController, int_id, ((configMAX_API_CALL_INTERRUPT_PRIORITY + 1) << 3), E_ACTIVE_HIGH_LEVEL_SENSITIVE);

	XScuGic_Disconnect(&xInterruptController, int_id);

	return ERR_NO_ERROR;
}

void ScuGicDriver::EnablePsInterupt(uint32_t int_id)
{
	XScuGic_Enable(&xInterruptController, int_id);
}

void ScuGicDriver::DisablePsInterrupt(uint32_t int_id)
{
	XScuGic_Disable(&xInterruptController, int_id);
}

uint32_t ScuGicDriver::CreateSoftwareInterrupt(uint32_t int_id, uint32_t cpu_mask)
{
	uint32_t err = ERR_NO_ERROR;

	err = XScuGic_SoftwareIntr(&xInterruptController, int_id,cpu_mask);
	if (err != XST_SUCCESS) {
		return 0;
	}

	return ERR_NO_ERROR;
}


