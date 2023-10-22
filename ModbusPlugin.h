#ifndef MODBUSPLUGIN_H
#define MODBUSPLUGIN_H

#include <QObject>
#include <QComboBox>
#include <QtPlugin>
#include <QTcpSocket>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>
#include <QMutexLocker>

#include "ui_Modbus.h"
#include "../interface.h"

#ifdef Q_OS_WIN32
#include "windows.h"
#include "Winsock2.h"
#endif

#ifdef Q_OS_LINUX
#include "sys/socket.h"
#endif

static const char STX = 0x02;			// Frame Start
static const char ETX = 0x03;			// Frame End
static const char EOT = 0x04;			// Frame Interrupted
static const char LF = 0x0A;			// Line Feed
static const char SP = 0x20;			// Space
static const char CR = 0x0D;			// Carriage Return
static const int DataTimeOut = 60000;


enum NetTraffic
{
    Connecting, Waitingforanswer, Disconnected, Connected, Paused
};


/* Table of CRC values for high-order byte */
const quint8 table_crc_hi[256] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Table of CRC values for low-order byte */
const quint8 table_crc_lo[256] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

//enum registerType { coil = 0, discreteInput, inputRegister, holdingRegister };
enum registerType { inputRegister = 0, holdingRegister };
enum registerTypeDef { uint16b = 0, int16b, uint32bMSDFirst, uint32bMSDLast };
enum readInterval { readConstant, read1mn, read2mn, read5mn, read10mn, read30mn, read1hour };

static uint16_t calcCRC16(QByteArray s)
{
    quint8 crc_hi = 0xFF;
    quint8 crc_lo = 0xFF;
    unsigned int i;
    foreach (uint8_t t, s)
    {
        i = crc_hi ^ t;
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
  }
  quint16 result = quint16(crc_hi << 8 | crc_lo);
  return result;
}

class comboType : public QComboBox
{
    Q_OBJECT
public:
    comboType() {
        addItem("Holding Register");
        addItem("Input Register");
        //addItem("Coil");
        //addItem("Discrete Input");
    }
};

class comboInterval : public QComboBox
{
    Q_OBJECT
public:
    comboInterval() {
        addItem("Constant");
        addItem("1mn");
        addItem("2mn");
        addItem("5mn");
        addItem("10mn");
        addItem("30mn");
        addItem("1 hour");
    }
};

class comboTypedef : public QComboBox
{
    Q_OBJECT
public:
    comboTypedef() {
        addItem("16b unsigned");
        addItem("16b signed");
        addItem("32b MSD First");
        addItem("32b MSD Last");
    }
};

enum requestType { reqRead = 0, reqWrite };


enum FunctionCode {
    Invalid = 0x00,
    ReadCoils = 0x01,
    ReadDiscreteInputs = 0x02,
    ReadHoldingRegisters = 0x03,
    ReadInputRegisters = 0x04,
    WriteSingleCoil = 0x05,
    WriteSingleRegister = 0x06,
    ReadExceptionStatus = 0x07,
    Diagnostics = 0x08,
    GetCommEventCounter = 0x0B,
    GetCommEventLog = 0x0C,
    WriteMultipleCoils = 0x0F,
    WriteMultipleRegisters = 0x10,
    ReportServerId = 0x11,
    ReadFileRecord = 0x14,
    WriteFileRecord = 0x15,
    MaskWriteRegister = 0x16,
    ReadWriteMultipleRegisters = 0x17,
    ReadFifoQueue = 0x18,
    EncapsulatedInterfaceTransport = 0x2B,
    ReadErrorCode = 0x84,
    WriteErrorCode = 0x86,
    UndefinedFunctionCode = 0x100
};

