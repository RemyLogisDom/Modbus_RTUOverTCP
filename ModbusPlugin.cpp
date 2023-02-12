#include <QtWidgets>
#include <QTcpSocket>


#include <QtCore/qmath.h>
#include <QtCore/qpointer.h>
#include <QtCore/qqueue.h>
#include <QtCore/qtimer.h>
#include "ModbusPlugin.h"
#include "../common.h"

/*
 1.01 Correctif sequence de lecture 16 et 32 et operateur == pour prendre en compte les enchainement 32b
 */

ModbusPlugin::ModbusPlugin(QWidget *parent) : QWidget(parent)
{
    ui = new QWidget();
    QGridLayout *layout = new QGridLayout(ui);
    QWidget *w = new QWidget();
    mui = new Ui::ModbusUI;
    mui->setupUi(w);
    layout->addWidget(w);
    mui->checkBoxWr10->setToolTip(tr("Only Write Multiple register 0x10 function will be used\ninstead of Single 0x06 write command"));
    mui->spinBoxIdle->setToolTip(tr("Waiting delay for constant reading"));
    mui->editName->setToolTip("ModbusPlugin");
    mui->deviceTable->setColumnCount(9);
    mui->deviceTable->setHorizontalHeaderItem(0, new QTableWidgetItem("RomID"));
    mui->deviceTable->setHorizontalHeaderItem(1, new QTableWidgetItem("Address"));
    mui->deviceTable->setHorizontalHeaderItem(2, new QTableWidgetItem("Slave"));
    mui->deviceTable->setHorizontalHeaderItem(3, new QTableWidgetItem("Type"));
    mui->deviceTable->setHorizontalHeaderItem(4, new QTableWidgetItem("Resolution"));
    mui->deviceTable->setHorizontalHeaderItem(5, new QTableWidgetItem("Read Interval"));
    mui->deviceTable->setHorizontalHeaderItem(6, new QTableWidgetItem("Value"));
    mui->deviceTable->setHorizontalHeaderItem(7, new QTableWidgetItem("Last Read"));
    mui->deviceTable->setHorizontalHeaderItem(8, new QTableWidgetItem("Max Value"));
    //mui->checkBoxLog->setCheckState(Qt::Checked);
    connect(mui->AddButton, SIGNAL(clicked()), this, SLOT(on_AddButton_clicked()));
    connect(mui->RemoveButton, SIGNAL(clicked()), this, SLOT(on_RemoveButton_clicked()));
    connect(mui->ReadButton, SIGNAL(clicked()), this, SLOT(on_ReadButton_clicked()));
    connect(mui->ReadAllButton, SIGNAL(clicked()), this, SLOT(on_ReadAllButton_clicked()));
    connect(mui->editIP, SIGNAL(editingFinished()), this, SLOT(editIP_editingFinished()));
    connect(mui->editPort, SIGNAL(editingFinished()), this, SLOT(on_editPort_editingFinished()));
    connect(mui->editName, SIGNAL(editingFinished()), this, SLOT(on_editName_editingFinished()));
    connect(mui->deviceTable->itemDelegate(),SIGNAL(closeEditor(QWidget*,QAbstractItemDelegate::EndEditHint)),SLOT(checkCombo()));
    connect(mui->deviceTable, SIGNAL(cellPressed(int, int)),SLOT(displayDevice(int, int)));
    connect(&readTimer, SIGNAL(timeout()),SLOT(readAllNow()));
    connect(this, SIGNAL(addRequest(modbusWriteRequest *)), &socketThread, SLOT(setWriteRequest(modbusWriteRequest *)));
    readTimer.start(1000);
    mui->RemoveButton->setEnabled(false);
    mui->ReadButton->setEnabled(false);
    QObject::connect(mui->deviceTable, &QTableWidget::cellClicked, [this]() {
        mui->RemoveButton->setEnabled(false);
        mui->ReadButton->setEnabled(false);
    });

    QObject::connect(mui->deviceTable->verticalHeader(), &QHeaderView::sectionClicked, [this]() {
        mui->RemoveButton->setEnabled(true);
        mui->ReadButton->setEnabled(true);
    });

    QObject::connect(mui->Start, &QPushButton::clicked, [this]() {
        socketThread.endLessLoop = true;
        socketThread.start();
        log(QString("Connection requested to %1:%2").arg(socketThread.ip).arg(socketThread.port));
    });

    QObject::connect(mui->Stop, &QPushButton::clicked, [this]() {
        if (socketThread.isRunning()) {
        socketThread.endLessLoop = false; }
        log("Stop requested");
    });

    connect(&socketThread, SIGNAL(tcpStatusChange(int)), this, SLOT(tcpStatusChange(int)));
    connect(&socketThread, SIGNAL(read(QByteArray)), this, SLOT(read(QByteArray)));
    connect(&socketThread, SIGNAL(write(QByteArray)), this, SLOT(write(QByteArray)));
    connect(&socketThread, SIGNAL(logTCP(QString)), this, SLOT(logTCP(QString)));

    tcpIconUnconnectedState.addPixmap(QPixmap(QString::fromUtf8(":/images/images/disconnected.png")), QIcon::Normal, QIcon::Off);
    tcpIconHostLookupState.addPixmap(QPixmap(QString::fromUtf8(":/images/images/connecting.png")), QIcon::Normal, QIcon::Off);
    tcpIconConnectingState.addPixmap(QPixmap(QString::fromUtf8(":/images/images/connecting.png")), QIcon::Normal, QIcon::Off);
    tcpIconConnectedState.addPixmap(QPixmap(QString::fromUtf8(":/images/images/connected.png")), QIcon::Normal, QIcon::Off);
    tcpIconClosingState.addPixmap(QPixmap(QString::fromUtf8(":/images/images/disconnecting.png")), QIcon::Normal, QIcon::Off);
}

