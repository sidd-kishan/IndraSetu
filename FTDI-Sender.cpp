#include <iostream>
#include <iomanip>
#include <chrono>   // for std::chrono::milliseconds
#include <thread>   // for std::this_thread::sleep_for
#include <windows.h>
#include "FTD2XX.h" // must be in project include path
#include <stdlib.h>   // For _MAX_PATH definition
#include <stdio.h>
#include <malloc.h>

#define OneSector 1024 
#define SectorNum 2000 

/*** HELPER FUNCTION DECLARATIONS ***/

// Formats and writes FT_DEVICE_LIST_INFO_NODE structure to output
// stream object.
std::ostream& operator<<(std::ostream& os, const FT_DEVICE_LIST_INFO_NODE& device);

// Prints a formatted error message and terminates the program with
// status code EXIT_FAILURE.
void error(const char* message);

// Prints FT_STATUS and a formatted error message and terminates
// the program with status code EXIT_FAILURE.
void ft_error(FT_STATUS status, const char* message);

// Prints FT_STATUS and a formatted error message, closes `handle`,
// and terminates the program with status code EXIT_FAILURE.
// (Delegates to ft_error(FT_STATUS, const char*))
void ft_error(FT_STATUS status, const char* message, FT_HANDLE handle);


/*** MAIN PROGRAM ***/