struct modbusItem {
    QString RomID;
    int id = 0;
    QTableWidgetItem *slave, *address, *maxValue;
    comboType *type;
    comboInterval *readInterval;
    comboTypedef *typeDef;
    QTableWidgetItem *value, *lastRead;
    modbusItem(QString romid) : RomID(romid)
    {
        bool ok;
        id = RomID.right(5).left(3).toInt(&ok);
        slave = new QTableWidgetItem("1");
        address = new QTableWidgetItem("1");
        type = new comboType;
        readInterval = new comboInterval;
        typeDef = new comboTypedef;
        value = new QTableWidgetItem();
        value->setFlags(value->flags() ^ Qt::ItemIsEditable);
        lastRead = new QTableWidgetItem();
        lastRead->setFlags(lastRead->flags() ^ Qt::ItemIsEditable);
        type->setCurrentIndex(0);
        readInterval->setCurrentIndex(0);
        maxValue = new QTableWidgetItem("65535");
    }
    bool operator==(const modbusItem& dev)
    {
        bool ok;
        // Check if slave are equal
        int S1 = slave->text().toInt(&ok);
        if (!ok) return false;
        int S2 = dev.slave->text().toInt(&ok);
        if (!ok) return false;
        if (S1 != S2) return false;

        // Check if request type are the same
        if (type->currentIndex() != dev.type->currentIndex()) return false;

        // Set the size of the first device
        int s = 1;
        switch (typeDef->currentIndex()) {
        case uint16b : s = 1; break;
        case int16b : s = 1; break;
        case uint32bMSDFirst : s = 2; break;
        case uint32bMSDLast : s = 2; break; }

        // Check if address are consecutive
        //int A1 = address->text().toInt(&ok) + 1;
        int A1 = address->text().toInt(&ok) + s;
        if (!ok) return false;
        int A2 = dev.address->text().toInt(&ok);
        if (!ok) return false;
        if (A1 != A2) return false;

        return true;
    }
};

struct modbusReadRequest {
    modbusReadRequest(quint16 Slave, quint16 Type, quint16 Address, quint16 DataSize) : slave(Slave), type(Type), address(Address), size(DataSize)
    {
        switch (Type) {
        case 0 : type = ReadHoldingRegisters; break;
        case 1 : type = ReadInputRegisters; break;
        }
    }
    quint16 slave;
    quint16 type;
    quint16 address;
    quint16 size;
    QByteArray buffer;
    bool operator==(const modbusReadRequest& req)
    {
        bool result = (slave == req.slave) && (type == req.type) && (address == req.address) && (size == req.size);
        return result;
    }
};

struct modbusWriteRequest {
    modbusItem *dev;
    quint16 address;
    QByteArray req;
    QString value;
    bool operator==(const modbusWriteRequest& r)
    {
        if (req.size() < 4) return false;
        if (r.req.size() < 4) return false;
        for (int n=0; n<4; n++) {
            if (req.at(n) != r.req.at(n)) return false;
        }
        return true;
    }
};