ModbusPlugin::~ModbusPlugin()
{
    if (socketThread.isRunning())
        socketThread.abort();
}

QObject *ModbusPlugin::getObject()
{
    return this;
}

QWidget *ModbusPlugin::widgetUi()
{
    return ui;
}

QWidget *ModbusPlugin::getDevWidget(QString)
{
    return nullptr;
}


void ModbusPlugin::setConfigFileName(const QString fileName)
{
    configFileName = fileName;
    configFileName.chop(3);
    configFileName.append(".cfg");
    mui->labelInterfaceName->setToolTip(configFileName);
}

void ModbusPlugin::readDevice(const QString &)
{
}

QString ModbusPlugin::getDeviceCommands(const QString &)
{
    return "";
}



void ModbusPlugin::saveConfig()
{
    QFile file(configFileName);
    QByteArray configdata;
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setGenerateByteOrderMark(true);
        QString str;
        QDateTime now = QDateTime::currentDateTime();
        str += "Configuration file " + now.toString() + "\n";
        str += saveformat("IPadress", socketThread.ip);
        str += saveformat("Port", QString("%1").arg(socketThread.port));
        str += saveformat("Name", mui->editName->text());
        str += saveformat("Write0x10Only", QString("%1").arg(mui->checkBoxWr10->checkState()));
        str += saveformat("Idle", QString("%1").arg(mui->spinBoxIdle->value()));
        sortDevicesId();
        int index = 1;
        foreach (modbusItem *dev, modbusDevices) {
            QString data;
            data.append(dev->RomID + "/");    // RomID
            data.append(dev->slave->text() + "/");    // Slave
            data.append(dev->address->text() + "/");    // Address
            data.append(dev->type->currentText() + "/");    // Type
            data.append(dev->typeDef->currentText() + "/");  // TypeDef
            data.append(dev->readInterval->currentText() + "/");    // Read Interval
            data.append(dev->maxValue->text());    // Max Value
            str += saveformat(QString("Device_%1").arg(index++, 3, 10, QChar('0')), data); }
        out << str;
        file.close(); }
}


void ModbusPlugin::readConfig()
{
    QFile file(configFileName);
    QByteArray configdata;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        QString configData;
        configData.append(in.readAll());
        file.close();
        setipaddress(getvalue("IPadress", configData));
        setport(getvalue("Port", configData));
        bool ok;
        int Wr10 = getvalue("Write0x10Only", configData).toInt(&ok);
        if (ok && Wr10) mui->checkBoxWr10->setCheckState(Qt::Checked);
        int Idle = getvalue("Idle", configData).toInt(&ok);
        if (ok && Idle) mui->spinBoxIdle->setValue(Idle);
        int count = 1;
        QString devID = QString("Device_%1").arg(count, 3, 10, QChar('0'));
        QString data = getvalue(devID, configData);
        QString Name = getvalue("Name", configData);
        if (!Name.isEmpty()) mui->editName->setText(Name);
        emit(updateInterfaceName(this, mui->editName->text()));
        while (!data.isEmpty())
        {
            QStringList parameters = data.split("/");
            if (parameters.count() == 6) {
                modbusItem *newDev = addDevice(parameters.at(0));
                if (newDev) {
                    newDev->RomID = parameters.at(0);
                    newDev->slave->setText(parameters.at(1));
                    newDev->address->setText(parameters.at(2));
                    newDev->type->setCurrentText(parameters.at(3));
                    newDev->typeDef->setCurrentText(parameters.at(4));
                    newDev->readInterval->setCurrentText(parameters.at(5));
                    newDev->maxValue->setText("65535");
                }
            }
            if (parameters.count() == 7) {
                modbusItem *newDev = addDevice(parameters.at(0));
                if (newDev) {
                    newDev->RomID = parameters.at(0);
                    newDev->slave->setText(parameters.at(1));
                    newDev->address->setText(parameters.at(2));
                    newDev->type->setCurrentText(parameters.at(3));
                    newDev->typeDef->setCurrentText(parameters.at(4));
                    newDev->readInterval->setCurrentText(parameters.at(5));
                    newDev->maxValue->setText(parameters.at(6));
                }
            }
            devID = QString("Device_%1").arg(++count, 3, 10, QChar('0'));
            data = getvalue(devID, configData);
        }
        sortDevicesList();
    }
    socketThread.endLessLoop = true;
    socketThread.start();
    readDevices(modbusDevices);
    log(QString("Connection requested to %1:%2").arg(socketThread.ip).arg(socketThread.port));
}




