#ifndef SPIFFSFILE_H
#define SPIFFSFILE_H
#include <Arduino.h>
#include <spiffs.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#define MAX_FILENAME_LENGTH 255
#define MAX_DATA_LENGTH 512

namespace SpiffsFile {

inline File currentFile;
inline size_t currentFileSizeFull = 0;
inline size_t currentFileSize = 0;
inline const size_t BUFFER_SIZE = 4096;
inline uint8_t buffer[BUFFER_SIZE];
inline size_t bufferPos = 0;
inline std::string currentFileName;

enum ErrorCode
{
  FILE_OK = 0,
  FILE_NOT_FOUND,
  FILE_OPEN_FAILED,
  FILE_WRITE_FAILED,
  FILE_READ_FAILED,
  FILE_CLOSE_FAILED,
  FILE_DELETE_FAILED,
  FILE_RENAME_FAILED,
  FILE_FORMAT_FAILED,
  FILE_INIT_FAILED,
  FILE_SIZE_MISMATCH,
  SPIFFS_NOT_INITIALIZED,
  SPIFFS_INITIALIZED,
  SPIFFS_FORMAT_FAILED,
  SPIFFS_FORMAT_SUCCESS,
  FILE_NOT_OPENED,
  FILE_WRITE_COMPLETE,
  UNKNOWN_ERROR
};

inline ErrorCode initSPIFFS();
inline ErrorCode formatSPIFFS();
inline ErrorCode createFile(const std::string &filename, size_t size);
inline ErrorCode writeFile(const std::string &data);
inline ErrorCode closeFile();
inline ErrorCode deleteFile(const std::string &filename);
inline ErrorCode renameFile(const std::string &oldFilename, const std::string &newFilename);
inline ErrorCode listFiles(JsonArray &jsonArray);
inline String errorCodeToString(ErrorCode errorCode);

 inline String errorCodeToString(ErrorCode errorCode)
  {
    switch (errorCode)
    {
      case FILE_OK:
        return "FILE_OK";
      case FILE_NOT_FOUND:
        return "FILE_NOT_FOUND";
      case FILE_OPEN_FAILED:
        return "FILE_OPEN_FAILED";
      case FILE_WRITE_FAILED:
        return "FILE_WRITE_FAILED";
      case FILE_READ_FAILED:
        return "FILE_READ_FAILED";
      case FILE_CLOSE_FAILED:
        return "FILE_CLOSE_FAILED";
      case FILE_DELETE_FAILED:
        return "FILE_DELETE_FAILED";
      case FILE_RENAME_FAILED:
        return "FILE_RENAME_FAILED";
      case FILE_FORMAT_FAILED:
        return "FILE_FORMAT_FAILED";
      case FILE_INIT_FAILED:
        return "FILE_INIT_FAILED";
      case FILE_SIZE_MISMATCH:
        return "FILE_SIZE_MISMATCH";
      case SPIFFS_NOT_INITIALIZED:
        return "SPIFFS_NOT_INITIALIZED";
      case SPIFFS_INITIALIZED:
        return "SPIFFS_INITIALIZED";
      case FILE_NOT_OPENED:
        return "FILE_NOT_OPENED";
      case FILE_WRITE_COMPLETE:
        return "FILE_WRITE_COMPLETE";
      case SPIFFS_FORMAT_FAILED:
        return "SPIFFS_FORMAT_FAILED";
      case SPIFFS_FORMAT_SUCCESS:
        return "SPIFFS_FORMAT_SUCCESS";
      default:
        return "UNKNOWN_ERROR";
    }
  }

 inline ErrorCode initSPIFFS()
  {
    if (!SPIFFS.begin(true))
    {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return SPIFFS_NOT_INITIALIZED;
    }
    Serial.println("SPIFFS initialized successfully");
    return SPIFFS_INITIALIZED;
  }

 inline ErrorCode formatSPIFFS()
  {
    if (!SPIFFS.format())
    {
      return SPIFFS_FORMAT_FAILED;
    }
    return SPIFFS_FORMAT_SUCCESS;
  }

 inline ErrorCode createFile(const std::string &filename, size_t size)
  {
    if (filename.length() > MAX_FILENAME_LENGTH)
    {
      return FILE_OPEN_FAILED;
    }

    // Serial.printf("Creating file: %s size: %d\n", filename.c_str(), size);
    if (SPIFFS.exists(filename.c_str()))
    {
      SPIFFS.remove(filename.c_str());
    }

    currentFile = SPIFFS.open(filename.c_str(), "w");
    if (!currentFile)
    {
      return FILE_OPEN_FAILED;
    }

    currentFileSize = 0;
    currentFileSizeFull = size;
    currentFileName = filename;
    bufferPos = 0;

    return FILE_OK;
  }

