/* 
 * File:   utils.cpp
 * Author: Richard Greene
 * 
 * Some handy utilities
 *
 * Created on April 17, 2014, 3:16 PM
 */

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <arpa/inet.h>
#include <iwlib.h>
#include <exception>

#include <SDL/SDL.h>

#define RAPIDJSON_ASSERT(x)                         \
  if(x);                                            \
  else throw std::exception();  

#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filestream.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/document.h>

using namespace rapidjson;

#include <Filenames.h>
#include <Version.h>
#include <Logger.h>
#include <ErrorMessage.h>
#include <MessageStrings.h>
#include <Shared.h>
#include <utils.h>
#include "Hardware.h"

/// Get the current time in millliseconds
long GetMillis(){
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // printf("time = %d sec + %ld nsec\n", now.tv_sec, now.tv_nsec);
    return now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

long startTime = 0;

/// Start the stopwatch timer
void StartStopwatch()
{
    startTime = GetMillis();    
}

/// Stop the stopwatch and return its time in millliseconds
long StopStopwatch()
{
    return GetMillis() - startTime;
}

/// Get the version string for this firmware.  Currently we just return a 
/// string constant, but this wrapper allows for alternate implementations.
std::string GetFirmwareVersion()
{
    return FIRMWARE_VERSION "\n";
}

/// Get the board serial number.  Currently we just return the main Sitara 
/// board's serial no., but this wrapper allows for alternate implementations.
std::string GetBoardSerialNum()
{
    static char serialNo[14] = {0};
    if(serialNo[0] == 0)
    {
        memset(serialNo, 0, 14);
        int fd = open(BOARD_SERIAL_NUM_FILE, O_RDONLY);
        if(fd < 0 || lseek(fd, 16, SEEK_SET) != 16
                  || read(fd, serialNo, 12) != 12)
            LOGGER.LogError(LOG_ERR, errno, ERR_MSG(SerialNumAccess));
        close(fd);
        serialNo[12] = '\n';
    }
    return serialNo;
}

/// Get the WiFi driver's mode
/// See http://sourceforge.net/mirror/android-wifi-tether/code/434/tree/tools/wireless-tools/iwconfig.c
int GetWiFiMode()
{
    int skfd; // socket file descriptor
    int retVal = -1;
    
    // Create a channel to the NET kernel. 
    if((skfd = iw_sockets_open()) < 0)
    {
      LOGGER.LogError(LOG_ERR, errno, ERR_MSG(CantOpenSocket));
      return retVal;
    }

    struct wireless_info info;
    memset(&info, 0, sizeof(struct wireless_info));
    
    if(iw_get_basic_config(skfd, WIFI_INTERFACE, &(info.b)) < 0 || 
       !info.b.has_mode)
        LOGGER.LogError(LOG_ERR, errno, ERR_MSG(CantGetWiFiMode));
    else
        retVal = info.b.mode;     

    // Close the socket.
    iw_sockets_close(skfd);

    return(retVal); 
}

/// Get the IP address of the WiFi or Ethernet connection, if any.
/// See http://stackoverflow.com/a/265978/2832475
std::string GetIPAddress()
{
    std::string ipAddress = NO_IP_ADDRESS;
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;
    char eth0Address[INET_ADDRSTRLEN] = {'\0'};
    char wlan0Address[INET_ADDRSTRLEN] = {'\0'};
    char* address;

    if(getifaddrs(&ifAddrStruct) == 0)
    {
        for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }
            if (ifa->ifa_addr->sa_family == AF_INET) 
            { 
                if(strcmp(ifa->ifa_name, ETHERNET_INTERFACE) == 0)
                    address = eth0Address;
                else if(strcmp(ifa->ifa_name, WIFI_INTERFACE) == 0)
                    address = wlan0Address;
                else
                    continue;

                tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                inet_ntop(AF_INET, tmpAddrPtr, address, INET_ADDRSTRLEN);
            } 
        }
        if(ifAddrStruct != NULL) 
            freeifaddrs(ifAddrStruct);
    
        if(strlen(wlan0Address) > 0 && GetWiFiMode() != WIFI_ACCESS_POINT_MODE)
        {
            // we found an IP address for WiFi, 
            // and it's not in access point mode
            ipAddress = wlan0Address;
        }
        else if(strlen(eth0Address) > 0)
        {
            // we found an IP address for Ethernet
            ipAddress = eth0Address;
        }
    }
    else
        LOGGER.LogError(LOG_ERR, errno, ERR_MSG(IPAddressAccess));
    
    return ipAddress;
}