void ModbusPlugin::setLockedState(bool state)
{
    mui->AddButton->setEnabled(!state);
    mui->RemoveButton->setEnabled(!state);
    mui->editIP->setEnabled(!state);
    mui->editPort->setEnabled(!state);
    mui->editName->setEnabled(!state);
    mui->checkBoxWr10->setEnabled(!state);
    mui->spinBoxIdle->setEnabled(!state);
    for (int n=0; n<mui->deviceTable->rowCount(); n++) {
        for (int i=1; i<3; i++) {
        QTableWidgetItem *item = mui->deviceTable->item(n, i);
        if (item) {
        if (state) item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        else item->setFlags(item->flags() | Qt::ItemIsEditable); } }

        QTableWidgetItem *item = mui->deviceTable->item(n, 8);
        if (item) {
        if (state) item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        else item->setFlags(item->flags() | Qt::ItemIsEditable); } }

        foreach (modbusItem *dev, modbusDevices) {
        dev->type->setEnabled(!state);
        dev->typeDef->setEnabled(!state);
        dev->readInterval->setEnabled(!state);
    }
}

QString ModbusPlugin::getDeviceConfig(QString)
{
    return "";
}

void ModbusPlugin::setDeviceConfig(const QString &, const QString &)
{
}


QString ModbusPlugin::getName()
{
    return mui->editName->text();
}


double ModbusPlugin::getMaxValue(const QString RomID)
{
    foreach (modbusItem *dev, modbusDevices) {
        if (dev->RomID == RomID) {
            bool ok;
            int max = dev->maxValue->text().toInt(&ok);
            if (ok) return max;
        } }
    return 65535;
}

bool ModbusPlugin::isManual(const QString)
{
    return false;
}

bool ModbusPlugin::isDimmable(const QString)
{
    return true;
}

bool ModbusPlugin::acceptCommand(const QString)
{
    return true;
}

void ModbusPlugin::setStatus(const QString status)
{   // C0A8011B05BE021MD=45
    QStringList split = status.split("=");
    if (split.count() != 2) return;
    QString RomID = split.first();
    QString command = split.last();
    foreach (modbusItem *dev, modbusDevices) {
        if (dev->RomID == RomID) {
            bool ok;
            modbusWriteRequest *wReq = new modbusWriteRequest;
            wReq->value = command;
            wReq->dev = dev;
            quint16 slave = dev->slave->text().toInt(&ok);
            quint16 address = dev->address->text().toInt(&ok);
            wReq->address = address;
            if ((dev->typeDef->currentIndex() == uint16b) || (dev->typeDef->currentIndex() == int16b))
            {
                quint16 data = command.toDouble(&ok);
                if (mui->checkBoxWr10->isChecked()) {
    // write multiple register
                    wReq->req.append(slave);
                    wReq->req.append(WriteMultipleRegisters);
                    wReq->req.append(address >> 8);
                    wReq->req.append(address & 0x00ff);
                    wReq->req.append(char(00));
                    wReq->req.append(char(01));
                    wReq->req.append(char(02));
                    wReq->req.append(data >> 8);
                    wReq->req.append(data & 0x00ff);
                    quint16 crc = calcCRC16(wReq->req);
                    wReq->req.append(crc >> 8);
                    wReq->req.append(crc & 0x00ff);
                    emit(addRequest(wReq));
                    return; }
                else {   // write single register
                    wReq->req.append(slave);
                    wReq->req.append(WriteSingleRegister);
                    wReq->req.append(address >> 8);
                    wReq->req.append(address & 0x00ff);
                    wReq->req.append(data >> 8);
                    wReq->req.append(data & 0x00ff);
                    quint16 crc = calcCRC16(wReq->req);
                    wReq->req.append(crc >> 8);
                    wReq->req.append(crc & 0x00ff);
                    emit(addRequest(wReq));
                    return; }
            }
            if ((dev->typeDef->currentIndex() == uint32bMSDFirst) || (dev->typeDef->currentIndex() == uint32bMSDLast))
            {
                quint32 data = command.toDouble(&ok);
                // write multiple register
                    wReq->req.append(slave);
                    wReq->req.append(WriteMultipleRegisters);
                    wReq->req.append(address >> 8);
                    wReq->req.append(address & 0x00ff);
                    wReq->req.append(char(00));
                    wReq->req.append(char(02));
                    wReq->req.append(char(04));
                    wReq->req.append((data & 0xff000000) >> 24);
                    wReq->req.append((data & 0x00ff0000) >> 16);
                    wReq->req.append((data & 0x0000ff00) >> 8);
                    wReq->req.append(data & 0x000000ff);
                    quint16 crc = calcCRC16(wReq->req);
                    wReq->req.append(crc >> 8);
                    wReq->req.append(crc & 0x00ff);
                    emit(addRequest(wReq));
                    return; }
        }
    }
}

