#include <Drivers/qspiPsDriver/qspiPsDriver.hpp>
#include <Drivers/qspiPsDriver/qspiPsDriverDefs.hpp>


XQspiPs 			ZynqQspiDriver::QspiInstance;
XQspiPs_Config* 	ZynqQspiDriver::QspiConfig;
bool 				ZynqQspiDriver::TransferInProgress;
SemaphoreHandle_t	ZynqQspiDriver::xQspiSemaphore;
uint8_t				ZynqQspiDriver::WriteBuffer[ADDRESS_3_OFFSET];
ZynqQspiDriver::ZynqQspiDriver() {
    // Constructor implementation can be empty if there's nothing to initialize beforehand
}

ZynqQspiDriver::~ZynqQspiDriver() {
    // Disable QSPI
    XQspiPs_Disable((&QspiInstance));
}

uint32_t ZynqQspiDriver::init() {
    uint32_t Status;
    // Initialize the QSPI driver so that it's ready to use
    QspiConfig = XQspiPs_LookupConfig(XPAR_XQSPIPS_0_DEVICE_ID);
    if (nullptr == QspiConfig) {
        return XST_FAILURE;
    }

    Status = XQspiPs_CfgInitialize(&QspiInstance, QspiConfig, QspiConfig->BaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    XQspiPs_SetOptions(&QspiInstance, XQSPIPS_MANUAL_START_OPTION | XQSPIPS_FORCE_SSELECT_OPTION | XQSPIPS_CLK_PHASE_1_OPTION);
    XQspiPs_SetClkPrescaler(&QspiInstance, XQSPIPS_CLK_PRESCALE_8);
    XQspiPs_SetSlaveSelect(&QspiInstance);



    return XST_SUCCESS;
}

uint32_t ZynqQspiDriver::write(uint32_t Address, uint8_t* p_data, size_t ByteCount) {
	/*
	 * Buraya eklenmesi gereken assertler:
	 * - Address XQSPIPS_FLASH_OPCODE_BE_4K tam kati olmali.
	 * - Address + ByteCount MAX_DATA'dan buyuk olmamali
	 */

	while( ByteCount >= PAGE_SIZE) {
		writePage(Address, p_data, PAGE_SIZE);
		p_data += PAGE_SIZE;
		Address += PAGE_SIZE;
		ByteCount -= PAGE_SIZE;
	}
	if( ByteCount > 0) {
		writePage(Address, p_data, ByteCount);
	}
    return XST_SUCCESS;
}

uint32_t ZynqQspiDriver::read(uint32_t Address, uint8_t* p_data, size_t ByteCount) {
	/*
	 * Setup the write command with the specified address and data for the
	 * FLASH
	 */
	uint8_t Command = XQSPIPS_FLASH_OPCODE_NORM_READ;
	WriteBuffer[COMMAND_OFFSET]   = Command;
	WriteBuffer[ADDRESS_1_OFFSET] = (u8)((Address & 0xFF0000) >> 16);
	WriteBuffer[ADDRESS_2_OFFSET] = (u8)((Address & 0xFF00) >> 8);
	WriteBuffer[ADDRESS_3_OFFSET] = (u8)(Address & 0xFF);

	if ((Command == XQSPIPS_FLASH_OPCODE_FAST_READ) || (Command == XQSPIPS_FLASH_OPCODE_DUAL_READ) || (Command == XQSPIPS_FLASH_OPCODE_QUAD_READ))
	{
		ByteCount += DUMMY_SIZE;
	}
	/*
	 * Send the read command to the FLASH to read the specified number
	 * of bytes from the FLASH, send the read command and address and
	 * receive the specified number of bytes of data in the data buffer
	 */
//	TransferInProgress = TRUE;

	XQspiPs_Transfer(&QspiInstance, WriteBuffer, NULL, OVERHEAD_SIZE);

	/*
	 * Wait for the transfer on the QSPI bus to be complete before
	 * proceeding
	 */
	xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//	while (TransferInProgress);

	XQspiPs_Transfer(&QspiInstance, NULL, p_data, ByteCount);

	xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);

	return XST_SUCCESS;
}

