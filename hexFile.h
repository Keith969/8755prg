#ifndef HEXFILE_H
#define HEXFILE_H

// *****************************************************************************
// File         [ hexFile.h ]
// Description  [ Implementation of the hexFile class ]
// Author       [ Keith Sabine ]
// *****************************************************************************

#include <QString>

class guiMainWindow;

// *****************************************************************************
// Class        [ hexDataChunk ]
// Description  [ A chunk of hex data ] 
// *****************************************************************************
class hexDataChunk
{
public:
	hexDataChunk():
		m_ByteCount(0),
		m_RecordType(0),
		m_Address(0),
		m_Checksum(0) {}
	~hexDataChunk() {}

	uint8_t                   byteCount();
	void                      setByteCount(uint8_t n);
	uint8_t                   recordType();
	void                      setRecordType(uint8_t n);
	uint16_t                  address();
	void                      setAddress(uint16_t n);
	uint8_t                   checkSum();
	void                      setCheckSum(uint8_t n);
	std::vector<uint8_t>    & data();
	void                      setData(const std::vector<uint8_t> &d);

private:
	uint8_t                   m_ByteCount;  // byte count
	uint8_t                   m_RecordType; // record type
	uint16_t                  m_Address;    // Address offset
	uint8_t                   m_Checksum;   // checksum
	std::vector<uint8_t>      m_Data;       // data
};


// *****************************************************************************
// Class        [ hexFile ]
// Description  [ ] 
// *****************************************************************************
class hexFile
{
public:
	hexFile() {}
	~hexFile() {}

	bool                      readHex(const QString& hexFileName);
	bool                      writeHex(const QString& hexFileName);
	void                      setMainWindow(guiMainWindow* win) { m_MainWindow = win; }
	guiMainWindow           * mainWindow() { return m_MainWindow; }

private:

	std::vector<hexDataChunk> m_HexData;     // The hex data
	guiMainWindow           * m_MainWindow;  // 
};

#endif /* HEXFILE_H */