void ModbusPlugin::setipaddress(const QString &adr)
{
    if (isIpValid(adr))
    {
        mui->editIP->setText(adr);
        socketThread.ip = adr;
    }
}

void ModbusPlugin::setport(const QString &p)
{
    if (!p.isEmpty()) {
        mui->editPort->setText(p);		// affichage de l'adresse IP
        socketThread.port = quint16(p.toInt()); }
    else {
        mui->editPort->setText("1470");		// affichage de l'adresse IP
        socketThread.port = quint16(1470); }
}

void ModbusPlugin::tcpStatusChange(int state)
{
    switch (state)
    //    switch (socketThread.tcpState())
    {
        case QAbstractSocket::UnconnectedState : mui->toolButtonTcpState->setIcon(tcpIconUnconnectedState);
            log(QString("Disconnected"));
            mui->toolButtonTcpState->setToolTip(tr("Not connected"));
        break;
        case QAbstractSocket::HostLookupState : mui->toolButtonTcpState->setIcon(tcpIconConnectingState);
            log(QString("Host lookup"));
            mui->toolButtonTcpState->setToolTip(tr("Host lookup"));
        break;
        case QAbstractSocket::ConnectingState : mui->toolButtonTcpState->setIcon(tcpIconConnectingState);
            log(QString("Connecting"));
            mui->toolButtonTcpState->setToolTip(tr("Connecting"));
        break;
        case QAbstractSocket::ConnectedState : mui->toolButtonTcpState->setIcon(tcpIconConnectedState);
            log(QString("Connected"));
            mui->toolButtonTcpState->setToolTip(tr("Connected"));
        break;
        case QAbstractSocket::ClosingState : mui->toolButtonTcpState->setIcon(tcpIconClosingState);
            log(QString("Disconnecting"));
            mui->toolButtonTcpState->setToolTip(tr("Disconnecting"));
        break;
        default: break;
    }
}

void ModbusPlugin::logTCP(QString str)
{
    log(str);
}


inline quint32 byteArrayToUint32(const QByteArray &bytes)
{
/*    auto count = bytes.size();
    if (count == 0 || count > 4) {
        return 0;
    }
    quint32 number = 0;
    for (int i = 0; i < count; ++i) {
        auto b = static_cast<quint32>(bytes[count - 1 - i]);
        number += static_cast<quint32>(b << (8 * i));
    }
    return number;*/
    bool ok;
    return bytes.toHex().toULong(&ok, 16);
}

inline quint16 byteArrayToUint16(const QByteArray &bytes)
{
/*    auto count = bytes.size();
    if (count == 0 || count > 2) {
        return 0;
    }
    quint16 number = 0;
    for (int i = 0; i < count; ++i) {
        auto b = static_cast<quint8>(bytes.at(i));
        number += static_cast<quint16>(b << (8 * i));
    }
    return number;


01 04 02 07 08 BA C6
01 04 07 D0 00 01 31 47

*/
    bool ok;
    return bytes.toHex().toUInt(&ok, 16);
}

inline qint16 byteArrayToint16(const QByteArray &bytes)
{
/*    auto count = bytes.size();
    if (count == 0 || count > 2) {
        return 0;
    }
    qint16 number = 0;
    for (int i = 0; i < count; ++i) {
        auto b = static_cast<qint16>(bytes[count - 1 - i]);
        number += static_cast<qint16>(b << (8 * i));
    }
    return number;*/
    bool ok;
    return bytes.toHex().toInt(&ok, 16);
}



void ModbusPlugin::read(QByteArray result)
{
    // 2 firts bytes : address
    // following byte : raw modbus answer
    // 043F 01 03 02 00 00"
    //qDebug() << result.toHex().toUpper();
    QList <quint16> values;
    quint16 A = byteArrayToUint16(result.mid(0,2));
    //qDebug() << "A = " + result.mid(0,2).toHex().toUpper() + QString(" %1").arg(A);
    quint16 slave = byteArrayToUint16(result.mid(2,1));
    //qDebug() << result.toHex().toUpper() + QString("  %1 %2").arg(A).arg(slave);
    if (result.length() < 5) return;
    int dataLength = result.at(4);
    for(int n=0; n<dataLength; n+=2) {
        modbusItem *dev = getRomID(slave, A++);
        QString Value;
        if (dev) {
            if (dev->typeDef->currentIndex() == uint16b) {
                Value = QString("%1").arg(byteArrayToUint16(result.mid(n+5,2))); }
            if (dev->typeDef->currentIndex() == int16b) {
                Value = QString("%1").arg(byteArrayToint16(result.mid(n+5,2))); }
            if (dev->typeDef->currentIndex() == uint32bMSDFirst) {
                if ((dataLength - n) < 4) log("Data Length Error " + result.toHex().toUpper());
                else Value = QString("%1").arg(byteArrayToUint32(result.mid(n+5,4))); }
            if (dev->typeDef->currentIndex() == uint32bMSDLast) {
                QByteArray swap;
                swap.append(result.mid(n+5, 2));
                swap.append(result.mid(n+7, 2));
                if ((dataLength - n) < 4) log("Data Length Error " + result.toHex().toUpper());
                else Value = QString("%1").arg(byteArrayToUint32(swap)); }
            dev->value->setText(Value);
            dev->lastRead->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
            emit(newDeviceValue(dev->RomID, Value));
            //qDebug() << result.toHex().toUpper() + QString("  A%1 S%2 = %3").arg(A-1).arg(slave).arg(Value) + " emit : " + dev->RomID + "  " + Value;
        }
    }
}



