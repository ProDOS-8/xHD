//
//  main.cpp
//  xHDServer  
//
//  Created by John Brooks on 7/1/16.
//  Copyright (c) 2016 JB.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <libserialport.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>

#if 0
//TODO: Switch xHD client to Z8530 built-in CRC calc
/*
 * http://wiki.synchro.net/ref:xmodem
 *
 * This function calculates the CRC used by the XMODEM/CRC Protocol
 * The first argument is a pointer to the message block.
 * The second argument is the number of bytes in the message block.
 * The function returns an integer which contains the CRC.
 * The low order 16 bits are the coefficients of the CRC.
 */
static unsigned
crc_xmodem(char *ptr, unsigned count)
{
	int crc, i;

	crc = 0;
	while (count-- > 0) {
		crc = crc ^ (unsigned)*ptr++ << 8;
		for (i = 0; i < 8; ++i)
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
	}
	return (crc & 0xFFFF);
}
#endif

int main(int argc, char *argv[])
{
	// Process the command line arguments
	bool bVerbose = false;
	bool bLog = true;
	char *pPortname = 0;
	char *apFilename[2] = {0, 0};

	for (int iArg = 1; iArg < argc; iArg++)
	{
		// Process option
		if (argv[iArg][0] == '-')
		{
			if (argv[iArg][1] == 'p')
			{
				pPortname = &argv[iArg][2];
				if (bVerbose)
					printf("Port: %s\n", pPortname);
				continue;
			}

			// Process flags
			for (int iChar = 1; argv[iArg][iChar]; iChar++)
			{
				switch (argv[iArg][iChar])
				{
				case 'v':
				{
					bVerbose = true;
					printf("Verbose: On\n");
					break;
				}
				case 'n':
				{
					bLog = false;
					if (bVerbose)
						printf("Logging: Off\n");
					break;
				}
				}
			}
			continue;
		}

		// Process disk image
		for (int iDrive = 0; iDrive < 2; iDrive++)
		{
			if (apFilename[iDrive] == 0)
			{
				apFilename[iDrive] = argv[iArg];
				if (bVerbose)
					printf("Drive %d: %s\n", iDrive, apFilename[iDrive]);
				break;
			}
		}
	}

	if (apFilename[0] == 0)
	{
		printf("Usage: %s [-nv] [-p<port>] img1 [img2]\n"
			   "\t-n\t\tno logging\n"
			   "\t-v\t\tbe verbose\n"
			   "\t-p<port>\t(partial) serial portname\n"
			   "\timg1\t\tdrive 1 disk image filename\n"
			   "\timg2\t\tdrive 2 disk image filename\n",
			   argv[0]);
		return EXIT_FAILURE;
	}

	// Memory-map the disk image file(s)
	char *apFileData[2] = {0, 0};
	for (int iDrive = 0; iDrive < 2; iDrive++)
	{
		if (apFilename[iDrive] == 0)
			continue;

		int fd = open(apFilename[iDrive], O_RDWR, S_IWRITE | S_IREAD);
		if (fd == -1)
		{
			printf("Err: Open %s - %s\n", apFilename[iDrive], strerror(errno));
			return EXIT_FAILURE;
		}

		struct stat stats;
		if (fstat(fd, &stats) == -1)
		{
			printf("Err: Stat %s - %s\n", apFilename[iDrive], strerror(errno));
			return EXIT_FAILURE;
		}
		unsigned int uFilesize = (unsigned int)stats.st_size;

		apFileData[iDrive] = (char*)mmap(0, uFilesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (apFileData[iDrive] == MAP_FAILED)
		{
			printf("Err: Map %s - %s\n", apFilename[iDrive], strerror(errno));
			return EXIT_FAILURE;
		}
	}
	
	// Create a configuration for the serial ports
	struct sp_port_config *pSerialConfig = 0;
	assert(sp_new_config(&pSerialConfig) == SP_OK);
	assert(sp_set_config_baudrate(pSerialConfig, 230400) == SP_OK);
	assert(sp_set_config_bits(pSerialConfig, 8) == SP_OK);
	assert(sp_set_config_parity(pSerialConfig, SP_PARITY_NONE) == SP_OK);
	assert(sp_set_config_stopbits(pSerialConfig, 1) == SP_OK);
	assert(sp_set_config_flowcontrol(pSerialConfig, SP_FLOWCONTROL_RTSCTS) == SP_OK);

	// Find and configure valid serial ports
	const int MAX_PORTS = 2;
	int iValidPortCount = 0;
	struct sp_port *apValidPorts[MAX_PORTS];
	enum sp_return eResult;
	{
		struct sp_port **ports;

		if (sp_list_ports(&ports) != SP_OK)
		{
			printf("Err: Port enum - %s\n", sp_last_error_message());
			return EXIT_FAILURE;
		}

		for (int i = 0; ports[i]; i++)
		{
			struct sp_port *pPort = ports[i];

			bool bValid = false;
			if (pPortname)
			{
				if (strstr(sp_get_port_name(pPort), pPortname))
					bValid = true;
			}
			else
			{
				if (sp_get_port_transport(pPort) == SP_TRANSPORT_USB)
					bValid = true;
			}

			if (bValid)
			{
				if (bVerbose)
				{
					printf("Valid port %d: %s\n", iValidPortCount, sp_get_port_name(pPort));
					printf("\tDescr: %s\n", sp_get_port_description(pPort));
					if (sp_get_port_transport(pPort) == SP_TRANSPORT_USB)
					{
						int iUsbBus = -1;
						int iUsbAddress = -1;
						if (sp_get_port_usb_bus_address(pPort, &iUsbBus, &iUsbAddress) == SP_OK)
							printf("\tUSB: Bus=%d, Address=%d\n", iUsbBus, iUsbAddress);
						int iUsbVid = -1;
						int iUsbPid = -1;
						if (sp_get_port_usb_vid_pid(pPort, &iUsbVid, &iUsbPid) == SP_OK)
							printf("\tUSB: VendorID=$%04X, ProductID=$%04X\n", iUsbVid, iUsbPid);
						printf("\tManufacturer: %s\n", sp_get_port_usb_manufacturer(pPort));
						printf("\tProduct: %s\n", sp_get_port_usb_product(pPort));
						printf("\tSerial #: %s\n", sp_get_port_usb_serial(pPort));
					}
				}

				// Retain the port and configure it's settings
				if (iValidPortCount < MAX_PORTS)
				{
					// Copy the valid port since the port list will be freed
					eResult = sp_copy_port(pPort, &apValidPorts[iValidPortCount++]);
					assert(eResult == SP_OK);

					// Use the copied port
					pPort = apValidPorts[iValidPortCount-1];

					// See if the port can be opened and configured
					eResult = sp_open(pPort, SP_MODE_READ_WRITE);
					if (eResult == SP_OK)
						eResult = sp_set_config(pPort, pSerialConfig);
					if (eResult != SP_OK)
					{
						// Port is not valid
						iValidPortCount--;
						sp_close(pPort);
						sp_free_port(pPort);
						if (bVerbose)
							printf("\n\t--- Could not open this port: %s ---\n\n", sp_last_error_message());
					}
				}
			}
			else // ! valid
				if (bVerbose)
					printf("\tskip\t'%s'\n", sp_get_port_name(pPort));
		}
		sp_free_port_list(ports);
	}

	if (iValidPortCount == 0)
	{
		printf("Err: No valid port\n");
		return EXIT_FAILURE;
	}

// TODO: Add server shutdown
	while (1)
	{
		unsigned char auReadBuf[256];

		for (int iIndex = 0; iIndex < iValidPortCount; iIndex++)
		{
			struct sp_port *pPort = apValidPorts[iIndex];

			int iTxLen = 0;
			int iBytesRead;

			// Look for a command byte
			iBytesRead = sp_blocking_read(pPort, auReadBuf, 1, 1/*ms timeout*/);

			if (iBytesRead > 0)
			{
				if (bVerbose)
					printf("P%d) Cmd %02X\n", iIndex, auReadBuf[0]);

				// Now read 16-bit block number and byte checksum
				iBytesRead = 1 + sp_blocking_read(pPort, auReadBuf + 1, 3, 1/*ms timeout*/);
				if (iBytesRead != 4)
					continue;
				if (bVerbose)
					for (int i = 0; i < iBytesRead; i++)
						printf("\t%02X '%c'\n", auReadBuf[i], auReadBuf[i] & 0x7f);

				int iChksum = auReadBuf[0] ^ auReadBuf[1] ^ auReadBuf[2];
				if (iChksum != auReadBuf[3])
				{
					printf ("--- Chksum failed ---   read=%02X,%02X,%02X,%02X, calc=%02X\n",
						auReadBuf[0], auReadBuf[1], auReadBuf[2], auReadBuf[3], iChksum);
					continue;
				}
				if (bVerbose)
					printf("\tChksum ok: %02X\n", auReadBuf[4]);

				// If 4x bytes were read and the checksum passed, process request
				int iBlock = auReadBuf[1] | (auReadBuf[2] << 8);
				int iDrive = (auReadBuf[0] >> 2) & 1;
				char *pDriveData = apFileData[iDrive];
				if (pDriveData == 0)
				{
					if (bLog)
					{
						printf("%d\tN\n", iDrive);
						fflush(stdout);
					}
					// TODO: Make this an I/O error on the client side
					continue;
				}
				pDriveData += iBlock << 9;

				// Read block
				if (auReadBuf[0] & 1)
				{
					// Echo command packet to confirm the request was received
					iTxLen = sp_blocking_write(pPort, auReadBuf, 4, 1/*ms timeout*/);
					if (iTxLen != 4)
						continue;

					// Send the requsted block
					iTxLen = sp_blocking_write(pPort, pDriveData, 512, 5/*ms timeout*/);
					if (iTxLen < 512)
					{
						printf("Err: WriteLen=%d\n",iTxLen);
						continue;
					}

					// Calc data checksum
					iChksum = 0;
					for (int i = 0; i < 512; i++)
						iChksum ^= pDriveData[i];

					// Send checksum
					iTxLen = sp_blocking_write(pPort, &iChksum, 1, 30/*ms timeout*/);
					if (iTxLen != 1)
						continue;

					if (bLog)
					{
						printf("%d\tR %5d\r", iDrive, iBlock);
						fflush(stdout);
					}
				}
				else // Write block
				{
					static unsigned char auBlockBuf[512 + 1];
					int iReadLen = sp_blocking_read(pPort, auBlockBuf, 512 + 1, 30/*ms timeout*/);
					assert(iReadLen == 512 + 1);

					iChksum = 0;
					for (int i = 0; i < 512; i++)
						iChksum ^= auBlockBuf[i];
					auReadBuf[3] = iChksum;

					iTxLen = sp_blocking_write(pPort, auReadBuf, 4, 1/*ms timeout*/);
					if (0) printf ("Wrt Hdr=%02X,%02X,%02X,%02X, calc=%02X, rcv=%02x\n",
						auReadBuf[0], auReadBuf[1], auReadBuf[2], auReadBuf[3], iChksum, auBlockBuf[512]);

					// Block data checksum matches, write it to disk
					if (iChksum == auBlockBuf[512])
						memcpy(pDriveData, auBlockBuf, 512);

					if (bLog)
					{
						printf("%d\t\t\t\tW %5d\r", iDrive, iBlock);
						fflush(stdout);
					}
				}
			}
		}
	}

	// Close and free valid ports
	sp_free_config (pSerialConfig);
	for (int i = 0; i < iValidPortCount; i++)
	{
		struct sp_port *pPort = apValidPorts[i];
		sp_close(pPort);
		sp_free_port(pPort);
	}
}
