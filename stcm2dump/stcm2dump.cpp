// Will o' Wisp DS - http://www.otomate.jp/will_psp_ds/

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <algorithm>
#include <vector>

using namespace std;

struct _STCM2Header
{
	uint8_t head[0x20];
	uint32_t offsetToExports;
	uint32_t numOfExports;

} STCM2Header;

typedef struct _STCM2Export
{
	uint32_t valid;
	uint8_t exportName[0x20];
	uint32_t exportOffset;
} STCM2Export;

vector<STCM2Export*> exports;

ifstream infile;

int startOffset = -1;
vector<uint32_t> parsedFuncs;

FILE *bytecodeFile = NULL;
FILE *textFile = NULL;
int textId = 0;

void ReadFunction(uint32_t functionOffset)
{
	vector<uint32_t> funcs;
	vector<bool> isFunction;
	
	funcs.push_back(functionOffset);
	isFunction.push_back(true);

	for(uint32_t i = 0; i < funcs.size(); i++)
	{
		uint32_t unk1 = 0, opcode = 0, argCount = 0, sectionSize = 0;

		bool alreadyParsed = false;
		for(uint32_t x = 0; x < parsedFuncs.size(); x++)
		{
			if(parsedFuncs[x] == funcs[i])
			{
				alreadyParsed = true;
				break;
			}
		}

		if(alreadyParsed)
			continue;

		infile.seekg(funcs[i], ios_base::beg);
		parsedFuncs.push_back(funcs[i]);
		
		infile.read((char*)&unk1, 0x04);
		infile.read((char*)&opcode, 0x04);
		infile.read((char*)&argCount, 0x04);
		infile.read((char*)&sectionSize, 0x04);
		
		for(uint32_t x = 0; x < exports.size(); x++)
		{
			if(funcs[i] == exports[x]->exportOffset)
			{
				fprintf(bytecodeFile, "\r\n@@@%s:\r\n", exports[x]->exportName);
			}
		}

		fprintf(bytecodeFile, "@@off_%x: ",funcs[i]);

		if(unk1 == 1 && sectionSize >= 0x10)
		{
			fprintf(bytecodeFile, "{%08x} @off_%x@ {%08x %08x} ", unk1, opcode, argCount, sectionSize);

			if(find(parsedFuncs.begin(), parsedFuncs.end(), opcode) == parsedFuncs.end())
			{
				funcs.push_back(opcode);
				isFunction.push_back(true);
			}
		}
		else
		{
			fprintf(bytecodeFile, "{%08x %08x %08x %08x} ", unk1, opcode, argCount, sectionSize);
		}

		if(isFunction[i])
		{
			for(uint8_t i = 0; i < argCount; i++)
			{
				int32_t a = 0, b = 0, c = 0;
				
				infile.read((char*)&a, 0x04);
				infile.read((char*)&b, 0x04);
				infile.read((char*)&c, 0x04);
				
				streamoff prevOffset = infile.tellg();
	
				if(a >= 0 && a >= startOffset)
				{
					funcs.push_back(a);
					isFunction.push_back(false);
					fprintf(bytecodeFile, "@off_%x@ ",a);
				}
				else
				{
					fprintf(bytecodeFile, "{%08x} ",a);
				}
				
				if(a == 0xffffff41 && b >= startOffset)
				{
					funcs.push_back(b);
					isFunction.push_back(true);
					fprintf(bytecodeFile, "@off_%x@ ",b);
				}
				else
				{
					fprintf(bytecodeFile, "{%08x} ",b);
				}
	
				if(c >= 0 && c >= startOffset)
				{
					fprintf(bytecodeFile, "c: %08x\r\n",c);
					exit(1);
	
					funcs.push_back(c);
					isFunction.push_back(false);
					fprintf(bytecodeFile, "@off_%x@ ",c);
				}
				else
				{
					fprintf(bytecodeFile, "{%08x} ",c);
				}
	
				infile.seekg(prevOffset, ios_base::beg);
			}
		}
		else
		{
			if(opcode > 1 && opcode < 0x70)
			{
				// text
				uint8_t *text = new uint8_t[sectionSize];
				infile.read((char*)text, sectionSize);

				fprintf(bytecodeFile, "<%08d> ", textId);
				fprintf(textFile, "//<%08d> %s\r\n",textId, text);
				fprintf(textFile, "<%08d> %s\r\n\r\n",textId, text);
				textId++;

				delete text;
			}
			else
			{
				// data
				uint8_t *data = new uint8_t[sectionSize];
				infile.read((char*)data, sectionSize);

				for(uint32_t x = 0; x < sectionSize; x++)
					fprintf(bytecodeFile, "{%02x} ", data[x]);

				delete data;
			}
		}

		fprintf(bytecodeFile, "\r\n");
	}
}

