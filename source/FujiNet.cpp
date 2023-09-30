/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2022, Andrea Odetti

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <StdAfx.h>
#include <string>
#include <fstream>
#include <streambuf>

#include "YamlHelper.h"
#include "FujiNet.h"
#include "Interface.h"
#include "W5100.h"
#include "../Registry.h"

#include "StrFormat.h"
#include "Memory.h"
#include "Log.h"

#include <iostream>
#include <urlmon.h>
#pragma comment(lib,"urlmon.lib")

// Minimal appkey and json parse support
#define FUJICMD_WRITE_APPKEY 0xDE
#define FUJICMD_READ_APPKEY 0xDD
#define FUJICMD_READ_JSON 0xFE // Temporary for emulation only

const std::string& FujiNet::GetSnapshotCardName()
{
	static const std::string name("FujiNet");
	return name;
}

#define JSON_PARSE_MODE_FIND_VALUE 0
#define JSON_PARSE_MODE_BUILD_STRING 1
#define JSON_PARSE_MODE_BUILD_NUMBER 2

FujiNet::FujiNet(UINT slot) : Card(CT_FujiNet, slot)
{
	Reset(true);
}

FujiNet::~FujiNet()
{

}

void FujiNet::resetBuffer()
{
	memset(buffer, 0, W5100_MEM_SIZE);
	bufferLen = 0;
	bufferReadIndex = 0;
}

void FujiNet::Reset(const bool powerCycle)
{
	LogFileOutput("FujiNet Bridge Initialization\n");
	resetBuffer();

	if (powerCycle)
	{

	}
}

void FujiNet::readAppKey()
{
	std::string filename(StrFormat("ak_%02X%02X%02X%02X.txt", buffer[0], buffer[1], buffer[2], buffer[3]));
	resetBuffer();

	// read file into string
	std::ifstream file(filename.c_str());
	std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	// store string to buffer
	bufferLen = str.length();
	memcpy(buffer, str.c_str(), bufferLen);
}

void FujiNet::writeAppKey()
{
	std::string filename(StrFormat("ak_%02X%02X%02X%02X.txt", buffer[0], buffer[1], buffer[2], buffer[3]));

	// write file from buffer
	std::ofstream file(filename.c_str());
	file.write((const char*) &buffer[6], buffer[4] + (buffer[5] << 8));
	file.close();
}

void FujiNet::readJson()
{
	std::string url;
	url.assign((const char*)buffer, bufferLen);

	IStream* pStream;
	BYTE payload[32768];
	ULONG payloadLength;
	BYTE parseMode = JSON_PARSE_MODE_FIND_VALUE;

	// Download the HTTPS request into the buffer
	resetBuffer();
	HRESULT hr = URLOpenBlockingStream(nullptr, url.c_str(), &pStream, 0, nullptr);
	if (!FAILED(hr))
	{
		hr = pStream->Read(&payload, sizeof(payload), &payloadLength);
		pStream->Release();

		if (payloadLength > 0)
		{
			// Quick and dirty json extraction of key and value, one per line
			for (ULONG i = 0;i < payloadLength;i++)
			{
				BYTE c = payload[i];
				if (parseMode == JSON_PARSE_MODE_FIND_VALUE)
				{
					if (c == '"')
						parseMode = JSON_PARSE_MODE_BUILD_STRING;
					else if (c >= '0' && c <= '9')
					{
						parseMode = JSON_PARSE_MODE_BUILD_NUMBER;
						buffer[bufferLen++] = c;
					}
				}
				else
				{
					if (
						(parseMode == JSON_PARSE_MODE_BUILD_STRING && (c != '"')) ||
						(parseMode == JSON_PARSE_MODE_BUILD_NUMBER && ((c >= '0' && c <= '9') && c == '.' || c == '-'))
						)
					{
						buffer[bufferLen++] = c;
					}
					else
					{
						buffer[bufferLen++] = '\n';
						parseMode = JSON_PARSE_MODE_FIND_VALUE;
					}
				}
			}
		}
	}
}

// This handles communication between the Apple program and this card
BYTE FujiNet::IO(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	BYTE res = write ? 0 : MemReadFloatingBus(nCycles);
	const uint8_t loc = address & 0x0f;

	// Read from buffer
	// 0 = len lo byte, 1 = len hi byte, 2 = next data
	if (!write)
	{
		switch (loc)
		{
		case 0:
			res = (BYTE)(bufferLen & 0x00ff); 
			break;
		case 1:
			res = (BYTE)((bufferLen >> 8) && 0xff); 
			break;
		case 2:
			if (bufferReadIndex < bufferLen)
				res = buffer[bufferReadIndex++];
			break;
		}
		return res;
	}
	else
	{
		std::string filename;

		if (loc == 0) // Fill command data buffer
		{
			buffer[bufferLen++] = value;
		}
		else if (loc == 1) // Set command (or clear buffer if 0)
		{
			switch (value)
			{
			case 0: resetBuffer();						break;
			case FUJICMD_READ_APPKEY:	readAppKey();	break;
			case FUJICMD_WRITE_APPKEY:	writeAppKey();	break;
			case FUJICMD_READ_JSON:		readJson();		break;
			}
		}
	}

	return res;
}

BYTE __stdcall IOHandler(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	// Get 00_0 peripheral slot
	UINT uSlot = ((address & 0xf0) >> 4) - 8;

	if (uSlot < 8) {
		FujiNet* pCard = (FujiNet*)MemGetSlotParameters(uSlot);
		return pCard->IO(programcounter, address, write, value, nCycles);
	}
	return 0;
}

void FujiNet::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, IOHandler, IOHandler, nullptr, nullptr, this, nullptr);
}


void FujiNet::Update(const ULONG nExecutedCycles)
{
}


void FujiNet::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{

}

bool FujiNet::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	return true;
}
