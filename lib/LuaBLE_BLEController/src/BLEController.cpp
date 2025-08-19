#ifdef __cplusplus
extern "C" {
#endif

#include "driver/uart.h"
#include "esp_task_wdt.h"
#include "esp_mac.h"

#ifdef __cplusplus
}
#endif
#include "BLEController.h"
#include <Arduino.h>

BLEController bleController;

class BLEController::ServerCallbacks : public BLEServerCallbacks
{
    BLEController &controller;
public:
    ServerCallbacks(BLEController &ctrl) : controller(ctrl) {}

     void onConnect(BLEServer *pServer, NimBLEConnInfo& connInfo)
    {
        controller.deviceConnected = true;
        if (controller.onConnectCallback)
        {
            controller.onConnectCallback();
        }
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(BLEServer *pServer, NimBLEConnInfo& connInfo, int reason)
    {
        controller.deviceConnected = false;
        controller.startAdvertising();
        if (controller.onDisconnectCallback)
        {
            controller.onDisconnectCallback();
        }
    }


    void onMTUChange( uint16_t newMtu, NimBLEConnInfo& connInfo)
    {
        controller.handleMtuChange(newMtu);
    }
};

class BLEController::CharCallbacks : public BLECharacteristicCallbacks
{
    BLEController &controller;

public:
    CharCallbacks(BLEController &ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pCharacteristic,NimBLEConnInfo& connInfo)
    {
        std::string rxValue = pCharacteristic->getValue();
        controller.handleReceivedMessage(rxValue);
        rxValue.clear();
    }
};

class BLEController::OtaCallbacks : public BLECharacteristicCallbacks
{
    BLEController &controller;

public:
    OtaCallbacks(BLEController &ctrl) : controller(ctrl) {}
     void onWrite(BLECharacteristic *pCharacteristic,NimBLEConnInfo& connInfo)
    {
        std::string pData = pCharacteristic->getValue();
        int len = pData.length();
        controller.receiveOTA(pData);
    }
};

void BLEController::receiveOTA(const std::string &pData)
{
    if (pData.length() > 0)
    {
        if (pData.find('\4') != std::string::npos)
        {
            blankota = blankota + pData;
            blankota.erase(blankota.length() - 1);
            msgota.push(blankota);
            blankota = "";
        }
        else
        {
            blankota = blankota + pData;
        }
    }
}

void BLEController::handleMtuChange(uint16_t newMtu)
{
    mtu = newMtu;
}

BLEController::BLEController() : deviceConnected(false), mtu(250),
                                 serviceUUID(BLE_SERVICE_UUID),
                                 txUUID(BLE_UUID_TX),
                                 rxUUID(BLE_UUID_RX),
                                 batUUID(BATTERY_SERVICE_UUID),
                                 productUUID(BLE_PRODUCT_UUID),
                                 blePrefix(BLE_NAME_PREFIX),
                                 otaServiceUUID(OTA_SERVICE_UUID),
                                 otaTxUUID(OTA_UUID_TX),
                                 otaRxUUID(OTA_UUID_RX),
                                 currentBatteryLevel(0), otaHandler(0),
                                 TextMode(true)
{
    memset(currentSWVersion, 0, sizeof(currentSWVersion));
}

void BLEController::setDeviceName(String name)
{

    deviceName = name;
    // STORAGE.store("deviceName", name);
}

String BLEController::getDeviceName()
{
    char bleNameAuto[BLE_NAME_LENGTH];
    char bleMAcString[18];
    uint8_t bleMac[6];
    esp_read_mac(bleMac, ESP_MAC_BT);
    snprintf(bleNameAuto, sizeof(bleNameAuto), "%s-%02X%02X", blePrefix.c_str(),bleMac[4], bleMac[5]);
    // convert bleMac to string
    sniprintf(bleMAcString, sizeof(bleMAcString), "%02X:%02X:%02X:%02X:%02X:%02X", bleMac[0], bleMac[1], bleMac[2], bleMac[3], bleMac[4], bleMac[5]);
    macAddress = String(bleMAcString);

    String _devicename = bleNameAuto;
    return _devicename;
}

void BLEController::begin(const String Name)
{
    this->deviceName =Name.isEmpty() ? getDeviceName() : Name;
    BLEDevice::init(this->deviceName.c_str());
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    BLEDevice::setMTU(512);

    pServer = BLEDevice::createServer();
    serverCallbacks = new ServerCallbacks(*this);
    pServer->setCallbacks(serverCallbacks);
    // pServer->setCallbacks(new ServerCallbacks(*this));

    BLEService *pService = pServer->createService(serviceUUID.c_str());
    BLEService *pBatService = pServer->createService(batUUID.c_str());

    pTxCharacteristic = pService->createCharacteristic(txUUID.c_str(), NIMBLE_PROPERTY::NOTIFY);
    pRxCharacteristic = pService->createCharacteristic(rxUUID.c_str(), NIMBLE_PROPERTY::WRITE);
    pBatCharacteristic = pBatService->createCharacteristic(
        "2A19", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    pRxCharacteristic->setCallbacks(new CharCallbacks(*this));

    bool success = pService->start();

    bool success2 = pBatService->start();

    initOTA();
    startAdvertising();
    _initialized = true;
}

void BLEController::initOTA()
{
    esp_ota_mark_app_valid_cancel_rollback();
    BLEService *otaService = pServer->createService(OTA_SERVICE_UUID);
    otaTX = otaService->createCharacteristic(
        OTA_UUID_TX, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
    otaRX = otaService->createCharacteristic(
        OTA_UUID_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ |
                         NIMBLE_PROPERTY::WRITE_NR);
    otaRX->setCallbacks(new OtaCallbacks(*this));
    otaTX->setCallbacks(new OtaCallbacks(*this));
    otaService->start();
    LittleFSFile::initFS(); 
}

void BLEController::processOTAData()
{
    if (msgota.empty())
    {
        return;
    }

    std::string data = msgota.front();
    msgota.pop();

    uint8_t msgId;
    if (data[0] == 0x01)
    {
        msgId = data[1];
        String jsonData = data.substr(1).c_str();
        handleOTA(jsonData, msgId);
    }
    else if (data[0] == 0x02)
    {
        msgId = data[1];
        std::string fileData = data.substr(1);
        LittleFSFile::ErrorCode errorCode = LittleFSFile::writeFile(fileData);
        updateFileTransferProgress(fileData.length());
        sendResponseBinaryOTA((byte)errorCode, msgId);
    }
}

void BLEController::sendResponseBinaryOTA(byte data, uint8_t msgId)
{
    // String response = String('\02') + String(char(msgId)) + String(char(data));
    String response = '\02' + String(char(data));
    sendmsgOTA(response.c_str());
}

void BLEController::sendResponseJsonOTA(String data, String msgtype, uint8_t msgId)
{
    JsonDocument Jsonresponse;
    Jsonresponse["msgtype"] = msgtype;
    Jsonresponse["response"] = data;

    String response;
    serializeJson(Jsonresponse, response);

    // String finalResponse = '\01' + byte(msgId) + response;
    String finalResponse = '\01' + response;
    sendmsgOTA(finalResponse.c_str());
}

void BLEController::sendmsgOTA(String str)
{
    str = str + '\04';
    int mtu = BLEDevice::getMTU();
    int cunkmtu = mtu - 3;

    for (int i = 0; i < str.length(); i += cunkmtu)
    {
        String chunk = str.substring(i, i + cunkmtu);
        // Serial.println("--" + chunk);
        otaTX->setValue((uint8_t *)chunk.c_str(), chunk.length());
        otaTX->notify();
        delay(10);
    }
}

// perform the actual update from a given stream
bool BLEController::performUpdate(Stream &updateSource, size_t updateSize)
{
    if (Update.begin(updateSize))
    {
        size_t written = Update.writeStream(updateSource);
        if (written == updateSize)
        {
            Serial.println("Written : " + String(written) + " successfully");
        }
        else
        {
            Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
            // return false;
        }
        if (Update.end())
        {
            Serial.println("OTA done!");
            if (Update.isFinished())
            {
                Serial.println("Update successfully completed. Rebooting.");
                return true;
            }
            else
            {
                Serial.println("Update not finished? Something went wrong!");
                return false;
            }
        }
        else
        {
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            return false;
        }
    }
    else
    {
        Serial.println("Not enough space to begin OTA");
        return false;
    }
}

// check given FS for valid update.bin and perform update if available
bool BLEController::updateFromFS(fs::FS &fs, const char *path)
{
    // add / to the start of the path
    bool success = false;
    File updateBin = fs.open(path);
    if (updateBin)
    {
        // if(updateBin.isDirectory()){
        //    Serial.printf("Error, %s is not a file\n", path);
        //    updateBin.close();
        //    return;
        // }

        size_t updateSize = updateBin.size();

        Serial.printf("Update name: %s size: %d\n", path, updateSize);
        if (updateSize > 0)
        {
            Serial.println("Try to start update");
            success = performUpdate(updateBin, updateSize);
        }
        else
        {
            Serial.println("Error, file is empty");
        }

        updateBin.close();

        // whe finished remove the binary from sd card to indicate end of the process
        fs.remove(path);
    }
    else
    {
        Serial.println("Could not load update.bin from sd root");

        return false;
    }
    return success;
}

void BLEController::startAdvertising()
{
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData adData;
    adData.setName(this->deviceName.c_str());
    setManufacturerData(adData);

    adData.setCompleteServices(NimBLEUUID(productUUID.c_str()));
    pAdvertising->setAdvertisementData(adData);
    // pAdvertising->setScanResponse(true);
    // pAdvertising->setMinPreferred(0x06);
    // pAdvertising->setMinPreferred(0x12);
    // pAdvertising->setPreferredParams(0x0006, 0x000C);
    pAdvertising->enableScanResponse(true);
    // pAdvertising->setScanResponseData(adData);
    bool success = BLEDevice::startAdvertising();
}

void BLEController::setManufacturerData(BLEAdvertisementData &adData)
{
    std::vector<uint8_t> manData;
    manData.push_back(static_cast<uint8_t>(currentBatteryLevel));
    manData.push_back(currentSWVersion[0]);
    manData.push_back(currentSWVersion[1]);
    manData.push_back(currentSWVersion[2]);
    manData.push_back(255);
    manData.push_back(currentHWVersion[0]);
    manData.push_back(currentHWVersion[1]);
    manData.push_back(currentHWVersion[2]);

    adData.setManufacturerData(manData);
}

void BLEController::handleReceivedMessage(const std::string &message)
{

    if (TextMode)
    {
        if (message.find('\04') != std::string::npos) // run code
        {                                             // Control-D
            // executeTextBuffer();
            executeTextBufferFlag = true;
        }
        else if (message.find('\01') != std::string::npos) // abort code execution
        {
            // call callbackabort

            if (TextAbortCallback)
            {
                TextAbortCallback(message.c_str());
            }
        }
        else if (message.find('\02') != std::string::npos) // add data to lua queue
        {
            // call callbackabortTextAbortCallback

            if (TextQueueCallback)
            {
                TextQueueCallback(message.c_str());
            }
        }
        else if (message.find('\03') != std::string::npos) // to clear text buffer
        {
            TextBuffer = "";
        }
        else
        {
            TextBuffer += message;
        }
        //  receivedData.clear();
    }
    else
    {
        receivedData += message;

        if (receivedData.find('\n') != std::string::npos)
        {
            // INFO_PRINTLN("Received message: %s", receivedData.c_str());
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, receivedData);

            if (error)
            {
                // ERROR_PRINTLN("JSON parsing failed: %s", error.c_str());
            }

            if (!error)
            {

                const char *msgType = doc["msgtyp"];
                if (msgType)
                {
                    Message msg(msgType, doc);
                    auto highPriorityCallback = highPriorityCallbacks.find(msgType);
                    if (highPriorityCallback != highPriorityCallbacks.end())
                    {
                        processHighPriorityMessage(msg);
                    }
                    else
                    {
                        messageQueue.push(msg);
                    }
                }
            }

            receivedData.clear();
        }
    }
}

void BLEController::clearMessageQueue()
{
    while (!messageQueue.empty() && messageQueue.size() > 0)
    {
        if (messageQueue.front().type != nullptr)
        {
            // INFO_PRINTLN("DROPPING MESSAGE: %s", messageQueue.front().type.c_str());
            messageQueue.pop();
        }
        else
        {
            // INFO_PRINTLN("DROPPING NULL MESSAGE");
        }
    }
}

void BLEController::setIsBlockingOperationRunning(bool isRunning)
{
    this->isBlockingOperationRunning = isRunning;
}

bool BLEController::getIsBlockingOperationRunning()
{
    return this->isBlockingOperationRunning;
}

void BLEController::processHighPriorityMessage(Message &msg)
{
    auto callback = highPriorityCallbacks.find(msg.type);
    if (callback != highPriorityCallbacks.end())
    {
        callback->second(msg.doc);
    }
}

void BLEController::processJsonMessages()
{

    while (messageQueue.empty() == false && this->isBlockingOperationRunning == false)
    {
        // INFO_PRINTLN("BLEController::processMessages()");
        Message &msg = messageQueue.front();
        auto callback = messageCallbacks.find(msg.type);
        if (callback != messageCallbacks.end())
        {
            this->setIsBlockingOperationRunning(true);
            // //INFO_PRINTLN("BLEController::processMessages() callback: %s", msg.type.c_str());
            // create copy of message
            // JsonDocument doc = msg.doc;

            callback->second(msg.doc);
            // //INFO_PRINTLN("BLEController::processMessages() callback done: %s", msg.type.c_str());
            if (this->isBlockingOperationRunning == true && messageQueue.empty() == false)
                messageQueue.pop(); // pop processed message

            this->setIsBlockingOperationRunning(false);
        }
        else if (messageQueue.empty() == false) // pop unwanted messages
        {
            messageQueue.pop();
        }

        // INFO_PRINTLN("BLEController::processMessages() done");
    }
}

void BLEController::processTextMessages()
{

    if (TextMode && executeTextBufferFlag)
    {
        executeTextBufferFlag = false;

        executeTextBuffer();
    }
}

void BLEController::sendMessage(const JsonDocument &message)
{
    String jsonString;
    serializeJson(message, jsonString);
    jsonString += '\n'; // Add newline as message delimiter

    int sendMtu = mtu - 3; // 3 bytes overhead
    // INFO_PRINTLN("Sending BLE message: %s", jsonString.c_str());
    for (size_t i = 0; i < jsonString.length(); i += sendMtu)
    {
        String chunk = jsonString.substring(i, std::min(i + sendMtu, jsonString.length()));
        pTxCharacteristic->setValue((uint8_t *)chunk.c_str(), chunk.length());
        pTxCharacteristic->notify();
        // delay(10); // Give the client time to process
        delayMicroseconds(100); // Give the client time to process
    }
}

void BLEController::sendMessage(String jsonString)
{

    jsonString += '\n'; // Add newline as message delimiter

    int sendMtu = mtu - 3; // 3 bytes overhead
    // INFO_PRINTLN("Sending BLE message: %s", jsonString.c_str());
    for (size_t i = 0; i < jsonString.length(); i += sendMtu)
    {
        String chunk = jsonString.substring(i, std::min(i + sendMtu, jsonString.length()));
        pTxCharacteristic->setValue((uint8_t *)chunk.c_str(), chunk.length());
        pTxCharacteristic->notify();
        //    delay(1); // Give the client time to process
        delayMicroseconds(100); // Give the client time to process
    }
}

void BLEController::registerMessageCallback(const String &msgType, std::function<void(JsonDocument &)> callback)
{
    messageCallbacks[msgType] = callback;
    // INFO_PRINTLN("Registered message callback: %s", msgType.c_str());
}

void BLEController::registerHighPriorityCallback(const String &msgType, std::function<void(JsonDocument &)> callback)
{

    highPriorityCallbacks[msgType] = callback;
    // INFO_PRINTLN("Registered high priority message callback: %s", msgType.c_str());
}

void BLEController::updateBatteryAndVersion(int batteryLevel, const uint8_t *version, size_t SWversionLength, uint8_t *HWVersion, size_t HWVersionLength)
{
    currentBatteryLevel = batteryLevel;
    if (SWversionLength > sizeof(currentSWVersion))
    {
        SWversionLength = sizeof(currentSWVersion);
    }
    memcpy(currentSWVersion, version, SWversionLength);

    if (HWVersionLength > sizeof(currentHWVersion))
    {
        HWVersionLength = sizeof(currentHWVersion);
    }
    memcpy(currentHWVersion, HWVersion, HWVersionLength);

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData adData;
    adData.setName(this->deviceName.c_str());
    setManufacturerData(adData);
    adData.setCompleteServices(NimBLEUUID(productUUID.c_str()));
    pAdvertising->setAdvertisementData(adData);

    if (pServer->getAdvertising()->isAdvertising())
    {
        pServer->getAdvertising()->stop();
        pServer->getAdvertising()->start();
    }

    pBatCharacteristic->setValue((uint8_t *)&batteryLevel, 1);
    pBatCharacteristic->notify();
    // INFO_PRINTLN("Updated battery level: %d", batteryLevel);
    // INFO_PRINTLN("Updated version: %d.%d.%d", currentSWVersion[0], currentSWVersion[1], currentSWVersion[2]);
    // INFO_PRINTLN("Updated HW version: %d.%d.%d", currentHWVersion[0], currentHWVersion[1], currentHWVersion[2]);
}

void BLEController::updateBattery(int batteryLevel)
{
    currentBatteryLevel = batteryLevel;
    pBatCharacteristic->setValue((uint8_t *)&batteryLevel, 1);
    pBatCharacteristic->notify();
}

BLEController::FileTransferStatus BLEController::getFileTransferStatus()
{
    return fileStatus;
}

void BLEController::updateFileTransferProgress(size_t bytesReceived)
{
    fileStatus.bytesTransferred += bytesReceived;
    fileStatus.progressPercentage = (fileStatus.bytesTransferred * 100) / fileStatus.totalSize;
    // You might want to send a progress update to the client here
}

void BLEController::resetFileTransferStatus()
{
    fileStatus = {};
}
void BLEController::handleOTA(String data, uint8_t msgId)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);
    String response;