void ModbusPlugin::write(QByteArray result)
{
    if (result.length() == 6) {
        bool ok;
        quint16 slave = result.mid(0, 1).toHex().toUInt(&ok, 16);
        quint16 function = result.mid(1, 1).toHex().toUInt(&ok, 16);
        quint16 address = result.mid(2, 2).toHex().toUInt(&ok, 16);
        //quint16 size = result.mid(2, 2).toHex().toUInt(&ok, 16);
        if (function == WriteSingleRegister) {
            modbusItem *dev = getRomID(slave, address);
            //qDebug() << QString("Slave : %1  Address : %2 ").arg(slave).arg(address) + result.toHex().toUpper(); // + result.toHex().toUpper(); // 0106001105F0
            if (dev) {
                modbusReadRequest *request = new modbusReadRequest(slave, dev->type->currentIndex(), address, dev->typeDef->currentIndex());
                socketThread.setReadRequest(request);
            }
        }
        if (function == WriteMultipleRegisters) {
            modbusItem *dev = getRomID(slave, address);
            if (dev) {
                modbusReadRequest *request = new modbusReadRequest(slave, dev->type->currentIndex(), address, dev->typeDef->currentIndex());
                socketThread.setReadRequest(request);
            }
        }
        //registerType type = static_cast<registerType>(wReq->dev->type->currentIndex());
        //registerTypeDef typeDef = static_cast<registerTypeDef>(wReq->dev->typeDef->currentIndex());
        //quint16 address = wReq->dev->address->text().toInt(&ok);
        //delete writeReqList.takeFirst(); }
    }
}



modbusItem *ModbusPlugin::getRomID(quint16 slave, quint16 address)
{
    bool ok;
    foreach (modbusItem *dev, modbusDevices) {
        if (dev->slave->text().toInt(&ok) == slave)
            if (dev->address->text().toInt(&ok) == address)
                return dev;
    }
    return nullptr;
}


QString ModbusPlugin::buildRomID(int n)
{
    QString portHex = QString("%1").arg(socketThread.port, 4, 16, QLatin1Char('0')).toUpper();
    QString ipHex = ip2Hex(socketThread.ip);
    QString id = QString("%1").arg(n, 3, 10, QLatin1Char('0')).toUpper();
    QString RomID = ipHex +  portHex + id + "MD";
    return RomID;
}

bool ModbusPlugin::isPortValid(QString str)
{
    bool ok;
    int port = str.toInt(&ok);
    if (!ok | (port < 0) | (port > 65535)) return false;
    return true;
}

bool ModbusPlugin::isIpValid(QString str)
{
    QStringList p = str.split(".");
    if (p.count() != 4) return false;
    bool ok;
    int n = p.at(0).toInt(&ok);
    if (!ok | (n < 0) | (n > 255)) return false;
    n = p.at(1).toInt(&ok);
    if (!ok | (n < 0) | (n > 255)) return false;
    n = p.at(2).toInt(&ok);
    if (!ok | (n < 0) | (n > 255)) return false;
    n = p.at(3).toInt(&ok);
    if (!ok | (n < 0) | (n > 255)) return false;
    return true;
}

QString ModbusPlugin::ip2Hex(const QString &ip)
{
    bool ok;
    int p1 = ip.indexOf(".");		// get first point position
    if (p1 == -1) return "";
    int p2 = ip.indexOf(".", p1 + 1);	// get second point position
    if (p2 == -1) return "";
    int p3 = ip.indexOf(".", p2 + 1);	// get third point position
    if (p3 == -1) return "";
    int l = ip.length();
    QString N1 = ip.mid(0, p1);
    if (N1 == "") return "";
    QString N2 = ip.mid(p1 + 1, p2 - p1 - 1);
    if (N2 == "") return "";
    QString N3 = ip.mid(p2 + 1, p3 - p2 - 1);
    if (N3 == "") return "";
    QString N4 = ip.mid(p3 + 1, l - p3 - 1);
    if (N4 == "") return "";
    int n1 = N1.toInt(&ok);
    if (!ok) return "";
    int n2 = N2.toInt(&ok);
    if (!ok) return "";
    int n3 = N3.toInt(&ok);
    if (!ok) return "";
    int n4 = N4.toInt(&ok);
    if (!ok) return "";
    return QString("%1%2%3%4").arg(n1, 2, 16, QLatin1Char('0')).arg(n2, 2, 16, QLatin1Char('0')).arg(n3, 2, 16, QLatin1Char('0')).arg(n4, 2, 16, QLatin1Char('0')).toUpper();
}