uint32_t ZynqQspiDriver::erase(uint32_t Address, size_t ByteCount) {
	/*
	 * Buraya eklenmesi gereken assertler:
	 * - address SUBSECTOR_SIZE tam kati olmali
	 * - ByteCount SUBSECTOR_SIZE tam kati olmali
	 */

	u8 WriteEnableCmd = { XQSPIPS_FLASH_OPCODE_WREN };
	u8 ReadStatusCmd[] = { XQSPIPS_FLASH_OPCODE_RDSR1, 0 };  /* must send 2 bytes */
	u8 FlashStatus[2];
//	int Sector;

	/*
	 * If erase size is same as the total size of the flash, use bulk erase
	 */
	if (ByteCount == (NUM_SECTORS * SECTOR_SIZE)) {
		/*
		 * Send the write enable command to the FLASH so that it can be
		 * written to, this needs to be sent as a separate transfer
		 * before the erase
		 */
//		TransferInProgress = TRUE;

		XQspiPs_Transfer(&QspiInstance, &WriteEnableCmd, NULL,
				  sizeof(WriteEnableCmd));

		/*
		 * Wait for the transfer on the QSPI bus to be complete before
		 * proceeding
		 */
		xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//		while (TransferInProgress);

		/* Setup the bulk erase command*/
		WriteBuffer[COMMAND_OFFSET] = XQSPIPS_FLASH_OPCODE_BE;

		/*
		 * Send the bulk erase command; no receive buffer is specified
		 * since there is nothing to receive
		 */
//		TransferInProgress = TRUE;
		XQspiPs_Transfer(&QspiInstance, WriteBuffer, NULL,
				  BULK_ERASE_SIZE);

		xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//		while (TransferInProgress);

		/* Wait for the erase command to the FLASH to be completed*/
		while (1) {
			/*
			 * Poll the status register of the device to determine
			 * when it completes, by sending a read status command
			 * and receiving the status byte
			 */
//			TransferInProgress = TRUE;

			XQspiPs_Transfer(&QspiInstance, ReadStatusCmd, FlashStatus,
					  sizeof(ReadStatusCmd));

			/*
			 * Wait for the transfer on the QSPI bus to be complete
			 * before proceeding
			 */
			xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//			while (TransferInProgress);

			/*
			 * If the status indicates the write is done, then stop
			 * waiting; if a value of 0xFF in the status byte is
			 * read from the device and this loop never exits, the
			 * device slave select is possibly incorrect such that
			 * the device status is not being read
			 */
			FlashStatus[1] |= FlashStatus[0];
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}
		return XST_SUCCESS;
	}

	/*
	 * If the erase size is less than the total size of the flash, use
	 * sector erase command
	 */

	while( ByteCount > SECTOR_SIZE ) {
		/*
		 * Send the write enable command to the SEEPOM so that it can be
		 * written to, this needs to be sent as a separate transfer
		 * before the write
		 */
//		TransferInProgress = TRUE;

		XQspiPs_Transfer(&QspiInstance, &WriteEnableCmd, NULL,
				  sizeof(WriteEnableCmd));

		/*
		 * Wait for the transfer on the QSPI bus to be complete before
		 * proceeding
		 */
		xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//		while (TransferInProgress);

		/*
		 * Setup the write command with the specified address and data
		 * for the FLASH
		 */
		if(Address % SUBSECTOR_SIZE != 0)
		{
			WriteBuffer[COMMAND_OFFSET]   = XQSPIPS_FLASH_OPCODE_BE_4K;
		}
		else
		{
			WriteBuffer[COMMAND_OFFSET]   = XQSPIPS_FLASH_OPCODE_SE;
		}
		WriteBuffer[COMMAND_OFFSET]   = XQSPIPS_FLASH_OPCODE_SE;
		WriteBuffer[ADDRESS_1_OFFSET] = (u8)(Address >> 16);
		WriteBuffer[ADDRESS_2_OFFSET] = (u8)(Address >> 8);
		WriteBuffer[ADDRESS_3_OFFSET] = (u8)(Address & 0xFF);

		/*
		 * Send the sector erase command and address; no receive buffer
		 * is specified since there is nothing to receive
		 */
//		TransferInProgress = TRUE;
		XQspiPs_Transfer(&QspiInstance, WriteBuffer, NULL,
				  SEC_ERASE_SIZE);

		xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//		while (TransferInProgress);

		/*
		 * Wait for the sector erase command to the
		 * FLASH to be completed
		 */
		while (1) {
			/*
			 * Poll the status register of the device to determine
			 * when it completes, by sending a read status command
			 * and receiving the status byte
			 */
//			TransferInProgress = TRUE;

			XQspiPs_Transfer(&QspiInstance, ReadStatusCmd, FlashStatus,
					  sizeof(ReadStatusCmd));

			/*
			 * Wait for the transfer on the QSPI bus to be complete
			 * before proceeding
			 */
			xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//			while (TransferInProgress);

			/*
			 * If the status indicates the write is done, then stop
			 * waiting, if a value of 0xFF in the status byte is
			 * read from the device and this loop never exits, the
			 * device slave select is possibly incorrect such that
			 * the device status is not being read
			 */
			FlashStatus[1] |= FlashStatus[0];
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}

		if(Address % SUBSECTOR_SIZE != 0)
		{
			Address += SUBSECTOR_SIZE;
			ByteCount -= SUBSECTOR_SIZE;
		}
		else
		{
			Address += SECTOR_SIZE;
			ByteCount -= SECTOR_SIZE;
		}
	}

	while( ByteCount > SUBSECTOR_SIZE ) {
		/*
		 * Send the write enable command to the SEEPOM so that it can be
		 * written to, this needs to be sent as a separate transfer
		 * before the write
		 */
//		TransferInProgress = TRUE;

		XQspiPs_Transfer(&QspiInstance, &WriteEnableCmd, NULL,
				  sizeof(WriteEnableCmd));

		/*
		 * Wait for the transfer on the QSPI bus to be complete before
		 * proceeding
		 */
		xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//		while (TransferInProgress);

		/*
		 * Setup the write command with the specified address and data
		 * for the FLASH
		 */
		WriteBuffer[COMMAND_OFFSET]   = XQSPIPS_FLASH_OPCODE_BE_4K;
		WriteBuffer[ADDRESS_1_OFFSET] = (u8)(Address >> 16);
		WriteBuffer[ADDRESS_2_OFFSET] = (u8)(Address >> 8);
		WriteBuffer[ADDRESS_3_OFFSET] = (u8)(Address & 0xFF);

		/*
		 * Send the sector erase command and address; no receive buffer
		 * is specified since there is nothing to receive
		 */
//		TransferInProgress = TRUE;
		XQspiPs_Transfer(&QspiInstance, WriteBuffer, NULL,
				  SEC_ERASE_SIZE);

		xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//		while (TransferInProgress);

		/*
		 * Wait for the sector erase command to the
		 * FLASH to be completed
		 */
		while (1) {
			/*
			 * Poll the status register of the device to determine
			 * when it completes, by sending a read status command
			 * and receiving the status byte
			 */
//			TransferInProgress = TRUE;

			XQspiPs_Transfer(&QspiInstance, ReadStatusCmd, FlashStatus,
					  sizeof(ReadStatusCmd));

			/*
			 * Wait for the transfer on the QSPI bus to be complete
			 * before proceeding
			 */
			xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//			while (TransferInProgress);

			/*
			 * If the status indicates the write is done, then stop
			 * waiting, if a value of 0xFF in the status byte is
			 * read from the device and this loop never exits, the
			 * device slave select is possibly incorrect such that
			 * the device status is not being read
			 */
			FlashStatus[1] |= FlashStatus[0];
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}

		Address += SECTOR_SIZE;
		ByteCount -= SECTOR_SIZE;
	}
	return XST_SUCCESS;
}