    if (error)
    {
        Serial.println("Failed to parse JSON");
        sendResponseJsonOTA("JSON_ERROR", "jsonerror", msgId);
        return;
    }

    const char *msgtype = doc["msgtype"];
    if (strcmp(msgtype, "fileopen") == 0)
    {
        const char *filename = doc["filename"];
        size_t size = doc["filesize"];
        fileStatus.currentFileName = doc["filename"].as<String>();
        fileStatus.fileIndex = fileStatus.fileIndex;
        fileStatus.totalSize = doc["filesize"].as<size_t>();
        LittleFSFile::ErrorCode errorCode = LittleFSFile::createFile(filename, size);
        response = LittleFSFile::errorCodeToString(errorCode);
        sendResponseJsonOTA(response, msgtype, msgId);
    }
    else if (strcmp(msgtype, "fileclose") == 0)
    {
        LittleFSFile::ErrorCode errorCode = LittleFSFile::closeFile();
        resetFileTransferStatus();
        fileStatus.fileIndex++;
        response = LittleFSFile::errorCodeToString(errorCode);
        sendResponseJsonOTA(response, msgtype, msgId);
    }
    else if (strcmp(msgtype, "filedelete") == 0)
    {
        const char *filename = doc["filename"];
        LittleFSFile::ErrorCode errorCode = LittleFSFile::deleteFile(filename);
        response = LittleFSFile::errorCodeToString(errorCode);
        sendResponseJsonOTA(response, msgtype, msgId);
    }
    else if (strcmp(msgtype, "filerename") == 0)
    {
        const char *oldFilename = doc["oldfilename"];
        const char *newFilename = doc["newfilename"];
        LittleFSFile::ErrorCode errorCode = LittleFSFile::renameFile(oldFilename, newFilename);
        response = LittleFSFile::errorCodeToString(errorCode);
        sendResponseJsonOTA(response, msgtype, msgId);
    }
    else if (strcmp(msgtype, "filelist") == 0)
    {
        // Serial.println("filelist");
        JsonDocument responseDoc; // Increase the size as needed
        JsonArray fileListArray = responseDoc.createNestedArray("fileList");
        LittleFSFile::ErrorCode errorCode = LittleFSFile::listFiles(fileListArray);

        if (errorCode == LittleFSFile::FILE_OK)
        {
            responseDoc["msgtype"] = "filelist";
            responseDoc["response"] = "OK";

            String response;
            serializeJson(responseDoc, response);
            sendResponseJsonOTA(response, msgtype, msgId);
        }
        else
        {
            String errorStr = LittleFSFile::errorCodeToString(errorCode);
            sendResponseJsonOTA(errorStr, msgtype, msgId);
        }
    }