void ModbusPlugin::log(const QString str)
{
    if (mui->checkBoxLog->isChecked())
    {
        mui->logTxt->append(QDateTime::currentDateTime().toString("HH:mm:ss ") + str);
        QString currenttext = mui->logTxt->toPlainText();
        if (currenttext.length() > 10000) mui->logTxt->setText(currenttext.mid(9000));
        mui->logTxt->moveCursor(QTextCursor::End);
    }
}

void ModbusPlugin::showLog()
{
}

void ModbusPlugin::on_AddButton_clicked()
{
    if (!isIpValid(socketThread.ip)) {
        QMessageBox msgBox;
        msgBox.setText("IP address is not valid");
        msgBox.exec();
        return;
    }
    if (!isPortValid(mui->editPort->text())) {
        QMessageBox msgBox;
        msgBox.setText("Port is not valid");
        msgBox.exec();
        return;
    }
    int count = 1;
    bool found = true;
    while (found) {
        found = false;
        QString id = buildRomID(count);
        foreach (modbusItem *dev, modbusDevices) {
            if (dev->RomID == id) found = true;  }
        if (found) count++;
    }
    addDevice(buildRomID(count));
    //qDebug() << QString ("Count %1").arg(count);
    sortDevicesList();
}

modbusItem *ModbusPlugin::addDevice(QString id)
{
// Get table size
    bool ok;
    int count = id.right(5).left(3).toInt(&ok) - 1; // a faire ici, decompte pas juste
    if (count > mui->deviceTable->rowCount()) count = mui->deviceTable->rowCount();
// add one line
    mui->deviceTable->insertRow(count);
// Generate RomID
    if (id.isEmpty()) id = buildRomID(count + 1);
// add device in the QHash store
    modbusItem *newDev = new modbusItem(id);
    if (!newDev) return nullptr;
    modbusDevices.insert(count, newDev);
// add RomID widget in the table
    QTableWidgetItem *item = new QTableWidgetItem(id);
    item->setFlags(item->flags() ^ Qt::ItemIsEditable);  // set RomID widget not editable
    mui->deviceTable->setItem(count, 0, item);
// add Address input item
    mui->deviceTable->setItem(count, 1, newDev->address);
    //newDev->address->setText(QString("%1").arg(count +1));
// add Slave input item
    mui->deviceTable->setItem(count, 2, newDev->slave);
// add Type combo input item
    mui->deviceTable->setCellWidget(count, 3, newDev->type);
// add Typedef combo input item
    mui->deviceTable->setCellWidget(count, 4, newDev->typeDef);
// add Read Interval combo input item
    mui->deviceTable->setCellWidget(count, 5, newDev->readInterval);
// set Value item
    mui->deviceTable->setItem(count, 6, newDev->value);
// set Last Read item
    mui->deviceTable->setItem(count, 7, newDev->lastRead);
    mui->deviceTable->setItem(count, 8, newDev->maxValue);
    emit(newDevice(this, id));
    return newDev;
}


void ModbusPlugin::checkImport()
{
    accepted = -2;
    if (QFile(configFileName).exists()) return;
    QString cfgfilename = "maison.cfg";
    QFile file(cfgfilename);
    QTextStream in(&file);
    QString configdata;
// Read original config file to get all device config and extract devices
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!in.atEnd()) {
            QString line = in.readLine();
            configdata.append(line + "\n"); }
        file.close(); }
    QString TAG_Begin = One_Wire_Device;
    QString TAG_End = EndMark;
    QString ReadRomID;
    SearchLoopBegin
        ReadRomID = getvalue("RomID", strsearch);
        if (ReadRomID.right(2) == "MD")
        {
            QString ip = getvalue("Master", strsearch);
            if (ip == socketThread.ip) {
            if (accepted < 0) {
                if (QMessageBox::question(this, "Importer", "Voulez vous importer l'ancienne configuration ?",
                                          QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes) accepted = 1; else accepted = 0; }
            if (accepted == 1) {
                bool ok;
                QString slave = getvalue("SlaveID", strsearch);
                QString address = getvalue("AddressID", strsearch);
                int type = getvalue("RequestType", strsearch).toInt(&ok);
                int typeDef = getvalue("Unsigned", strsearch).toInt(&ok);
                int interval = getvalue("SaveMode", strsearch).toInt(&ok);
                modbusItem *newDev = addDevice(ReadRomID);
                if (newDev) {
                    newDev->RomID = ReadRomID;
                    if (slave.isEmpty()) newDev->slave->setText("1"); else newDev->slave->setText(slave);
                    newDev->address->setText(address);
                    newDev->type->setCurrentIndex(type);
                    if (typeDef == 0) newDev->typeDef->setCurrentIndex(int16b);
                    if (interval == 0) newDev->readInterval->setCurrentIndex(1);
                    if (interval == 1) newDev->readInterval->setCurrentIndex(2);
                    if (interval == 2) newDev->readInterval->setCurrentIndex(3);
                    if (interval == 3) newDev->readInterval->setCurrentIndex(4);
                    if (interval == 6) newDev->readInterval->setCurrentIndex(5);
                    if (interval == 7) newDev->readInterval->setCurrentIndex(6);
// enum readInterval { readConstant, read1mn, read2mn, read5mn, read10mn, read30mn, read1hour };
//enum mode { M1, M2, M5, M10, M15, M20, M30, H1, H2, H3, H4, H6, H12, D1, D2, D5, D10, W1, W2, MT, AUTO, lastInterval };
                }
            }
        }
    }
    SearchLoopEnd
    if (accepted == -2) accepted = -1;
}


