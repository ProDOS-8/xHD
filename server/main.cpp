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
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>

#define PORT_STR	"usbserial"

#define VERBOSE		(0)
#define LOG			(1)

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

int main(void)
{
// TODO: Parse cmd line for switches & disk image filenames

	unsigned char auReadBuf[256] = {0};
	enum sp_return eResult;

	// Map virtual drives
	const char *apFilename[2] = {"/Applications/GSport/HD/P1NEW.PO", "/Applications/GSport/HD/P2.PO"};
	//const char *apFilename[2] = {"/Applications/GSport/HD/P1NEW.PO", "/Applications/GSport/Disk5.25/ProDOS_2_5a1.PO"};
	
	char * apFileData[2];
	for (int iDrive=0; iDrive<2; iDrive++)
	{
		int fd = open(apFilename[iDrive], O_RDWR, S_IWRITE | S_IREAD);
		struct stat stats;
		fstat(fd, &stats);
		unsigned int uFilesize = (unsigned int)stats.st_size;
		apFileData[iDrive] = (char*) mmap(0, uFilesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	}
	
	// Create a configuration for the serial ports
	struct sp_port_config *pSerialConfig=0;
	{
		eResult = sp_new_config(&pSerialConfig);
		assert(eResult == SP_OK);
		eResult = sp_set_config_baudrate(pSerialConfig, 230400);
		assert(eResult == SP_OK);
		eResult = sp_set_config_bits(pSerialConfig, 8);
		assert(eResult == SP_OK);
		eResult = sp_set_config_parity(pSerialConfig, SP_PARITY_NONE );
		assert(eResult == SP_OK);
		eResult = sp_set_config_stopbits(pSerialConfig, 1);
		assert(eResult == SP_OK);
		eResult = sp_set_config_flowcontrol(pSerialConfig, SP_FLOWCONTROL_RTSCTS );
		assert(eResult == SP_OK);
	}

	// Find and configure valid serial ports
	const int MAX_PORTS = 2;
	int iValidPortCount = 0;
	struct sp_port * apValidPorts[MAX_PORTS];
	{
		struct sp_port **ports;

		eResult = sp_list_ports(&ports);
		assert(eResult == SP_OK);
		
		for (int i = 0; ports[i]; i++)
		{
			struct sp_port *pPort = ports[i];
			
			if (strstr(sp_get_port_name(pPort), PORT_STR) )
			{
				if (VERBOSE)
				{
					printf("Valid port %d: %s\n", iValidPortCount, sp_get_port_name(pPort) );
					printf("\tDescr: %s\n", sp_get_port_description(pPort));
					int iUsbBus = -1;
					int iUsbAddress = -1;
					eResult = sp_get_port_usb_bus_address(pPort, &iUsbBus, &iUsbAddress);
					assert(eResult == SP_OK);
					printf("\tUSB: Bus=%d, Address=%d\n", iUsbBus, iUsbAddress);
					int iUsbVid = -1;
					int iUsbPid = -1;
					eResult = sp_get_port_usb_vid_pid(pPort, &iUsbVid, &iUsbPid);
					assert(eResult == SP_OK);
					printf("\tUSB: VendorID=$%04X, ProductID=$%04X\n", iUsbVid, iUsbPid);
					printf("\tManufacturer: %s\n", sp_get_port_usb_manufacturer(pPort));
					printf("\tProduct: %s\n", sp_get_port_usb_product(pPort));
					printf("\tSerial #: %s\n", sp_get_port_usb_serial(pPort));
					
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
					enum sp_return eResultOpen;
					enum sp_return eResultConfig;
					eResultOpen = sp_open(pPort, SP_MODE_READ_WRITE);
					eResultConfig = sp_set_config(pPort, pSerialConfig);
					if (eResultOpen != SP_OK || eResultConfig != SP_OK)
					{
						// Port is not valid
						iValidPortCount--;
						sp_close(pPort);
						sp_free_port(pPort);
						if (VERBOSE)
						{
							printf("\n\t--- Could not open this port ---\n\n");
						}
					}
				}
			}
			else // ! usbserial
			{
				if (VERBOSE)
					printf("\tskip\t'%s'.\n", sp_get_port_name(pPort));
			}
		}
		sp_free_port_list(ports);
	}

// TODO: Add server shutdown
	while (1)
	{
		for (int iIndex = 0; iIndex < iValidPortCount; iIndex++)
		{
			struct sp_port *pPort = apValidPorts[iIndex];

			int iBytesToRead = sp_input_waiting(pPort);
			int iWriteLen = 0;
#if 0
			if (iBytesToRead == 0)
			{
				usleep(100);
			}
			if (iBytesToRead > 0)
#endif
			{
				int iBytesRead;
#if 1
				auReadBuf[0] = 0;
				iBytesRead = sp_blocking_read(pPort, auReadBuf+1, 1, 1/*ms timeout*/);
				
				if (iBytesRead > 0)
				{
					{
						if (VERBOSE)
						{
							printf("P%d) Cmd\n", iIndex);
						}
						iBytesRead = 2 + sp_blocking_read(pPort, auReadBuf+2, 3, 1/*ms timeout*/);
						if (iBytesRead != 5) continue;
#endif
#if 0
				//iBytesRead = sp_blocking_read(pPort, auReadBuf, 1, 10/*ms timeout*/);
				iBytesRead = sp_blocking_read(pPort, auReadBuf, 1, 0/*ms timeout*/);
				assert(iBytesRead == 1); if (iBytesRead != 1) continue;
				
				switch(auReadBuf[0])
				{
					case 0xC5:	// E
					{
						if (VERBOSE)
						{
							printf("P%d) Cmd E\n", iIndex);
						}
						iBytesRead = sp_blocking_read(pPort, auReadBuf+1, 4, 10/*ms timeout*/);
						assert(iBytesRead == 4); if (iBytesRead != 4) continue;
						iBytesRead++;
#endif
						if (VERBOSE)
						{
							for (int i = 1; i < iBytesRead; i++)
							{
								printf("\t%02X '%c'\n", auReadBuf[i], auReadBuf[i] & 0x7f);
							}
						}
						int iChksum = auReadBuf[0] ^ auReadBuf[1] ^ auReadBuf[2] ^ auReadBuf[3];
						if (iChksum != auReadBuf[4])
						{
							printf ("--- Chksum failed ---   read=%02X,%02X,%02X,%02X,%02X, calc=%02X\n",
								auReadBuf[0],auReadBuf[1],auReadBuf[2],auReadBuf[3],auReadBuf[4],
								iChksum);
							continue;
						}
						else
						{
							if (VERBOSE)
							{
								printf("\tChksum ok: %02X\n", auReadBuf[4]);
							}
							
							int iBlock = auReadBuf[2] | (auReadBuf[3] << 8);
							char *pDriveData = apFileData[(auReadBuf[1] >> 2) & 1];
							pDriveData += iBlock * 512;

							// Read block
							if (auReadBuf[1] & 1)
							{
#if 1
								iChksum = 0;
								for (int i = 1; i < 4; i++)
									iChksum ^= auReadBuf[i];
								auReadBuf[4] = iChksum;

								iWriteLen = sp_blocking_write(pPort, auReadBuf+1, 4, 10/*ms timeout*/);
								assert(iWriteLen == 4); if (iWriteLen != 4) continue;
#endif
#if 0
								iChksum = 0;
								for (int i = 0; i < 4; i++)
									iChksum ^= auReadBuf[i];
								auReadBuf[4] = iChksum;

								iWriteLen = sp_blocking_write(pPort, auReadBuf, 5, 10/*ms timeout*/);
								assert(iWriteLen == 5); if (iWriteLen != 5) continue;
#endif
#if 0
								time_t t = time(0);   // get time now
								struct tm * pDateTime = localtime( & t );
								int iDate = ((pDateTime->tm_year-100) << 9)
											| (pDateTime->tm_mon << 5)
											| pDateTime->tm_mday;

								auReadBuf[4] = pDateTime->tm_min;	// TimeLo=Minute
								auReadBuf[5] = pDateTime->tm_hour;	// TimeHi=Hour
								auReadBuf[6] = iDate & 0xff;		// DateLo
								auReadBuf[7] = (iDate >> 8);		// DateHi

								iChksum = 0;
								for (int i = 0; i < 8; i++)
									iChksum ^= auReadBuf[i];
								auReadBuf[8] = iChksum;

								iWriteLen = sp_blocking_write(pPort, auReadBuf, 9, 10/*ms timeout*/);
								assert(iWriteLen == 9); if (iWriteLen != 9) continue;
#endif

								static unsigned char auSendBuf[513];
								iWriteLen = sp_blocking_write(pPort, pDriveData, 512, 50/*ms timeout*/);
								if (iWriteLen < 512)
									printf("WriteLen=%d\n",iWriteLen);
								assert(iWriteLen == 512); if (iWriteLen != 512) continue;

								if (iWriteLen == 512)
								{
									iChksum = 0;
									for (int i = 0; i < 512; i++)
										iChksum ^= pDriveData[i];
									iWriteLen = sp_blocking_write(pPort, &iChksum, 1, 10/*ms timeout*/);
									assert(iWriteLen == 1);  if (iWriteLen != 1) continue;
									
									if (LOG)
									{
										printf("%d\tR %5d\r", (auReadBuf[1] >> 2) & 1, iBlock);
										fflush(stdout);
									}
								}
							}
							else // Write block
							{
								static unsigned char auBlockBuf[513];
								int iReadLen = sp_blocking_read(pPort, auBlockBuf, 512+1, 30/*ms timeout*/);
								assert(iReadLen == 512+1);

								iChksum = 0;
								for (int i = 0; i < 512; i++)
									iChksum ^= auBlockBuf[i];
								auReadBuf[4] = iChksum;	

#if 1
								iWriteLen = sp_blocking_write(pPort, auReadBuf+1, 4, 1/*ms timeout*/);
								printf ("Wrt Hdr=%02X,%02X,%02X,%02X,%02X, calc=%02X, rcv=%02x\n",
									auReadBuf[0],auReadBuf[1],auReadBuf[2],auReadBuf[3],auReadBuf[4],
									iChksum, auBlockBuf[512]);
								//assert(iWriteLen == 4);
#endif
#if 0								
								iWriteLen = sp_blocking_write(pPort, auReadBuf, 5, 10/*ms timeout*/);
								assert(iWriteLen == 5);
#endif

								// Block data checksum matches, write it to disk
								if (iChksum == auBlockBuf[512])
								{
									memcpy(pDriveData, auBlockBuf, 512);
								}

								if (LOG)
								{
									printf("%d\t\t\t\tW %5d\r", (auReadBuf[1] >> 2) & 1, iBlock);
									fflush(stdout);
								}
							}
						}
						break;
					}
#if 0					
					default:
					{
						printf("P%d) Bad cmd: %02X\n", iIndex, auReadBuf[0]);
						sp_flush(pPort, SP_BUF_INPUT);
						break;
					}
#endif
	
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