class tcpSocket : public QThread
{
    Q_OBJECT
public:
QString ip, command, log;
quint16 port = 1470;
bool endLessLoop = false;
bool idle = true;
private:
QTcpSocket *socket;
QList<modbusReadRequest*>readReqList;
QList<modbusWriteRequest*>writeReqList;
QByteArray responseBuffer;
QMutex mutexReqList;
#define Aborts 3
int aborted = Aborts;
requestType rType = reqRead;


#define SearchLoopBegin												\
int index_a = 0, index_b = 0, index = 0;									\
QString strsearch;												\
index_a = configdata.indexOf("\n" + TAG_Begin, 0);								\
do															\
    {														\
    if (index_a != -1)											\
        {													\
         index_b = configdata.indexOf("\n" + TAG_End, index_a);					\
         if (index_b != -1)										\
        {													\
            strsearch = configdata.mid(index_a, index_b - index_a);


#define SearchLoopEnd												\
        }													\
            }												\
        index_a = configdata.indexOf("\n" + TAG_Begin, index_b);					\
        index ++;												\
        }													\
    while(index_a != -1);


public:
    tcpSocket() = default;
    void run()
    {
    QTcpSocket Socket;
    socket = &Socket;
    Socket.moveToThread(this);
    qintptr sd = Socket.socketDescriptor();
#ifdef Q_OS_LINUX
    int set = 1;
    setsockopt(int(sd), SOL_SOCKET, MSG_NOSIGNAL, (const char *)&set, sizeof(int));
#endif
    QObject::connect(socket, &QAbstractSocket::stateChanged, [this]() { int state = socket->state(); emit(tcpStatusChange(state)); });
    QObject::connect(socket, &QIODevice::readyRead, [this](){
        responseBuffer += socket->readAll();
        emit(logTCP("get : " + responseBuffer.toHex(' ').toUpper()));
        if ((responseBuffer.size() > 3) && (!readReqList.isEmpty() || !writeReqList.isEmpty()))
        {
            //quint8 slave = responseBuffer.at(0);
            //qDebug() << "function = " + responseBuffer.mid(1,1).toHex().toUpper();
            quint16 address = 0;
            quint8 function = responseBuffer.at(1);
            quint8 dataCount = responseBuffer.at(2);
            if (rType == reqRead) {
                if (!readReqList.isEmpty()) {
                    address = readReqList.first()->address; } }
            if (rType == reqWrite) {
                if (!writeReqList.isEmpty()) {
                    address = writeReqList.first()->address; } }
            //emit(logTCP(QString("Buffer size %1").arg(responseBuffer.size())));
            switch (function) {

                case ReadHoldingRegisters :
                case ReadInputRegisters :
                    if (responseBuffer.size() == dataCount + 5) // enough data
                    {
                        if (checkCRC16())
                        {
                            responseBuffer.prepend(address);
                            responseBuffer.prepend(address >> 8);
                            endReadTransaction();
                        }
                        else readTransactionRetry();
                    }
                    else    // wait for more data
                    {
                        if (socket->waitForReadyRead(5000)) emit(logTCP(QString("Not enough Data Buffer size %1").arg(responseBuffer.size())));
                        else readRequestTimeOut();
                    }
                break;

            case WriteSingleRegister :
                if (responseBuffer.size() == 8)  {  // enough data
                    if (checkCRC16()) endWriteTransaction();
                    else writeTransactionRetry(); }
                else {   // wait for more data
                    if (socket->waitForReadyRead(5000)) emit(logTCPWr(QString("Not enough Data Buffer size %1").arg(responseBuffer.size())));
                    else writeRequestTimeOut();
                }
                break;
            case WriteMultipleRegisters :
                if (responseBuffer.size() == 8) { // enough data
                        if (checkCRC16()) endWriteTransaction();
                        else  writeTransactionRetry(); }
                else {  // wait for more data
                    if (socket->waitForReadyRead(5000)) emit(logTCPWr(QString("Not enough Data Buffer size %1").arg(responseBuffer.size())));
                    else writeRequestTimeOut();
                }
                break;

            case ReadErrorCode :   //   01 84 01 82 C0
                emit(logTCP("Read error code " + responseBuffer.mid(2,1).toHex().toUpper()));
                responseBuffer.clear();
                removeFirstReqList();    //if (!readReqList.isEmpty()) delete readReqList.takeFirst();
                break;

            case WriteErrorCode :   //   01 84 01 82 C0
                emit(logTCPWr("Write error code " + responseBuffer.mid(2,1).toHex().toUpper()));
                responseBuffer.clear();
                if (!writeReqList.isEmpty()) delete writeReqList.takeFirst();
                break;

            default :
                emit(logTCP(QString("Not yet done for function %1").arg(function)));
                responseBuffer.clear();
                removeFirstReqList();    //if (!readReqList.isEmpty()) delete readReqList.takeFirst();
                break;
            }
        }
    });

    emit(logTCP("Modbus start"));
    while (endLessLoop) {
        if (Socket.state() == QAbstractSocket::ConnectedState) {
            Socket.disconnectFromHost();
            Socket.waitForDisconnected(5000); }
        if (endLessLoop) {
            emit(logTCP("Try to connect to " + ip + QString(":%1").arg(port)));
            Socket.connectToHost(ip, port);
            Socket.waitForConnected(5000);
        }
        while ((Socket.state() == QAbstractSocket::ConnectedState) && endLessLoop) {
            if (!writeReqList.isEmpty()) {
                msleep(100);
                idle = false;
                QString reqHex = writeReqList.first()->req.toHex(' ').toUpper();
                if (socket->isValid())
                {
                    emit(logTCPWr("write request : " + reqHex));
                    rType = reqWrite;
                    socket->write(writeReqList.first()->req);
                }
                if (!Socket.waitForReadyRead(5000)) {
                    emit(logTCPWr("resend write request : " + reqHex));
                    rType = reqWrite;
                    socket->write(writeReqList.first()->req);
                    if (!Socket.waitForReadyRead(5000)) {
                            emit(logTCPWr("write request aborted : " + reqHex));
                            if (!writeReqList.isEmpty()) delete writeReqList.takeFirst();
                            responseBuffer.clear();
                    }
                }
            }
            else if (!readReqList.isEmpty())
            {
                idle = false;
                QByteArray req;
                build_read_request(readReqList.first(), req);
                QString reqHex = req.toHex(' ').toUpper();
                if (socket->isValid())
                {
                    emit(logTCP("send : " + reqHex));
                    rType = reqRead;
                    socket->write(req);
                }
                if (!Socket.waitForReadyRead(5000)) {
                    emit(logTCP("resend : " + reqHex));
                    rType = reqRead;
                    socket->write(req);
                    if (!Socket.waitForReadyRead(5000)) {
                            emit(logTCP("request aborted : " + reqHex));
                            removeFirstReqList();    //if (!readReqList.isEmpty()) delete readReqList.takeFirst();
                            responseBuffer.clear();
                    }
                }
            }
            else {
                if (idle == false) {
                    idle = true;
                }
            if (endLessLoop) sleep(1);
            }
        }
        responseBuffer.clear();
        readReqList.clear();
        writeReqList.clear();
    }
    Socket.disconnectFromHost();
    if (Socket.state() != QAbstractSocket::UnconnectedState) Socket.waitForDisconnected();
    if (socket != nullptr) QObject::disconnect(socket);
    log.clear();
    emit(logTCP("finished"));
    socket = nullptr;
    }