void ModbusPlugin::updateIpRomID()
{
    int count = mui->deviceTable->rowCount();
    for (int n=0; n<count; n++) {  // tableWidget
        QString RomID = buildRomID(n+1);
        mui->deviceTable->item(n, 0)->setText(RomID);
        foreach (modbusItem *dev, modbusDevices) {  // modbusDevice
            if (dev->id == n+1) dev->RomID = RomID; }
    }
}



void ModbusPlugin::on_RemoveButton_clicked()
{
    QModelIndexList indexList = mui->deviceTable->selectionModel()->selectedIndexes();
    int row = 0;
    foreach (QModelIndex index, indexList) { row = index.row(); }
    foreach (modbusItem *dev, modbusDevices) {
        if (dev->RomID == mui->deviceTable->item(row, 0)->text()) {
            //qDebug() << QString("Remove ") + dev->RomID;
            modbusDevices.removeAll(dev);
            mui->deviceTable->removeRow(row);
            mui->RemoveButton->setEnabled(false);
            mui->ReadButton->setEnabled(false);
            return;
        }
    }
}


void ModbusPlugin::on_ReadButton_clicked()
{
    QModelIndexList indexList = mui->deviceTable->selectionModel()->selectedIndexes();
    QList<int>reqList;
    foreach (QModelIndex index, indexList) {    // get each selected line
        if (reqList.isEmpty()) reqList.append(index.row());
        else {
            if (!reqList.contains(index.row())) reqList.append(index.row()); }
    }
    bool ok;
    foreach (int row, reqList) {
        foreach (modbusItem *dev, modbusDevices) {
            if (dev->RomID == mui->deviceTable->item(row, 0)->text()) {
            quint16 slave = dev->slave->text().toInt(&ok);
            registerType type = static_cast<registerType>(dev->type->currentIndex());
            registerTypeDef typeDef = static_cast<registerTypeDef>(dev->typeDef->currentIndex());
            quint16 address = dev->address->text().toInt(&ok);
            modbusReadRequest *request = new modbusReadRequest(slave, type, address, typeToSize(typeDef));
            socketThread.setReadRequest(request); } } }
}



int ModbusPlugin::typeToSize(int type)
{
    switch (type) {
    case uint16b :
    case int16b : return 1;
    case uint32bMSDFirst :
    case uint32bMSDLast : return 2;
    }
    return 1;
}


void ModbusPlugin::sortDevicesList()
{
    int n;
    int i;
    for (n=0; n < modbusDevices.count(); n++)
    {
        for (i=n+1; i < modbusDevices.count(); i++)
        {
            bool ok;
            int32_t valorN = modbusDevices.at(n)->slave->text().toInt(&ok) << 16;
            int32_t valorI = modbusDevices.at(i)->slave->text().toInt(&ok) << 16;
            valorN += modbusDevices.at(n)->address->text().toInt(&ok);
            valorI += modbusDevices.at(i)->address->text().toInt(&ok);
            if (valorN > valorI) {
                modbusDevices.move(i, n);
                n=0;}
        }
    }
    foreach (modbusItem *item, modbusDevices) { item->address->setBackground(QColor(Qt::white)); }
    for (n=0; n < modbusDevices.count(); n++)
    {
        for (i=n+1; i < modbusDevices.count(); i++)
        {
            bool ok;
            int32_t valorN = modbusDevices.at(n)->slave->text().toInt(&ok) << 16;
            int32_t valorI = modbusDevices.at(i)->slave->text().toInt(&ok) << 16;
            valorN += modbusDevices.at(n)->address->text().toInt(&ok);
            valorI += modbusDevices.at(i)->address->text().toInt(&ok);
            if (valorN == valorI) {
                modbusDevices.at(i)->address->setBackground(QColor(Qt::gray));
                modbusDevices.at(i-1)->address->setBackground(QColor(Qt::gray));
                }
        }
    }
}