void ReadSection(uint32_t sectionOffset)
{
	uint32_t nextBlock = sectionOffset;
	while(nextBlock + 0x10 < STCM2Header.offsetToExports)
	{
		uint32_t unk1 = 0, opcode = 0, argCount = 0, sectionSize = 0;

		infile.seekg(nextBlock, ios_base::beg);
		
		infile.read((char*)&unk1, 0x04);
		infile.read((char*)&opcode, 0x04);
		infile.read((char*)&argCount, 0x04);
		infile.read((char*)&sectionSize, 0x04);
		
		ReadFunction(nextBlock);

		nextBlock += sectionSize;
	}
}

int main(int argc, char **argv)
{
	if(argc != 4)
	{
		printf("usage: %s in.dat out-scr.txt out-dialog.txt", argv[0]);
		return 0;
	}

	infile.open(argv[1], ifstream::binary);

	if(!infile.is_open())
	{
		cout << "Could not open " << argv[1] << endl;
		return -1;
	}
	
	bytecodeFile = fopen(argv[2], "wb");
	textFile = fopen(argv[3], "wb");
	
	memset(STCM2Header.head, '\0', 0x20);
	infile.read((char*)&STCM2Header.head, 0x20);
	infile.read((char*)&STCM2Header.offsetToExports, 0x04);
	infile.read((char*)&STCM2Header.numOfExports, 0x04);

	if(memcmp(STCM2Header.head, "\x53\x54\x43\x4D", 4) != 0)
	{
		cout << "Not a valid STCM file" << endl;
		return -4;
	}

	/*
	for(uint32_t i = 0; i < 0x20; i++)
		fprintf(bytecodeFile, "{%02x} ", STCM2Header.head[i]);
	fprintf(bytecodeFile, "\r\n");
	*/

	infile.seekg(STCM2Header.offsetToExports, ios_base::beg);
	
	for(uint32_t i = 0; i < STCM2Header.numOfExports; i++)
	{
		STCM2Export *exportInfo = new STCM2Export;
		
		memset(exportInfo->exportName, '\0', 0x20);
		infile.read((char*)&exportInfo->valid, 0x04);
		infile.read((char*)&exportInfo->exportName, 0x20);
		infile.read((char*)&exportInfo->exportOffset, 0x04);

		exports.push_back(exportInfo);
	}

	infile.seekg(0x50, ios_base::beg);

	int globalData = 0;
	infile.read((char*)&globalData,0x04);

	if(globalData != 0x424f4c47)
	{
		cout << "Invalid GLOBAL_DATA section" << endl;
		return -3;
	}
	
	infile.seekg(0x5c, ios_base::beg);
	fprintf(bytecodeFile, "\n[GLOBAL_DATA]\r\n");
	while(!infile.eof())
	{
		int data = 0;
		infile.read((char*)&data,0x04);
		
		if(data == 0x45444f43)
		{
			infile.seekg(-0x04, ios_base::cur);
			break;
		}

		fprintf(bytecodeFile, "{%08x}\r\n", data);
	}

	int codeStart = 0;
	infile.read((char*)&codeStart, 0x04);

	if(codeStart != 0x45444f43)
	{
		cout << "Could not find CODE_START_ section" << endl;
		return -2;
	}

	startOffset = (int)infile.tellg() + 8;
	
	fprintf(bytecodeFile, "\n[CODE_START_]\r\n");
	ReadSection(startOffset);

	infile.close();

	return 0;
}