void ZynqQspiDriver::LinearModeEnable() {
    // Placeholder implementation
}

void ZynqQspiDriver::LinearModeDisable() {
    // Placeholder implementation
}

uint32_t ZynqQspiDriver::writePage(uint32_t Address, uint8_t* p_data, uint32_t ByteCount)
{
	u8 WriteEnableCmd = { XQSPIPS_FLASH_OPCODE_WREN };
	u8 ReadStatusCmd[] = { XQSPIPS_FLASH_OPCODE_RDSR1, 0 };  /* must send 2 bytes */
	u8 FlashStatus[2];

	/*
	 * Send the write enable command to the FLASH so that it can be
	 * written to, this needs to be sent as a separate transfer before
	 * the write
	 */
//	TransferInProgress = TRUE;

	XQspiPs_Transfer(&QspiInstance, &WriteEnableCmd, NULL,
			  sizeof(WriteEnableCmd));

	/*
	 * Wait for the transfer on the QSPI bus to be complete before
	 * proceeding
	 */
	xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//	while (TransferInProgress);

	/*
	 * Setup the write command with the specified address and data for the
	 * FLASH
	 */
	WriteBuffer[COMMAND_OFFSET]   = XQSPIPS_FLASH_OPCODE_PP;
	WriteBuffer[ADDRESS_1_OFFSET] = (u8)((Address & 0xFF0000) >> 16);
	WriteBuffer[ADDRESS_2_OFFSET] = (u8)((Address & 0xFF00) >> 8);
	WriteBuffer[ADDRESS_3_OFFSET] = (u8)(Address & 0xFF);

	/*
	 * Send the write command, address, and data to the FLASH to be
	 * written, no receive buffer is specified since there is nothing to receive
	 */
//	TransferInProgress = TRUE;
	XQspiPs_Transfer(&QspiInstance, WriteBuffer, NULL, OVERHEAD_SIZE);

	xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);

	XQspiPs_Transfer(&QspiInstance, p_data, NULL, ByteCount);

	xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//	while (TransferInProgress);

	/*
	 * Wait for the write command to the FLASH to be completed, it takes
	 * some time for the data to be written
	 */
	while (1) {
		/*
		 * Poll the status register of the FLASH to determine when it
		 * completes, by sending a read status command and receiving the
		 * status byte
		 */
//		TransferInProgress = TRUE;

		XQspiPs_Transfer(&QspiInstance, ReadStatusCmd, FlashStatus,
				  sizeof(ReadStatusCmd));

		/*
		 * Wait for the transfer on the QSPI bus to be complete before
		 * proceeding
		 */
		xSemaphoreTake(xQspiSemaphore, portMAX_DELAY);
//		while (TransferInProgress);

		/*
		 * If the status indicates the write is done, then stop waiting,
		 * if a value of 0xFF in the status byte is read from the
		 * device and this loop never exits, the device slave select is
		 * possibly incorrect such that the device status is not being
		 * read
		 */
		FlashStatus[1] |= FlashStatus[0];
		if ((FlashStatus[1] & 0x01) == 0) {
			break;
		}
	}
	return XST_SUCCESS;
}

uint32_t ZynqQspiDriver::setupIntr()
{
	ScuGicDriver::ConnectPsInterrupt((Xil_InterruptHandler)XQspiPs_InterruptHandler, (void*)QspiHandler, XPAR_XQSPIPS_0_INTR, E_SCUGIC_TRIGGER_TYPE::E_ACTIVE_HIGH_LEVEL_SENSITIVE);
}

void ZynqQspiDriver::QspiHandler(void *CallBackRef, u32 StatusEvent, unsigned int ByteCount)
{
	/*
	 * Indicate the transfer on the QSPI bus is no longer in progress
	 * regardless of the status event
	 */
	static BaseType_t xHigherPriorityTaskWoken;
	xSemaphoreGiveFromISR(xQspiSemaphore, &xHigherPriorityTaskWoken);
//	TransferInProgress = FALSE;

	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	/* If the event was not transfer done, then track it as an error*/
	if (StatusEvent != XST_SPI_TRANSFER_DONE) {
//		Error++;
	}
}
