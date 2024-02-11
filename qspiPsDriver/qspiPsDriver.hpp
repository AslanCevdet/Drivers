#pragma once
#include <qspiPsDriver/qspiPsDriverDefs.hpp>
#include <ScuGicDriver/ScuGicDriver.hpp>
#include <xqspips.h> // Include Xilinx QSPI driver API
#include <FreeRTOS.h>
#include <semphr.h>

class ZynqQspiDriver {
public:
    ZynqQspiDriver();
    ~ZynqQspiDriver();

    uint32_t init();
    uint32_t write(uint32_t Address, uint8_t* p_data, size_t ByteCount);
    uint32_t read(uint32_t Address, uint8_t* p_data, size_t ByteCount);
    uint32_t erase(uint32_t Address, size_t ByteCount);
    void LinearModeEnable();
    void LinearModeDisable();

private:
    static uint32_t writePage(uint32_t Address, uint8_t* p_data, uint32_t ByteCount);
    static uint32_t setupIntr();
    static void QspiHandler(void *CallBackRef, u32 StatusEvent, unsigned int ByteCount);
    static XQspiPs QspiInstance; // Instance of the XQspiPs
    static XQspiPs_Config *QspiConfig;
    static bool TransferInProgress;
    static SemaphoreHandle_t xQspiSemaphore;
    static uint8_t WriteBuffer[ADDRESS_3_OFFSET];
};