    void setReadRequest(modbusReadRequest *r) {
        mutexReqList.lock();
        bool found = false;
        foreach (modbusReadRequest *req, readReqList) {
            if (*r == *req) found = true; }
        if (!found) readReqList.append(r);
        mutexReqList.unlock();
    }

    inline void endWriteTransaction() {
        emit(write(responseBuffer));
        emit(logTCPWr("get : " + responseBuffer.toHex(' ').toUpper()));
        if (!writeReqList.isEmpty()) delete writeReqList.takeFirst();
        responseBuffer.clear();
        aborted = Aborts; }

    inline void removeFirstReqList() {
        mutexReqList.lock();
        if (!readReqList.isEmpty()) delete readReqList.takeFirst();
        mutexReqList.unlock();
    }

    inline void readRequestTimeOut() {
        removeFirstReqList();
        responseBuffer.clear();
        aborted = Aborts; }

    inline void writeRequestTimeOut() {
        delete writeReqList.takeFirst();
        responseBuffer.clear();
        aborted = Aborts; }

    inline void readTransactionRetry() {
        emit(logTCP("CRC bad"));
        responseBuffer.clear();
        aborted --;
        if (aborted <= 0) {
            emit(logTCP("request aborted"));
            removeFirstReqList();
            responseBuffer.clear();
            aborted = Aborts;
        } }

    inline void endReadTransaction() {
        emit(read(responseBuffer));
        removeFirstReqList();
        responseBuffer.clear();
        aborted = Aborts;
    }

    inline void writeTransactionRetry() {
        emit(logTCPWr("CRC bad"));
        responseBuffer.clear();
        aborted --;
        if (aborted <= 0) {
            emit(logTCPWr("request aborted"));
            if (!writeReqList.isEmpty()) delete writeReqList.takeFirst();
            responseBuffer.clear();
            aborted = Aborts; } }

    bool checkCRC16()
    {
        QByteArray crc = responseBuffer.right(2);
        quint16 crc_received = uchar(crc.at(0)) << 8 | uchar(crc.at(1));
        responseBuffer.resize(responseBuffer.size() - 2);
        quint16 crc_calc = calcCRC16(responseBuffer);
        if (crc_calc != crc_received) return false;
        return true;
    }

    QAbstractSocket::SocketState tcpState() {
        if (socket) return socket->state();
        else return QAbstractSocket::UnconnectedState;
    }