 inline void flushBuffer()
  {
    if (bufferPos > 0)
    {
      size_t bytesWritten = currentFile.write(buffer, bufferPos);
      if (bytesWritten != bufferPos)
      {
        currentFile.flush();
        currentFile.close();
        currentFile = SPIFFS.open(currentFileName.c_str(), "a");
        if (!currentFile)
        {
          return;
        }
        bytesWritten = currentFile.write(buffer, bufferPos);
        currentFile.flush();
        if (bytesWritten != bufferPos)
        {
          currentFile.close();
          return;
        }
      }
      else
      {
        // Serial.printf("Wrote %d bytes\n", bytesWritten);
      }
      currentFileSize += bytesWritten;
      bufferPos = 0;
    }
  }

 inline ErrorCode writeFile(const std::string &data)
  {
    if (!currentFile)
    {
      return FILE_NOT_OPENED;
    }

    const uint8_t *dataPtr = reinterpret_cast<const uint8_t *>(data.c_str());
    size_t remainingBytes = data.length();

    while (remainingBytes > 0)
    {
      size_t bytesToCopy = std::min(remainingBytes, BUFFER_SIZE - bufferPos);
      memcpy(buffer + bufferPos, dataPtr, bytesToCopy);
      bufferPos += bytesToCopy;
      dataPtr += bytesToCopy;
      remainingBytes -= bytesToCopy;

      if (bufferPos == BUFFER_SIZE)
      {
        flushBuffer();
        if (!currentFile)
        {
          return FILE_WRITE_FAILED;
        }
      }
    }

    if (currentFileSize + bufferPos > currentFileSizeFull)
    {
      closeFile();
      return FILE_SIZE_MISMATCH;
    }

    if (currentFileSize + bufferPos == currentFileSizeFull)
    {
      flushBuffer();
      if (!currentFile)
      {
        return FILE_WRITE_FAILED;
      }
      closeFile();
      return FILE_WRITE_COMPLETE;
    }

    return FILE_OK;
  }

inline  ErrorCode closeFile()
  {
    if (!currentFile)
    {
      return FILE_CLOSE_FAILED;
    }

    if (currentFileSize != currentFileSizeFull)
    {
      currentFile.close();
      return FILE_SIZE_MISMATCH;
    }

    currentFile.close();
    currentFileSize = 0;
    currentFileSizeFull = 0;
    currentFileName = "";
    return FILE_OK;
  }

 inline ErrorCode deleteFile(const std::string &filename)
  {
    if (!SPIFFS.exists(filename.c_str()))
    {
      return FILE_NOT_FOUND;
    }

    if (SPIFFS.remove(filename.c_str()))
    {
      return FILE_OK;
    }
    else
    {
      return FILE_DELETE_FAILED;
    }
  }

 inline ErrorCode renameFile(const std::string &oldFilename, const std::string &newFilename)
  {
    if (!SPIFFS.exists(oldFilename.c_str()))
    {
      return FILE_NOT_FOUND;
    }

    if (SPIFFS.rename(oldFilename.c_str(), newFilename.c_str()))
    {
      return FILE_OK;
    }
    else
    {
      return FILE_RENAME_FAILED;
    }
  }

// inline ErrorCode listFiles(JsonArray &jsonArray)
// {
//   File root = SPIFFS.open("/");
//   File file = root.openNextFile();
//   while (file)
//   {
//     JsonObject fileObj = jsonArray.createNestedObject();
    
//     // Sanitize the file name by removing any invalid characters
//     String sanitizedFileName = file.name();
//     sanitizedFileName.replace("\\", "\\\\");
//     sanitizedFileName.replace("\"", "\\\"");
    
//     fileObj["name"] = sanitizedFileName;
//     fileObj["size"] = file.size();
    
//     delay(1);
//     Serial.printf("File: %s, size: %d\n", file.name(), file.size());
//     file = root.openNextFile();
//   }
//   return FILE_OK;
// }


inline ErrorCode listFiles(JsonArray &jsonArray)
{
  // Serial.println("listFiles");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file)
  {
    JsonObject fileObj = jsonArray.createNestedObject();
    
    // Sanitize the file name by removing any invalid characters
    String sanitizedFileName = file.name();
    sanitizedFileName.replace("\\", "\\\\");
    sanitizedFileName.replace("\"", "\\\"");
    
    fileObj["name"] = sanitizedFileName;
    fileObj["size"] = file.size();
    
    delay(1);
    // Serial.printf("File: %s, size: %d\n", file.name(), file.size());
    file = root.openNextFile();
  }
  return FILE_OK;
}

 inline int numOfFiles()
{
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  int count = 0;
  while (file)
  {
    if (!file.isDirectory())
    {
      count++;
    }
    file = root.openNextFile();
  }
  return count;
}


} // namespace SpiffsFile
#endif