/// Removes all the files in specified directory
bool PurgeDirectory(std::string directoryPath)
{
    struct dirent* nextFile;
    DIR* folder;
    
    folder = opendir(directoryPath.c_str());

    // opendir returns NULL pointer if argument is not an existing directory
    if (folder == NULL) return false;
    
    while (nextFile = readdir(folder))
    {
        // skip current directory and parent directory
        if (strcmp(nextFile->d_name, ".") == 0 || 
            strcmp(nextFile->d_name, "..") == 0)
            continue;
        std::ostringstream filePath;
        filePath << directoryPath << "/" << nextFile->d_name;
        remove(filePath.str().c_str());
    }

    closedir(folder);
    return true;
}

/// Copy a file specified by sourcePath
/// If the specified destination path is a directory, use the source filename
/// as the destination filename, otherwise use filename specified in destination 
/// path
/// This function only supports the following source/destination paths:
/// sourcePath must look like /path/to/file
/// providedDestinationPath can be /some/directory, file will be copied to 
/// /some/directory/file
/// providedDestinationPath can be /some/directory/otherFile, file will be 
/// copied to /some/directory/otherFile
/// providedDestinationPath must not have trailing slash if it is a directory
/// Anything else is not supported
bool Copy(std::string sourcePath, std::string providedDestinationPath)
{
    std::ifstream sourceFile(sourcePath.c_str(), std::ios::binary);
    std::string destinationPath;
    DIR* dir;

    if (!sourceFile.is_open())
    {
        std::cerr << "could not open source file (" 
                  << sourcePath << ") for copy operation" << std::endl;
        return false;
    }
   
    // opendir returns NULL if the argument is not an existing directory
    dir = opendir(providedDestinationPath.c_str());
    
    if (dir != NULL)
    {
        // providedDestinationPath is a directory, 
        // use source filename as destination filename
        size_t startPos = sourcePath.find_last_of("/") + 1;
        std::string fileName = 
                sourcePath.substr(startPos, sourcePath.length() - startPos);
        destinationPath = providedDestinationPath + std::string("/") + fileName;
    }
    else
    {
        // providedDestinationPath includes destination file name
        destinationPath = providedDestinationPath;
    }

    closedir(dir);
    
    std::ofstream destinationFile(destinationPath.c_str(), std::ios::binary);

    if (!destinationFile.is_open())
    {
        std::cerr << "could not open destination file (" 
                  <<  destinationPath << ") for copy operation" << std::endl;
        return false;
    }
    
    destinationFile << sourceFile.rdbuf();
    return true;
}

/// Makes a directory if it does not exist
int MkdirCheck(std::string path)
{
    struct stat st;
    int status = 0;

    if (stat(path.c_str(), &st) != 0)
    {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        status = -1;
    }

    return(status);
}

/// Ensure all directories in path exist
/// See http://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
int MakePath(std::string path)
{
    const char *pp;
    char *sp;
    int status;
    std::string copypath = path;

    status = 0;
    pp = copypath.c_str();
    while (status == 0 && (sp = (char*)strchr(pp, '/')) != 0)
    {
        if (sp != pp)
        {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = MkdirCheck(copypath);
            *sp = '/';
        }
        pp = sp + 1;
    }
    if (status == 0)
        status = MkdirCheck(path);
    return (status); 
}

/// Get a universally unique identifier, as a 36-character string
void GetUUID(char* uuid)
{
    memset(uuid, 0, UUID_LEN + 1);
    int fd = open(UUID_FILE, O_RDONLY); 
    if(fd < 0)
    {
        LOGGER.LogError(LOG_ERR, errno, ERR_MSG(CantOpenUUIDFile), 
                                                UUID_FILE);
        return;
    }

    read(fd, uuid, UUID_LEN);
    
    close(fd);
}

#define LOAD_BUF_LEN (1024)
/// Determines if smith-client is currently connected to the Spark backend 
/// server via the Internet.
bool IsInternetConnected()
{
    bool isConnected = false;
    
    try
    { 
        FILE* pFile = fopen(SMITH_STATE_FILE, "r");
        char buf[LOAD_BUF_LEN];
        FileReadStream frs(pFile, buf, LOAD_BUF_LEN);
        
        Document doc;
        doc.ParseStream(frs);
        
        const Value& connected = doc[INTERNET_CONNECTED_KEY];
        
        if(connected.IsTrue())
            isConnected = true;
        
        fclose(pFile); 
    }
    catch(std::exception)
    {
        LOGGER.HandleError(CantDetermineConnectionStatus);
    }

    return isConnected;
}