    void abort() {
        endLessLoop = false;
        sleep(1); }


    void build_read_request(modbusReadRequest *r, QByteArray &req)
    { // 01 04 00 37 00 01 80 04
        req.append(r->slave);
        req.append(r->type);
        req.append(r->address >> 8);
        req.append(r->address & 0x00ff);
        req.append(r->size >> 8);
        req.append(r->size & 0x00ff);
        quint16 crc = calcCRC16(req);
        req.append(crc >> 8);
        req.append(crc & 0x00ff);
    //qDebug() << "req : " + req.toHex().toUpper();
    }

    // https://www.modbustools.com/modbus.html

public slots:
    void setWriteRequest(modbusWriteRequest *r) {
        bool found = false;
        foreach (modbusWriteRequest *req, writeReqList) {
            if (*r == *req) found = true;
        }
        if (!found) writeReqList.append(r);

    }

signals:
    void tcpStatusChange(int);
    void read(QByteArray);
    void write(QByteArray);
    void logTCP(QString);
    void logTCPWr(QString);
};

class ModbusPlugin : public QWidget, LogisDomInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "logisdom.network.LogisDomInterface/1.0" FILE "ModbusPlugin.json")
    Q_INTERFACES(LogisDomInterface)
public:
    ModbusPlugin( QWidget *parent = nullptr );
    ~ModbusPlugin() override;
    QObject* getObject() override;
    QWidget *widgetUi() override;
    QWidget *getDevWidget(QString RomID) override;
    void setConfigFileName(const QString) override;
    void readDevice(const QString &device) override;
    QString getDeviceCommands(const QString &device) override;
    void saveConfig() override;
    void readConfig() override;
    void setLockedState(bool) override;
    QString getDeviceConfig(QString) override;
    QString getName() override;
    void setDeviceConfig(const QString &, const QString &) override;
    void setStatus(const QString) override;
    bool acceptCommand(const QString) override;
    bool isDimmable(const QString) override;
    bool isManual(const QString) override;
    double getMaxValue(const QString) override;
    QTimer readTimer;
    int idle = 0;
signals:
    void newDeviceValue(QString, QString) override;
    void newDevice(LogisDomInterface*, QString) override;
    void deviceSelected(QString) override;
    void updateInterfaceName(LogisDomInterface*, QString) override;
    void connectionStatus(QAbstractSocket::SocketState) override;
    void addRequest(modbusWriteRequest *);
private:
    QWidget *ui;
    QWidget *devUi;
    QString configFileName;
    QIcon tcpIconUnconnectedState, tcpIconHostLookupState, tcpIconConnectingState, tcpIconConnectedState, tcpIconClosingState;
    QList<modbusItem*> modbusDevices;
    void setipaddress(const QString &adr);
    void setport(const QString &adr);
    QString ip2Hex(const QString &ip);
    QString buildRomID(int n);
    Ui::ModbusUI *mui;
    QString lastStatus;
    void log(const QString);
    void logWr(const QString);
    QString logStr;
    tcpSocket socketThread;
    QTcpSocket *socket;
    QStringList Paramters, Values;
    bool isIpValid(QString str);
    bool isPortValid(QString str);
    void updateIpRomID();
    void checkImport();
    void sortDevicesList();
    void sortDevicesId();
    modbusItem *getRomID(quint16 slave, quint16 address);
    modbusItem *addDevice(QString id = "");
    void setReadRequest(modbusItem *, int size);
    int typeToSize(int type);
    int lastMinute = -1;
    void readDevices(QList<modbusItem*>&);
    int accepted = -1;
private slots:
    void tcpStatusChange(int);
    void read(QByteArray);
    void write(QByteArray);
    void logTCP(QString);
    void logWrTCP(QString);
    void showLog();
    void on_AddButton_clicked();
    void on_RemoveButton_clicked();
    void editIP_editingFinished();
    void on_editPort_editingFinished();
    void on_ReadButton_clicked();
    void on_ReadAllButton_clicked();
    void readAllNow();
    void displayDevice(int, int);
    void checkCombo();
    void on_editName_editingFinished();
};


#endif

