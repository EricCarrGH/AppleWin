#pragma once

#include "Card.h"

#include <vector>
#include <map>
#include "W5100.h"

/*
* Documentation from
*   
*/

class FujiNet : public Card
{
public:
    static const std::string& GetSnapshotCardName();

    enum PacketDestination { HOST, BROADCAST, OTHER };

    FujiNet(UINT slot);
    virtual ~FujiNet();

    virtual void Destroy(void) {}
    virtual void InitializeIO(LPBYTE pCxRomPeripheral);
    virtual void Reset(const bool powerCycle);
    virtual void Update(const ULONG nExecutedCycles);
    virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
    virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

    BYTE IO(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);
private:

    void resetBuffer();
    void readAppKey();
    void writeAppKey();
    void readJson();

    BYTE buffer[W5100_MEM_SIZE];
    unsigned long bufferLen;
    int bufferReadIndex;
};