int main()
{
	FT_STATUS status;
	UCHAR LatencyTimer = 255;
	DWORD EventDWord;
	DWORD RxBytes;
	DWORD TxBytes;
	DWORD BytesReceived;

	// --- Get number of devices ---

	unsigned long deviceCount = 0;

	status = FT_CreateDeviceInfoList(&deviceCount);

	if (status != FT_OK) {
		ft_error(status, "FT_CreateDeviceInfoList");
	}

	if (deviceCount == 0) {
		error("No FTDI devices found.");
	}

	// --- Enumerate devices ---

	FT_DEVICE_LIST_INFO_NODE* deviceInfos = new FT_DEVICE_LIST_INFO_NODE[deviceCount];

	status = FT_GetDeviceInfoList(deviceInfos, &deviceCount);
	// ... populates deviceInfos array

	if (status != FT_OK) {
		ft_error(status, "FT_GetDeviceInfoList");
	}

	// --- Find device of interest ---

	const unsigned long myID = 0x04036014;
	// ... from "Device Manager": Vendor ID = 0403, Product ID = 6014

	FT_DEVICE_LIST_INFO_NODE myDevice{};

	// Find first device that matches `myID`
	for (unsigned int i = 0; i < deviceCount; i++)
	{
		if (deviceInfos[i].ID == myID)
		{
			// Copy device info into `myDevice`
			myDevice = deviceInfos[i];

			// Open device
			status = FT_Open(i, &myDevice.ftHandle);

			if (status != FT_OK) {
				ft_error(status, "FT_Open");
			}

			break;
		}
	}

	// Handle device not found...
	if (myDevice.ID != myID)
	{
		std::cerr << "0 of " << deviceCount << " devices with ID " << std::hex << std::showbase << myID << " found:\n";

		for (unsigned int i = 0; i < deviceCount; i++)
		{
			std::cerr << "Device " << i << "\n";
			std::cerr << deviceInfos[i] << std::endl;
		}

		error("Device not found.");
	}

	std::cout << "Device found with ID " << std::hex << std::showbase << myID << ":\n";
	std::cout << myDevice << std::endl;

	// Clean up device info list

	delete[] deviceInfos;

	// --- Set up command buffers ---

	uint8_t *recvBuffer = (uint8_t*)std::malloc(2000 * 1024);// [2000][1024];
	unsigned long bytesWritten = 0;
	unsigned long bytesRead = 0;

	// --- Configure Port for MPSSE ---

	// This procedure comes from FTDI application note
	// "AN 135 - MPSSE Basics", "Section 4. Software Configuration".
	// It's not clear yet if all steps are required for Lab 3, but
	// Lab 3 works having followed these steps.
	//                                                - Jon 

	// Reset device

	status = FT_ResetDevice(myDevice.ftHandle);

	if (status != FT_OK) {
		ft_error(status, "FT_ResetDevice", myDevice.ftHandle);
	}

	// Set USB transfer sizes

	status = FT_SetUSBParameters(myDevice.ftHandle, 64, 64);

	if (status != FT_OK) {
		ft_error(status, "FT_SetUSBParameters", myDevice.ftHandle);
	}

	// Disable error characters

	status = FT_SetChars(myDevice.ftHandle, 0, 0, 0, 0);

	if (status != FT_OK) {
		ft_error(status, "FT_SetChars", myDevice.ftHandle);
	}

	// Set device read/write timeouts

	status = FT_SetTimeouts(myDevice.ftHandle, 1, 1);

	if (status != FT_OK) {
		ft_error(status, "FT_SetTimeouts", myDevice.ftHandle);
	}

	// Set timeout before flushing receive buffer

	status = FT_SetLatencyTimer(myDevice.ftHandle, 1);

	if (status != FT_OK) {
		ft_error(status, "FT_SetLatencyTimer", myDevice.ftHandle);
	}

	// Disable flow control
	status = FT_SetFlowControl(myDevice.ftHandle, FT_FLOW_NONE, 0, 0);

	if (status != FT_OK) {
		ft_error(status, "FT_SetFlowControl", myDevice.ftHandle);
	}

	// Reset MPSSE controller

	status = FT_SetBitMode(myDevice.ftHandle, 0, 0);

	if (status != FT_OK) {
		ft_error(status, "FT_SetBitMode", myDevice.ftHandle);
	}

	std::chrono::milliseconds(10);

	// Initialize MPSSE controller

	status = FT_SetBitMode(myDevice.ftHandle, 0xFF, 0x40); // All pins outputs, MPSSE

	if (status == FT_OK)
	{
		status = FT_SetLatencyTimer(myDevice.ftHandle, LatencyTimer);     
		status = FT_SetUSBParameters(myDevice.ftHandle, 0x10000, 0x10000);
		status = FT_SetFlowControl(myDevice.ftHandle, FT_FLOW_RTS_CTS, 0, 0);
		status = FT_Purge(myDevice.ftHandle, FT_PURGE_RX);
		//access data from here  
	}
	else if (status != FT_OK) {
		ft_error(status, "FT_SetBitMode", myDevice.ftHandle);
	}

	// --- Test bad command detection ---

	/* TODO */

	// --- Configure MPSSE ---

	/* TODO */

	// --- Write commands ---
	// * iterate through 0x00 to 0xFF and output value on AD[7:0] every 100ms

	uint8_t gpio_command[] = { 0x80, 0x00, 0xFF };
	// 0x80: Command to set AD[7:0].
	// 0x00: Output values for AD[7:0] (placeholder)
	// 0xFF: GPIO directions for AD[7:0] (1 = output)

	uint8_t value = 0x00;
	for (int j = 0; j < 2000; j++) {
		//value = (j % 2 == 0) ? 0xAA:0x55 ;
		for (int i = 0; i < 1024; i++) {
			*(recvBuffer + i + (j * 1024)) = value;
			value++;
			value &= 0xFF;
		}
	}
	char TxBuffer[OneSector];
	int i = 0;
	for (; i <= 936;) {
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0xFF;
		TxBuffer[i++] = 0x00;
		TxBuffer[i++] = 0x00;
	}
	//for (i = 0; i < 1024; i++)TxBuffer[i] = 0x00;
	// Infinite loop
	while (true)
	{
		
		//std::cout << "\rWriting: " << std::noshowbase << std::hex << std::setfill('0') << std::setw(2) << int(value);
		status = FT_GetStatus(myDevice.ftHandle, &RxBytes, &TxBytes, &EventDWord);
		
		if (status == FT_OK && (TxBytes == 0)) {
			status = FT_Write(myDevice.ftHandle, TxBuffer, 255, &BytesReceived);
			//status = FT_Write(myDevice.ftHandle, TxBuffer, 1024, &BytesReceived);
			if (status == FT_OK && BytesReceived)
			{
				std::cout << "\n Bytes Read " << BytesReceived;
				//status = FT_Purge(myDevice.ftHandle, FT_PURGE_RX);
				//status = FT_Purge(myDevice.ftHandle, FT_PURGE_TX);
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			else
			{
				// FT_Write Failed  
			}
		} else {
			ft_error(status, "FT_Write", myDevice.ftHandle);
		}

		//if (bytesWritten != sizeof(gpio_command)) {
		//	error("Error while writing to device.");
		//}

		// Sleep thread
		std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		//std::this_thread::sleep_for(std::chrono::seconds(1));
		
	}

	return 0;
}

/*** HELPER FUNCTION DEFINITIONS ***/

void error(const char* message)
{
	std::cerr << "\033[1;31m[ERROR]\033[0m " << message << std::endl;
	exit(EXIT_FAILURE);
}

void ft_error(FT_STATUS status, const char* message)
{
	std::cerr << "\033[1;31m[ERROR]\033[0m " << message << ": " << status << std::endl;
	exit(EXIT_FAILURE);
}

void ft_error(FT_STATUS status, const char* message, FT_HANDLE handle)
{
	FT_Close(handle);
	ft_error(status, message);
}

std::ostream& operator<<(std::ostream& os, const FT_DEVICE_LIST_INFO_NODE& device)
{
	os << "  @" << std::showbase << std::hex << &device << "\n";
	os << "  Flags=" << std::showbase << std::hex << device.Flags << "\n";
	os << "  Type=" << std::showbase << std::hex << device.Type << "\n";
	os << "  ID=" << std::showbase << std::hex << device.ID << "\n";
	os << "  LocId=" << std::showbase << std::hex << device.LocId << "\n";
	os << "  SerialNumber=" << device.SerialNumber << "\n";
	os << "  Description=" << device.Description << "\n";
	os << "  ftHandle=" << std::showbase << std::hex << device.ftHandle << "\n";

	return os;
}