void ModbusPlugin::sortDevicesId()
{
    int n;
    int i;
    for (n=0; n < modbusDevices.count(); n++)
    {
        for (i=n+1; i < modbusDevices.count(); i++)
        {
            int16_t valorN = modbusDevices.at(n)->id;
            int16_t valorI = modbusDevices.at(i)->id;
            if (valorN > valorI) {
                modbusDevices.move(i, n);
                n=0;}
        }
    }
}



void ModbusPlugin::setReadRequest(modbusItem *dev, int size)
{
    bool ok;
    quint16 slave = dev->slave->text().toInt(&ok);
    registerType type = static_cast<registerType>(dev->type->currentIndex());
    quint16 address = dev->address->text().toInt(&ok);
    modbusReadRequest *request = new modbusReadRequest(slave, type, address, size);
    socketThread.setReadRequest(request);
}



void ModbusPlugin::readAllNow()
{
    if (lastMinute < 0) {
        on_ReadAllButton_clicked();
        lastMinute = QDateTime::currentDateTime().time().minute();
        //qDebug() << "First Read All";
    }
    QList<modbusItem*> devList;
    int m = QDateTime::currentDateTime().time().minute();
    if (m != lastMinute)
    {
        foreach (modbusItem *dev, modbusDevices) {
            switch (dev->readInterval->currentIndex())
            {
                case readConstant :
                case read1mn : devList.append(dev); break;
                case read2mn : if (m % 2 == 0) devList.append(dev); break;
                case read5mn : if (m % 5 == 0) devList.append(dev); break;
                case read10mn : if (m % 10 == 0) devList.append(dev); break;
                case read30mn : if (m % 30 == 0) devList.append(dev); break;
                case read1hour : if (m == 0) devList.append(dev); break;
            }
        }
        lastMinute = m;
    }
    else {
        if (socketThread.idle) {
            idle ++;
            if (idle >= mui->spinBoxIdle->value())
            foreach (modbusItem *dev, modbusDevices) {
                idle = 0;
                if (dev->readInterval->currentIndex() == readConstant) devList.append(dev);
                } } }
    readDevices(devList);
}



void ModbusPlugin::readDevices(QList<modbusItem*>& devList)
{
    if (devList.count() == 0) return;
    if (devList.count() == 1) {
        setReadRequest(devList.first(), 1);
        return; }
    int s = 0;
    int index = 1;
    for (int n=1; n<devList.count(); n++)
    {
        // check is device have consecutive address
        if (*devList.at(n-1) == *devList.at(n)) {
            index++;
            registerTypeDef typeDefDevice = static_cast<registerTypeDef>(devList.at(n-1)->typeDef->currentIndex());
            s += typeToSize(typeDefDevice);
        }
        // in the other case compute the read request
        else {
            registerTypeDef typeDefLastDevice = static_cast<registerTypeDef>(devList.at(n)->typeDef->currentIndex());
            s += typeToSize(typeDefLastDevice);
            setReadRequest(devList.at(n - index), s);
            index = 1;
            s = 0; }
    }
    registerTypeDef typeDefLastDevice = static_cast<registerTypeDef>(devList.last()->typeDef->currentIndex());
    if (index == 1)
    {
        registerTypeDef typeDefLastDevice = static_cast<registerTypeDef>(devList.last()->typeDef->currentIndex());
        int s = typeToSize(typeDefLastDevice);
        setReadRequest(devList.last(), s);
    }
    else
    {
        s += typeToSize(typeDefLastDevice);
        setReadRequest(devList.at(devList.count() - index), s);
    }
}



void ModbusPlugin::on_ReadAllButton_clicked()
{
    readDevices(modbusDevices);
}



void ModbusPlugin::checkCombo()
{
    sortDevicesList();
}


void ModbusPlugin::displayDevice(int row, int column)
{
    if (column == 0)
    {
        QString RomID = mui->deviceTable->item(row, column)->text();
        emit(deviceSelected(RomID));
    }
}


void ModbusPlugin::editIP_editingFinished()
{
    if (!isIpValid(mui->editIP->text())) {
        QMessageBox msgBox;
        msgBox.setText("IP address is not valid");
        msgBox.exec();
        return;
    }
    socketThread.ip = mui->editIP->text();
    updateIpRomID();
    if (accepted == -1) checkImport();
}


void ModbusPlugin::on_editPort_editingFinished()
{
    if (!isPortValid(mui->editPort->text())) {
        QMessageBox msgBox;
        msgBox.setText("Port is not valid");
        msgBox.exec();
        return;
    }
    socketThread.port = quint16(mui->editPort->text().toInt());
    updateIpRomID();
}



void ModbusPlugin::on_editName_editingFinished()
{
    emit(updateInterfaceName(this, mui->editName->text()));
}