    else if (strcmp(msgtype, "formatLittleFS") == 0)
    {
        esp_task_wdt_delete(NULL);
        LittleFSFile::ErrorCode errorCode = LittleFSFile::formatFS();
        response = LittleFSFile::errorCodeToString(errorCode);
        sendResponseJsonOTA(response, msgtype, msgId);
        esp_task_wdt_add(NULL);
    }
    else if (strcmp(msgtype, "initLittleFS") == 0)
    {
        LittleFSFile::ErrorCode errorCode = LittleFSFile::initFS();
        response = LittleFSFile::errorCodeToString(errorCode);
        sendResponseJsonOTA(response, msgtype, msgId);
    }
    else if (strcmp(msgtype, "otaupdate") == 0)
    {
        const char *filename = doc["filename"];
        bool success = updateFromFS(LittleFS, filename);
        if (success)
        {
            sendResponseJsonOTA("OTA update successful", msgtype, msgId);
            delay(1000);
            ESP.restart();
        }
        else
        {
            sendResponseJsonOTA("OTA update failed", msgtype, msgId);
        }
    }
    else if (strcmp(msgtype, "spiffssize") == 0)
    {
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        size_t freeBytes = totalBytes - usedBytes;
        JsonDocument responseDoc;
        responseDoc["totalBytes"] = totalBytes;
        responseDoc["usedBytes"] = usedBytes;
        responseDoc["freeBytes"] = freeBytes;
        serializeJson(responseDoc, response);
        sendResponseJsonOTA(response, msgtype, msgId);
    }
}

