#pragma once

#include <xscugic.h>

typedef enum{
	E_ACTIVE_HIGH_LEVEL_SENSITIVE	= 0x01,
	E_RISING_EDGE_SENSITIVE			= 0x03,
}E_SCUGIC_TRIGGER_TYPE;


class ScuGicDriver{

public:

static uint32_t Init();
static uint32_t ConnectPsInterrupt(Xil_InterruptHandler handler, void *CallBackRef, uint32_t int_id, E_SCUGIC_TRIGGER_TYPE trigger_type);
static uint32_t DisConnectInterrupt(uint32_t int_id);
static void EnablePsInterupt(uint32_t int_id);
static void DisablePsInterrupt(uint32_t int_id);
static uint32_t CreateSoftwareInterrupt(uint32_t int_id, uint32_t cpu_mask);

private:

static XScuGic xInterruptController; 	/* Interrupt controller instance */
};