void BLEController::setOtaCallbacks(std::function<void()> startCallback,
                                    std::function<void(int)> progressCallback,
                                    std::function<void()> successCallback,
                                    std::function<void(const char *)> errorCallback)
{
    otaStartCallback = startCallback;
    otaProgressCallback = progressCallback;
    otaSuccessCallback = successCallback;
    otaErrorCallback = errorCallback;
}

void BLEController::setServiceUUID(const char *uuid)
{
    serviceUUID = uuid;
}

void BLEController::setCharacteristicUUIDs(const char *txUuid, const char *rxUuid)
{
    txUUID = txUuid;
    rxUUID = rxUuid;
}

void BLEController::setOnConnectCallback(std::function<void()> callback)
{
    onConnectCallback = callback;
}

void BLEController::setOnDisconnectCallback(std::function<void()> callback)
{
    startAdvertising();
    onDisconnectCallback = callback;
    // bleController.switchToJsonMode();
}

void BLEController::stop()
{

    // INFO_PRINTLN("BLEController::stop()");
    if (!_initialized)
        return;

    // Stop advertising if it's currently running
    if (BLEDevice::getAdvertising()->isAdvertising())
    {
        BLEDevice::getAdvertising()->stop();
    }

    // Disconnect any connected devices
    if (pServer != nullptr)
    {
        // for (auto client : pServer->getPeerDevices(true))
        // {
        //     pServer->disconnect(client);
        // }
    }

    // Stop all BLE services
    if (pServer != nullptr)
    {
        // for (auto service : pServer->getServices())
        // {
        //     service->stop();
        // }
    }

    // Clear all characteristics
    // if (pTxCharacteristic != nullptr)
    // {
    //     pTxCharacteristic->removeDescriptor(nullptr);
    //     pTxCharacteristic = nullptr;
    // }
    // if (pRxCharacteristic != nullptr)
    // {
    //     pRxCharacteristic->removeDescriptor(nullptr);
    //     pRxCharacteristic = nullptr;
    // }
    // if (pBatCharacteristic != nullptr)
    // {
    //     pBatCharacteristic->removeDescriptor(nullptr);
    //     pBatCharacteristic = nullptr;
    // }
    // if (otaTX != nullptr)
    // {
    //     otaTX->removeDescriptor(nullptr);
    //     otaTX = nullptr;
    // }
    // if (otaRX != nullptr)
    // {
    //     otaRX->removeDescriptor(nullptr);
    //     otaRX = nullptr;
    // }

    // Clear all callbacks
    messageCallbacks.clear();
    highPriorityCallbacks.clear();
    onConnectCallback = nullptr;
    onDisconnectCallback = nullptr;
    otaStartCallback = nullptr;
    otaProgressCallback = nullptr;
    otaSuccessCallback = nullptr;
    otaErrorCallback = nullptr;

    // Clear message queue
    clearMessageQueue();

    // Reset all state variables
    deviceConnected = false;
    isBlockingOperationRunning = false;
    currentBatteryLevel = 0;
    memset(currentSWVersion, 0, sizeof(currentSWVersion));

    // Delete the BLE server
    if (pServer != nullptr)
    {
        // BLEDevice::deleteServer();
        pServer = nullptr;
    }

    // Finally, deinitialize the BLE device
    BLEDevice::deinit(true);

    // INFO_PRINTLN("BLEController::stop() complete");
}

void BLEController::switchToTextMode()
{
    TextMode = true;
    executeTextBufferFlag = false;
    TextBuffer.clear();
    sendMessage("Entered Text mode. Use Control-D to execute.");

    // Register BLE-specific Text functions
}

void BLEController::switchToJsonMode()
{
    TextMode = false;
    TextBuffer.clear();
    JsonDocument doc;
    doc["msgtyp"] = "jsonmode";
    sendMessage(doc);
}

void BLEController::executeTextBuffer()
{
    textBufferIsExecuting = true;
    if (TextMessageCallback != nullptr)
    {
        TextMessageCallback(TextBuffer.c_str());
    }

    textBufferIsExecuting = false;
    executeTextBufferFlag = false;

    TextBuffer.clear();
}

void BLEController::sendTextOutput(const std::string &_output)
{
    String output = _output.c_str();
    output = output + '\n';
    int sendMtu = mtu - 3; // 3 bytes overhead
    // INFO_PRINTLN("Sending BLE message: %s", output.c_str());
    for (size_t i = 0; i < output.length(); i += sendMtu)
    {
        String chunk = output.substring(i, std::min(i + sendMtu, output.length()));
        pTxCharacteristic->setValue((uint8_t *)chunk.c_str(), chunk.length());
        pTxCharacteristic->notify();
        //    delay(1); // Give the client time to process
        delayMicroseconds(100); // Give the client time to process
    }
}

bool BLEController::transferFileFromLittleFS(const char *filename, uint16_t mtu)
{
    if (!LittleFS.begin(true, "/littlefs"))
    {
        return false;
    }

    File file = LittleFS.open(filename, "r");
    if (!file)
    {
        LittleFS.end();
        return false;
    }

    uint8_t buffer[512];      // Or whatever maximum size you want to support
    size_t sendMtu = mtu - 3; // 3 bytes overhead

    while (file.available())
    {
        size_t bytesRead = file.read(buffer, sendMtu);
        if (bytesRead == 0)
        {
            break;
        }

        pTxCharacteristic->setValue(buffer, bytesRead);
        pTxCharacteristic->notify();
        delay(20);
    }

    file.close();
    LittleFS.end();
    return true;
}