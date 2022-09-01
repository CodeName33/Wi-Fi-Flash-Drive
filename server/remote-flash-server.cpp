// remote-flash-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//



#include <iostream>
#include <stdio.h>
#include <vector>
#include <thread>
#include <WinSock2.h>
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <winsock2.h>
#include <mstcpip.h>
#include <fstream>

#pragma comment (lib, "Ws2_32.lib")
namespace fs = std::filesystem;


#define MAKELONG64(LOW, HI)      ((__int64)(((DWORD)(((__int64)(LOW)) & 0xffffffff)) | ((__int64)((DWORD)(((__int64)(HI)) & 0xffffffff))) << 32))

#pragma pack (1)

const byte PARTITION_TYPE_NONE = 0x00;
const byte PARTITION_TYPE_FAT12 = 0x01;
const byte PARTITION_TYPE_FAT16_SMALL = 0x04;
const byte PARTITION_TYPE_EXTENDED = 0x05;
const byte PARTITION_TYPE_FAT16_LARGE = 0x06;
const byte PARTITION_TYPE_FAT32 = 0x0B;
const byte PARTITION_TYPE_FAT32_EXT = 0x0C;
const byte PARTITION_TYPE_FAT36_LARGE_EXT = 0x0E;
const byte PARTITION_TYPE_EXTENDED_EXT = 0x0F;

struct PARTITION_ENTRY
{
    byte state = 0x0;
    byte beginHead = 0;
    //unsigned short beginLba = 0;
    unsigned short beginSector : 6;
    unsigned short beginCylinder : 10;
    
    byte type = PARTITION_TYPE_NONE;
    byte endHead = 0;
    //unsigned short endLba = 0;
    unsigned short endSector : 6;
    unsigned short endCylinder : 10;
    
    unsigned int sectorsBetweenMBRAndPartition = 0;
    unsigned int sectorsCount = 0;
};

/*
00h 	Current State of Partition(00h=Inactive, 80h=Active) 	1 Byte
01h 	Beginning of Partition - Head 	1 Byte
02h 	Beginning of Partition - Cylinder/Sector (See Below) 	1 Word
04h 	Type of Partition (See List Below) 	1 Byte
05h 	End of Partition - Head 	1 Byte
06h 	End of Partition - Cylinder/Sector 	1 Word
08h 	Number of Sectors Between the MBR and the First Sector in the Partition 	1 Double Word
0Ch 	Number of Sectors in thePartition 	1 Double Word

PartitionTypes:
00h 	Unknown or Nothing
01h 	12-bit FAT
04h 	16-bit FAT (Partition Smallerthan 32MB)
05h 	Extended MS-DOS Partition
06h 	16-bit FAT (Partition Largerthan 32MB)
0Bh 	32-bit FAT (Partition Up to2048GB)
0Ch 	Same as 0BH, but uses LBA1 13h Extensions
0Eh 	Same as 06H, but uses LBA1 13h Extensions
0Fh 	Same as 05H, but uses LBA1 13h Extensions
*/

struct MBR
{
    byte executableCode[446] = {};
    PARTITION_ENTRY partitionEntry1 = {};
    PARTITION_ENTRY partitionEntry2 = {};
    PARTITION_ENTRY partitionEntry3 = {};
    PARTITION_ENTRY partitionEntry4 = {};
    byte bootRecordSignature[2] = { 0x55, 0xAA };
};

/*
000h 	Executable Code (Boots Computer) 	446 Bytes
1BEh 	1st Partition Entry (See NextTable) 	16 Bytes
1CEh 	2nd Partition Entry 	16 Bytes
1DEh 	3rd Partition Entry 	16 Bytes
1EEh 	4th Partition Entry 	16 Bytes
1FEh 	Boot Record Signature (55hAAh) 	2 Bytes
*/



struct FAT32
{
    byte jumpCode[3] = { 235, 88, 144 };
    char oemName[8] = { 'M', 'S', 'D', 'O', 'S', '5', '.', '0'};
    unsigned short bytesPerSector = 512;
    byte sectorsPerCluster = 0;
    unsigned short reservedSectors = 1;
    byte numberCopiesFat = 1;
    unsigned short maximumRootDirectoryEnties = 0;
    unsigned short numberOfSectorsFat16Small = 0;
    byte mediaDescriptor = 0xF8;
    unsigned short sectorsPerFat16 = 0;
    unsigned short sectorsPerTrack = 1;
    unsigned short numberOfHeads = 1;
    unsigned int numberOfHiddenSectors = 0;
    unsigned int numberOfSectorsInPartition = 0;
    unsigned int numberOfSectorsPerFat = 0;
    unsigned short flags = 0;
    unsigned short version = 0;
    unsigned int clusterNumberOfRootDir = 0;
    unsigned short sectorNumberFSInformation = 0;
    unsigned short sectorNumberBackupBoot = 0;
    byte reserved[12] = {};
    byte logicalDriveNumberPartition = 0;
    byte unused = 0;
    byte extendedSignature = 0x29;
    unsigned int serialNumber = 0x12345678;
    char volumeName[11] = { 'V', 'I', 'R', 'T', 'U', 'A', 'L', ' ', 'F', 'A', 'T'};
    char filSysType[8] = { 'F', 'A', 'T', '3', '2', ' ', ' ', ' '};
    byte executableCode[420] = {};
    byte bootRecordSignature[2] = { 0x55, 0xAA };
};

/*
00h 	Jump Code + NOP 	3 Bytes
03h 	OEM Name (Probably MSWIN4.1) 	8 Bytes
0Bh 	Bytes Per Sector 	1 Word
0Dh 	Sectors Per Cluster 	1 Byte
0Eh 	Reserved Sectors 	1 Word
10h 	Number of Copies of FAT 	1 Byte
11h 	Maximum Root DirectoryEntries (N/A for FAT32) 	1 Word
13h 	Number of Sectors inPartition Smaller than 32MB (N/A for FAT32) 	1 Word
15h 	Media Descriptor (F8h forHard Disks) 	1 Byte
16h 	Sectors Per FAT in Older FATSystems (N/A for FAT32) 	1 Word
18h 	Sectors Per Track 	1 Word
1Ah 	Number of Heads 	1 Word
1Ch 	Number of Hidden Sectors inPartition 	1 Double Word
20h 	Number of Sectors inPartition 	1 Double Word
24h 	Number of Sectors Per FAT 	1 Double Word
28h 	Flags (Bits 0-4 IndicateActive FAT Copy) (Bit 7 Indicates whether FAT Mirroringis Enabled or Disabled) (If FATMirroring is Disabled, the FAT Information is onlywritten to the copy indicated by bits 0-4) 	1 Word
2Ah 	Version of FAT32 Drive (HighByte = Major Version, Low Byte = Minor Version) 	1 Word
2Ch 	Cluster Number of the Startof the Root Directory 	1 Double Word
30h 	Sector Number of the FileSystem Information Sector (See Structure Below)(Referenced from the Start of the Partition) 	1 Word
32h 	Sector Number of the BackupBoot Sector (Referenced from the Start of the Partition) 	1 Word
34h 	Reserved 	12 Bytes
40h 	Logical Drive Number ofPartition 	1 Byte
41h 	Unused (Could be High Byteof Previous Entry) 	1 Byte
42h 	Extended Signature (29h) 	1 Byte
43h 	Serial Number of Partition 	1 Double Word
47h 	Volume Name of Partition 	11 Bytes
52h 	FAT Name (FAT32) 	8 Bytes
5Ah 	Executable Code 	420 Bytes
1FEh 	Boot Record Signature (55hAAh) 	2 Bytes
*/

struct FAT32_INFORMATION
{
    unsigned int firstSignature = 0x52526141;
    byte unknown[480] = {};
    unsigned int fsInfoSignature = 0x72724161;
    unsigned int numberOfFreeClusters = 0;
    unsigned int numberMostRecentlyAllocated = 0;
    byte reserved[12] = {};
    byte unknown2[2] = {};
    byte bootRecordSignature[2] = { 0x55, 0xAA };
};
/*
00h 	First Signature (52h 52h 61h41h) 	1 Double Word
04h 	Unknown, Currently (Mightjust be Null) 	480 Bytes
1E4h 	Signature of FSInfo Sector(72h 72h 41h 61h) 	1 Double Word
1E8h 	Number of Free Clusters (Setto -1 if Unknown) 	1 Double Word
1ECh 	Cluster Number of Cluster that was Most Recently Allocated. 	1 Double Word
1F0h 	Reserved 	12 Bytes
1FCh 	Unknown or Null 	2 Bytes
1FEh 	Boot Record Signature (55hAAh) 	2 Bytes
*/

struct FAT32_FILEDATE
{
    unsigned short day : 5;
    unsigned short month : 4;
    unsigned short yearFrom1980 : 7;
};

struct FAT32_FILETIME
{
    unsigned short secondsHalf : 5;
    unsigned short minutes : 6;
    unsigned short hours : 5;
};

struct FAT32_DIR_ENTRY {
    char DIR_Name[11];
    byte DIR_Attr;
    byte DIR_NTRes;
    byte DIR_CrtTime_Tenth;  // time in tenth of second
    FAT32_FILETIME DIR_CrtTime;       // seconds
    FAT32_FILEDATE DIR_CrtDate;
    FAT32_FILEDATE DIR_LstAccDate;
    unsigned short DIR_FstClusHI;     // +20 first cluster Hi
    FAT32_FILETIME DIR_WrtTime;
    FAT32_FILEDATE DIR_WrtDate;
    unsigned short DIR_FstClusLO;     // +26 first cluster Lo
    unsigned int DIR_FileSize;
} ;

const byte READ_ONLY = 0x01;
const byte HIDDEN = 0x02;
const byte SYSTEM = 0x04;
const byte VOLUME_ID = 0x08;
const byte DIRECTORY = 0x10;
const byte ARCHIVE = 0x20;
const byte ATTR_LONG_NAME = 0x0F;

struct FAT32_LFN_ENTRY {
    byte part = 0;
    wchar_t name1[5] = {};
    byte attr = ATTR_LONG_NAME;
    byte type = 0;
    byte checksum = 0;
    wchar_t name2[6] = {};
    unsigned short fstCLusLO = 0;
    wchar_t name3[2] = {};
};



/*
LDIR_Ord	0	1 	Sequence number (1-20) to identify where this entry is in the sequence of LFN entries to compose an LFN. One indicates the top part of the LFN and any value with LAST_LONG_ENTRY flag (0x40) indicates the last part of the LFN.
LDIR_Name1	1	10 	Part of LFN from 1st character to 5th character.
LDIR_Attr	11	1 	LFN attribute. Always ATTR_LONG_NAME and it indicates this is an LFN entry.
LDIR_Type	12	1 	Must be zero.
LDIR_Chksum	13	1 	Checksum of the SFN entry associated with this entry.
LDIR_Name2	14	12 	Part of LFN from 6th character to 11th character.
LDIR_FstClusLO	26	2 	Must be zero to avoid any wrong repair by old disk utility.
LDIR_Name3	28	4 	Part of LFN from 12th character to 13th character.
*/


struct remote_request
{
    uint64_t position;
    uint32_t length;
};

#pragma pack(0)





std::string format_size(__int64 number)
{
    if (number < 1024)
    {
        return std::to_string(number) + " bytes";
    }
    number /= 1024;
    if (number < 1024)
    {
        return std::to_string(number) + "K";
    }
    number /= 1024;
    if (number < 1024)
    {
        return std::to_string(number) + "MB";
    }
    number /= 1024;
    if (number < 1024)
    {
        return std::to_string(number) + "GB";
    }
    number /= 1024;
    if (number < 1024)
    {
        return std::to_string(number) + "TB";
    }
    return std::to_string(number);
}

struct SVIRTUAL_FILE
{
    std::wstring fileName;
    std::wstring filePath;
    __int64 addressBegin = 0;
    __int64 addressEnd = 0;
    __int64 fileSize = 0;
    FILE* file_handle = nullptr;
};

struct SDIRECTORY_ENTRY
{
    fs::path full_path;
    FAT32_DIR_ENTRY entry = {};
    std::vector<FAT32_LFN_ENTRY> lfn_entries = {};
};

class CFAT32
{
private:
    std::vector<byte> fs_memory;
    std::vector<SVIRTUAL_FILE> virtual_files;
    SVIRTUAL_FILE* last_file = nullptr;
    __int64 image_size = 0;
    unsigned int bytes_per_cluster = 0;
    unsigned int bytes_per_sector = 0;
public:
    std::string volumeLabel;
private:
    
    void create_short_name(const fs::path &file_name, char* short_file_name, std::map<std::string, unsigned int> & short_file_names)
    {
        std::wstring wname = file_name;
        std::string name(wname.begin(), wname.end());
        CharUpperA(&name[0]);
        AnsiToOem(&name[0], &name[0]);

        std::string basename;
        std::string extension;
        size_t point = name.rfind('.');
        if (point == std::string::npos)
        {
            basename = name;
        }
        else
        {
            basename = name.substr(0, point);
            extension = name.substr(point + 1);
        }

        if (basename.size() > 8 || extension.size() > 3)
        {
            std::string name_part = basename.substr(0, 6);
            while (name_part.size() < 6)
            {
                name_part.push_back(L'_');
            }
            short_file_names[name_part]++;
            std::string number = std::to_string(short_file_names[name_part]);
            name_part = name_part.substr(0, 8 - (number.size() + 1));
            
            name = name_part + "~" + number + extension.substr(0, 3);

            memcpy(short_file_name, &name[0], name.size());
        }
        else
        {
            memset(short_file_name, ' ', 8);
            memcpy(short_file_name, &basename[0], basename.size());

            if (extension.size() > 0)
            {
                memcpy(short_file_name + 8, &extension[0], extension.size());
            }
        }
    }

    void convert_time(const std::filesystem::file_time_type& file_time, FAT32_FILEDATE* out_date, FAT32_FILETIME* out_time)
    {
        const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(file_time);
        const auto time64 = std::chrono::system_clock::to_time_t(systemTime);

        tm time = {};
        _localtime64_s(&time, &time64);

        if (out_date)
        {
            out_date->yearFrom1980 = time.tm_year - 80;
            out_date->month = time.tm_mon + 1;
            out_date->day = time.tm_mday;
        }

        if (out_time)
        {
            out_time->hours = time.tm_hour;
            out_time->minutes = time.tm_min;
            out_time->secondsHalf = time.tm_sec / 2;
        }

    }

    unsigned char calc_checksum(unsigned char* pFcbName)
    {
        short FcbNameLen;
        unsigned char Sum;

        Sum = 0;
        for (FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--)
        {
            Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
        }
        return (Sum);
    }


    std::vector<FAT32_LFN_ENTRY> create_lfn(const std::wstring& file_name, FAT32_DIR_ENTRY dir_entry)
    {
        std::vector<FAT32_LFN_ENTRY> entries;

        byte checksum = calc_checksum((unsigned char*)dir_entry.DIR_Name);
        int n = 0;
        int pos = 0;
        while (pos < file_name.size() + 1)
        {
            FAT32_LFN_ENTRY entry = {};
            wchar_t* chars[13];
            chars[0] = &entry.name1[0];
            chars[1] = &entry.name1[1];
            chars[2] = &entry.name1[2];
            chars[3] = &entry.name1[3];
            chars[4] = &entry.name1[4];
            chars[5] = &entry.name2[0];
            chars[6] = &entry.name2[1];
            chars[7] = &entry.name2[2];
            chars[8] = &entry.name2[3];
            chars[9] = &entry.name2[4];
            chars[10] = &entry.name2[5];
            chars[11] = &entry.name3[0];
            chars[12] = &entry.name3[1];

            int len = (file_name.size() + 1 - pos < 13 ? file_name.size() + 1 - pos : 13);

            for (int i = 0; i < len; i++)
            {
                *chars[i] = file_name[pos + i];
            }

            if (len < 13)
            {
                for (int i = len; i < 13; i++)
                {
                    *chars[i] = (wchar_t)65535;
                }
            }

            pos += len;
            n++;
            entry.part = (pos < file_name.size() ? 0 : 0x40) + n;
            entry.checksum = checksum;
            entries.push_back(entry);
        }

        std::reverse(entries.begin(), entries.end());
        return entries;
    }

    void clear()
    {
        free_last_file();
        fs_memory.clear();
        virtual_files.clear();
        image_size = 0;
        bytes_per_cluster = 0;
        bytes_per_sector = 0;
    }

    void list_recursive(const fs::path& directory, std::vector<std::map<std::wstring, SDIRECTORY_ENTRY>> &files, std::vector<unsigned int> &directory_enties, __int64 &total_size, unsigned int &total_files_count)
    {
        int files_index = files.size();
        files.push_back(std::map<std::wstring, SDIRECTORY_ENTRY>());
        directory_enties.push_back(0);
        std::map<std::string, unsigned int> short_file_names;

        for (const auto& file_entry : fs::directory_iterator(directory))
        {
            if (file_entry.is_directory())
            {
                FAT32_DIR_ENTRY entry = {};
                create_short_name(file_entry.path().filename(), entry.DIR_Name, short_file_names);
                convert_time(file_entry.last_write_time(), &entry.DIR_WrtDate, &entry.DIR_WrtTime);
                convert_time(file_entry.last_write_time(), &entry.DIR_CrtDate, &entry.DIR_CrtTime);
                convert_time(file_entry.last_write_time(), &entry.DIR_LstAccDate, nullptr);
                entry.DIR_Attr = DIRECTORY;
                entry.DIR_FileSize = files.size();
                SDIRECTORY_ENTRY dir_entry = { file_entry.path(), entry, create_lfn(file_entry.path().filename(), entry) };
                files[files_index][file_entry.path().filename()] = dir_entry;
                directory_enties[files_index] += 1 + dir_entry.lfn_entries.size();


                list_recursive(directory / file_entry.path().filename(), files, directory_enties, total_size, total_files_count);
            }
            else if (file_entry.file_size() == 0)
            {
                std::cout << "Notice: File " << file_entry.path().filename() << " ignored, size = 0 bytes" << std::endl;
            }
            else if (file_entry.file_size() > 0xFFFFFFFF)
            {
                std::cout << "Warning: File " << file_entry.path().filename() << " not supported, size > 4GB" << std::endl;
            }
            else
            {
                total_size += file_entry.file_size();

                FAT32_DIR_ENTRY entry = {};

                create_short_name(file_entry.path().filename(), entry.DIR_Name, short_file_names);

                convert_time(file_entry.last_write_time(), &entry.DIR_WrtDate, &entry.DIR_WrtTime);
                convert_time(file_entry.last_write_time(), &entry.DIR_CrtDate, &entry.DIR_CrtTime);
                convert_time(file_entry.last_write_time(), &entry.DIR_LstAccDate, nullptr);
                entry.DIR_Attr = ARCHIVE;
                entry.DIR_FileSize = file_entry.file_size();
                SDIRECTORY_ENTRY dir_entry = { file_entry.path(), entry, create_lfn(file_entry.path().filename(), entry) };
                files[files_index][file_entry.path().filename()] = dir_entry;
                directory_enties[files_index] += 1 + dir_entry.lfn_entries.size();
                total_files_count++;
            }
            
        }
    }
public:
    ~CFAT32()
    {
        clear();
    }

    void create(const fs::path& directory)
    {
        std::cout << "FS: creating virtual fs from " << directory << "..." << std::endl;
        clear();
        std::vector<unsigned int> directory_entries;
        std::vector<std::map<std::wstring, SDIRECTORY_ENTRY>> directories;
        std::map<std::string, unsigned int> short_file_names;
        unsigned int total_files_count = 0;

        __int64 total_size = 0;

        list_recursive(directory, directories, directory_entries, total_size, total_files_count);
        {
            FAT32_DIR_ENTRY entry = {};
            memset(entry.DIR_Name, ' ', 11);
            std::string label = volumeLabel.substr(0, 11);
            memcpy(entry.DIR_Name, &label[0], label.size());
            //memcpy(entry.DIR_Name, "VIRTUAL FAT", 11);
            entry.DIR_Attr = VOLUME_ID;
            directories[0][L":entry"].entry = entry;
            directory_entries[0]++;
        }

        

        MBR mbr = {};
        FAT32 fat32 = {};
        FAT32_INFORMATION info = {};

        fat32.sectorsPerCluster = 32;
        fat32.bytesPerSector = 512;

        bytes_per_sector = fat32.bytesPerSector;
        bytes_per_cluster = fat32.sectorsPerCluster * fat32.bytesPerSector;

        unsigned int number_clusters_dirs = 0;

        for (unsigned int count : directory_entries)
        {
            number_clusters_dirs += count * 32 / bytes_per_cluster + (count * 32 % bytes_per_cluster ? 1 : 0);
        }

        unsigned int number_clusters_data = total_size / bytes_per_cluster + total_files_count;

        unsigned int number_clusters_system = 2;
        
        unsigned int number_clusters_fat = ((number_clusters_data + number_clusters_dirs) * 4) / bytes_per_cluster + 1;
        __int64 number_clusters_in_fat = (__int64)number_clusters_system + number_clusters_dirs + number_clusters_data;

        fat32.reservedSectors = 6270;
        fat32.sectorsPerTrack = 63;
        fat32.numberOfSectorsInPartition = (number_clusters_data + number_clusters_fat + number_clusters_dirs) * fat32.sectorsPerCluster + fat32.reservedSectors;
        mbr.partitionEntry1.beginHead = 130;
        //mbr.partitionEntry1
        //mbr.partitionEntry1.beginLba = 3;
        mbr.partitionEntry1.type = 12;
        mbr.partitionEntry1.endHead = 254;
        //mbr.partitionEntry1.endLba =  (fat32.numberOfSectorsInPartition / fat32.sectorsPerTrack) - mbr.partitionEntry1.beginLba; 
        mbr.partitionEntry1.sectorsBetweenMBRAndPartition = 8192;
        mbr.partitionEntry1.sectorsCount = fat32.numberOfSectorsInPartition;
        fat32.numberCopiesFat = 1;

        fat32.mediaDescriptor = 248;

        fat32.numberOfHeads = 255;
        fat32.numberOfHiddenSectors = 8192;
        fat32.numberOfSectorsPerFat = number_clusters_fat * fat32.sectorsPerCluster;
        fat32.clusterNumberOfRootDir = 2;
        fat32.sectorNumberFSInformation = 1;
        fat32.sectorNumberBackupBoot = 6;
        fat32.logicalDriveNumberPartition = 128;
        fat32.extendedSignature = 41;
        fat32.serialNumber = 1049449533;

        info.firstSignature = 1096897106;
        info.fsInfoSignature = 1631679090;

        
        unsigned int root_dir_sectors = ((fat32.maximumRootDirectoryEnties * 32) + (fat32.bytesPerSector - 1)) / fat32.bytesPerSector;
        unsigned int first_data_sector = mbr.partitionEntry1.sectorsBetweenMBRAndPartition + fat32.reservedSectors + (fat32.numberCopiesFat * fat32.numberOfSectorsPerFat) + root_dir_sectors;
        unsigned int first_fat_sector = mbr.partitionEntry1.sectorsBetweenMBRAndPartition + fat32.reservedSectors;

        unsigned int data_sectors = fat32.numberOfSectorsInPartition - (fat32.reservedSectors + (fat32.numberCopiesFat * fat32.numberOfSectorsPerFat) + root_dir_sectors);

        unsigned int total_clusters = data_sectors / fat32.sectorsPerCluster;

        fat32.sectorNumberBackupBoot = fat32.clusterNumberOfRootDir + number_clusters_dirs;

        unsigned int last_sector = (((number_clusters_in_fat)-2) * fat32.sectorsPerCluster) + first_data_sector;

        fs_memory.resize(first_data_sector * fat32.bytesPerSector + number_clusters_dirs * bytes_per_cluster);

        memcpy(&fs_memory[0], &mbr, sizeof(mbr));

        memcpy(&fs_memory[mbr.partitionEntry1.sectorsBetweenMBRAndPartition * fat32.bytesPerSector], &fat32, sizeof(fat32));


        unsigned int* FAT_table = (unsigned int*)(&fs_memory[first_fat_sector * bytes_per_sector]);
        unsigned int last_pos = 0;

        memset(FAT_table, 0, number_clusters_in_fat * 4);

        for (int i = 0; i < fat32.clusterNumberOfRootDir; i++)
        {
            FAT_table[i] = 0x0FFFFFF8;
        }

        unsigned int dir_base_addr = first_data_sector * bytes_per_sector - 2 * bytes_per_cluster;
        unsigned int alloc_cluster = fat32.clusterNumberOfRootDir + number_clusters_dirs;
        unsigned int directory_index = 0;
        __int64 dir_offset_cluster = fat32.clusterNumberOfRootDir;
        std::map<unsigned int, __int64> update_addresses;
        for (auto directory_files : directories)
        {

            unsigned int dir_bytes = 0;
            unsigned int entries_count = directory_entries[directory_index];

            auto found = update_addresses.find(directory_index);
            if (found != update_addresses.end())
            {
                FAT32_DIR_ENTRY* entry = (FAT32_DIR_ENTRY *)&fs_memory[found->second];
                entry->DIR_FstClusLO = LOWORD(dir_offset_cluster);
                entry->DIR_FstClusHI = HIWORD(dir_offset_cluster);
            }

            for (auto it : directory_files)
            {
                for (const FAT32_LFN_ENTRY &lfn : it.second.lfn_entries)
                {
                    memcpy(&fs_memory[dir_base_addr + dir_offset_cluster * bytes_per_cluster + dir_bytes], &lfn, sizeof(lfn));
                    dir_bytes += sizeof(lfn);
                    if (dir_bytes == bytes_per_cluster)
                    {
                        FAT_table[dir_offset_cluster] = dir_offset_cluster + 1;
                        dir_bytes = 0;
                        dir_offset_cluster++;
                    }
                }

                if (it.second.entry.DIR_Attr & DIRECTORY)
                {
                    update_addresses[it.second.entry.DIR_FileSize] = dir_base_addr + dir_offset_cluster * bytes_per_cluster + dir_bytes;
                    it.second.entry.DIR_FileSize = 0;
                }
                else if (!(it.second.entry.DIR_Attr & VOLUME_ID))
                {
                    it.second.entry.DIR_FstClusLO = LOWORD(alloc_cluster);
                    it.second.entry.DIR_FstClusHI = HIWORD(alloc_cluster);
                }
                
                memcpy(&fs_memory[dir_base_addr + dir_offset_cluster * bytes_per_cluster + dir_bytes], &it.second.entry, sizeof(it.second.entry));
                dir_bytes += sizeof(it.second.entry);
                if (dir_bytes == bytes_per_cluster)
                {
                    FAT_table[dir_offset_cluster] = dir_offset_cluster + 1;
                    dir_bytes = 0;
                    dir_offset_cluster++;
                }

                if (it.second.entry.DIR_FileSize > 0)
                {
                    unsigned int length = it.second.entry.DIR_FileSize;
                    __int64 file_cluster = alloc_cluster;
                    unsigned int first_sector_of_file_cluster = ((file_cluster - 2) * fat32.sectorsPerCluster) + first_data_sector;


                    SVIRTUAL_FILE virtual_file;
                    virtual_file.filePath = it.second.full_path;
                    virtual_file.fileName = it.first;

                    //std::cout << "Storing file" << fs::path(it.first) << ", first cluster: " << alloc_cluster << ", byte: " << ((((__int64)alloc_cluster - 2) * fat32.sectorsPerCluster) + first_data_sector) * bytes_per_sector << std::endl;

                    unsigned int begin_cluster = alloc_cluster;
                    virtual_file.addressBegin = ((((__int64)alloc_cluster - 2) * fat32.sectorsPerCluster) + first_data_sector) * bytes_per_sector;
                    alloc_cluster += (it.second.entry.DIR_FileSize / bytes_per_cluster) + (it.second.entry.DIR_FileSize % bytes_per_cluster ? 1 : 0);
                    virtual_file.addressEnd = (((__int64)(alloc_cluster - 2) * fat32.sectorsPerCluster) + first_data_sector) * bytes_per_sector - 1;
                    virtual_file.fileSize = it.second.entry.DIR_FileSize;

                    virtual_files.push_back(virtual_file);

                    for (int i = begin_cluster; i < alloc_cluster; i++)
                    {
                        FAT_table[i] = (i < alloc_cluster - 1 ? i + 1 : 0x0FFFFFF8);
                    }

                    unsigned char* buffer = new unsigned char[bytes_per_cluster];
                }
            }
            directory_index++;
            FAT_table[dir_offset_cluster] = 0x0FFFFFF8;
            if (dir_bytes > 0)
            {
                dir_offset_cluster++;
            }
        }

        info.numberOfFreeClusters = number_clusters_in_fat - (alloc_cluster - 1);
        info.numberMostRecentlyAllocated = alloc_cluster - 1;

        memcpy(&fs_memory[(mbr.partitionEntry1.sectorsBetweenMBRAndPartition + fat32.sectorNumberFSInformation) * fat32.bytesPerSector], &info, sizeof(info));

        __int64 first_sector_of_file_cluster = (((number_clusters_in_fat)-2) * fat32.sectorsPerCluster) + first_data_sector;
        image_size = first_sector_of_file_cluster * fat32.bytesPerSector;

        std::cout << "FS: done, size = " << format_size(total_size) << ", files: " << total_files_count << ", memory used: " << format_size(fs_memory.size()) << std::endl;
    }

    __int64 get_image_size()
    {
        return image_size;
    }

    unsigned int get_sectors_count()
    {
        return (unsigned int)(image_size / bytes_per_sector);
    }

    unsigned int read_data(__int64 position, void* buffer, unsigned int length)
    {
        unsigned int bytes_read = 0;
        if (position < image_size && length > 0)
        {
            if (position < fs_memory.size())
            {
                unsigned int to_read = (position + length > fs_memory.size() ? fs_memory.size() - position : length);

                memcpy(buffer, &fs_memory[position], to_read);

                buffer = ((byte*)buffer) + to_read;
                length -= to_read;
                bytes_read += to_read;
                position += to_read;
            }

            if (length > 0)
            {
                if (last_file != nullptr && last_file->addressBegin <= position && last_file->addressEnd >= position)
                {
                    bytes_read += read_file(last_file, position, buffer, length);
                }

                while (length > 0)
                {
                    int index = find_file(position);
                    if (index < 0)
                    {
                        break;
                    }
                    free_last_file();
                    last_file = &virtual_files[index];
                    if (position > last_file->addressEnd)
                    {
                        unsigned int to_read = (position + length > image_size ? image_size - position : length);

                        memset(buffer, 0, to_read);

                        buffer = ((byte*)buffer) + to_read;
                        length -= to_read;
                        bytes_read += to_read;
                        position += to_read;
                        break;
                    }
                    else
                    {
                        bytes_read += read_file(last_file, position, buffer, length);
                    }
                }
            }
        }
        return bytes_read;
    }

    void debug_test_image()
    {
        std::map<std::wstring, FAT32_DIR_ENTRY> files;
        MBR mbr = {};
        read_data(0, &mbr, sizeof(mbr));

        FAT32 fat32 = {};
        read_data((__int64)mbr.partitionEntry1.sectorsBetweenMBRAndPartition * 512, &fat32, sizeof(fat32));

        FAT32_INFORMATION info = {};
        read_data((__int64)mbr.partitionEntry1.sectorsBetweenMBRAndPartition * 512 + fat32.sectorNumberFSInformation * 512, &info, sizeof(info));

        unsigned int root_dir_sectors = ((fat32.maximumRootDirectoryEnties * 32) + (fat32.bytesPerSector - 1)) / fat32.bytesPerSector;
        unsigned int first_data_sector = mbr.partitionEntry1.sectorsBetweenMBRAndPartition + fat32.reservedSectors + (fat32.numberCopiesFat * fat32.numberOfSectorsPerFat) + root_dir_sectors;
        __int64 first_fat_sector = (__int64)mbr.partitionEntry1.sectorsBetweenMBRAndPartition + fat32.reservedSectors;

        unsigned int root_cluster_32 = fat32.clusterNumberOfRootDir;
        unsigned int first_sector_of_cluster = ((root_cluster_32 - 2) * fat32.sectorsPerCluster) + first_data_sector;

        unsigned int data_sectors = fat32.numberOfSectorsInPartition - (fat32.reservedSectors + (fat32.numberCopiesFat * fat32.numberOfSectorsPerFat) + root_dir_sectors);

        unsigned int total_clusters = data_sectors / fat32.sectorsPerCluster;

        wchar_t lfn_buffer[261] = {};

        unsigned char* FAT_table = new unsigned char[fat32.bytesPerSector];
        unsigned int last_pos = 0;
        for (int i = 0; i < total_clusters; i++)
        {
            unsigned int fat_offset = i * 4;
            __int64 fat_sector = first_fat_sector + (fat_offset / fat32.bytesPerSector);
            unsigned int ent_offset = fat_offset % fat32.bytesPerSector;

            if (fat_sector != last_pos)
            {
                read_data(fat_sector * fat32.bytesPerSector, FAT_table, fat32.bytesPerSector);
                last_pos = fat_sector;
            }
            unsigned int table_value = *(unsigned int*)&FAT_table[ent_offset];
            table_value &= 0x0FFFFFFF;
            if (table_value >= 0x0FFFFFF8)
            {
                int LLL = 0;
            }
            int KKK = 0;
        }


        std::string name;
        name.resize(12);
        __int64 pos = (__int64)first_sector_of_cluster * fat32.bytesPerSector;
        while (true)
        {
            byte entry[32];
            pos += read_data(pos, &entry, 32);

            if (entry[0] == 0x0)
            {
                break;
            }
            else if (entry[0] == 0xE5)
            {
                continue;
            }
            else if (entry[11] == 0x0F)
            {
                FAT32_LFN_ENTRY* lfn = (FAT32_LFN_ENTRY*)entry;
                byte partNumber = lfn->part;
                if (partNumber > 0x40)
                {
                    partNumber -= 0x40;
                    lfn_buffer[partNumber * 13] = 0;
                }

                memcpy(lfn_buffer + partNumber * 13 - 13, lfn->name1, 10);
                memcpy(lfn_buffer + partNumber * 13 - 8, lfn->name2, 12);
                memcpy(lfn_buffer + partNumber * 13 - 2, lfn->name3, 4);
            }
            else
            {
                FAT32_DIR_ENTRY* dir_entry = (FAT32_DIR_ENTRY*)entry;
                OemToAnsiBuff(dir_entry->DIR_Name, &name[0], 11);
                int KKK = 0;

                std::wstring final_name;
                if (lfn_buffer[0] == 0)
                {
                    final_name = std::wstring(name.begin(), name.end());
                    final_name = final_name.substr(0, final_name.size() - 4) + L"." + final_name.substr(final_name.size() - 4, 3);
                }
                else
                {
                    final_name = lfn_buffer;
                }

                files[final_name] = *dir_entry;

                lfn_buffer[0] = 0;
            }
        }

        //*
        for (auto it : files)
        {
            if (!(it.second.DIR_Attr & VOLUME_ID))
            {
                if (it.second.DIR_Attr & DIRECTORY)
                {
                    
                }
                else
                {
                    

                    if (it.second.DIR_FileSize > 0)
                    {
                        __int64 file_cluster = MAKELONG(it.second.DIR_FstClusLO, it.second.DIR_FstClusHI);
                        __int64 first_sector_of_file_cluster = ((file_cluster - 2) * fat32.sectorsPerCluster) + first_data_sector;
                        unsigned int file_size = it.second.DIR_FileSize;

                        unsigned int bytes_per_cluster = fat32.sectorsPerCluster * fat32.bytesPerSector;
                        unsigned char* buffer = new unsigned char[bytes_per_cluster];

                        std::string name(it.first.begin(), it.first.end());
                        std::cout << "Testing file " << fs::path(it.first) << ", start cluster: " << file_cluster << ", byte: " << first_sector_of_file_cluster * fat32.bytesPerSector << std::endl;
                        __int64 n = 0;
                        while (true)
                        {
                            read_data(first_sector_of_file_cluster * fat32.bytesPerSector, buffer, bytes_per_cluster);

                            buffer[16] = 0;

                            if (n == 0)
                            {
                                std::cout << buffer << std::endl;
                            }
                            break;

                            unsigned int bytes_to_write = (file_size < bytes_per_cluster ? file_size : bytes_per_cluster);
                            //fwrite(buffer, 1, bytes_to_write, df);
                            file_size -= bytes_to_write;

                            unsigned int fat_offset = file_cluster * 4;
                            __int64 fat_sector = first_fat_sector + (fat_offset / fat32.bytesPerSector);
                            unsigned int ent_offset = fat_offset % fat32.bytesPerSector;

                            if (fat_sector != last_pos)
                            {
                                read_data(fat_sector * fat32.bytesPerSector, FAT_table, fat32.bytesPerSector);
                                last_pos = fat_sector;
                            }
                            unsigned int table_value = *(unsigned int*)&FAT_table[ent_offset];
                            table_value &= 0x0FFFFFFF;
                            if (table_value >= 0x0FFFFFF8)
                            {
                                break;
                            }

                            file_cluster = table_value;
                            first_sector_of_file_cluster = ((file_cluster - 2) * fat32.sectorsPerCluster) + first_data_sector;
                            n++;
                        }
                        delete[] buffer;

                        std::cout << "========================== OK" << std::endl;

                    }

                    
                }
            }
        }
        //*/

        delete[] FAT_table;

        int KKK = 0;
    }
private:
    int find_file(__int64 position)
    {
        if (virtual_files.size() == 0)
        {
            return -1;
        }
        int min = 0;
        int max = virtual_files.size() - 1;
        int current = (max - min) / 2;

        if (position < virtual_files[min].addressEnd)
        {
            return min;
        }

        if (position >= virtual_files[max].addressBegin)
        {
            return max;
        }

        while (true)
        {
            if (max - min < 5)
            {
                for (current = min; current <= max; current++)
                {
                    if (position >= virtual_files[current].addressBegin && position <= virtual_files[current].addressEnd)
                    {
                        return current;
                    }
                }
                return -1;
            }

            if (position >= virtual_files[current].addressBegin && position <= virtual_files[current].addressEnd)
            {
                return current;
            }

            if (position < virtual_files[current].addressBegin)
            {
                max = current;
                current = min + (max - min) / 2;
            }
            else if (position > virtual_files[current].addressEnd)
            {
                min = current;
                current = min + (max - min) / 2;
            }
        }
    }

    void free_last_file()
    {
        if (last_file != nullptr)
        {
            if (last_file->file_handle != nullptr)
            {
                fclose(last_file->file_handle);
                last_file->file_handle = nullptr;
            }
            last_file = nullptr;
        }
    }

    unsigned int read_file(SVIRTUAL_FILE* file, __int64& position, void*& buffer, unsigned int& length)
    {
        unsigned int bytes_read = 0;
        unsigned int to_read = (position + length > last_file->addressEnd + 1 ? last_file->addressEnd +1 - position : length);

        if (file->file_handle == nullptr)
        {
            file->file_handle = _wfsopen(file->filePath.c_str(), L"rb", _SH_DENYNO);
            if (file->file_handle == nullptr)
            {
                memset(buffer, 0, to_read);
                buffer = ((byte*)buffer) + to_read;
                length -= to_read;
                position += to_read;
                bytes_read += to_read;
                return bytes_read;
            }
        }

        if (last_file->addressBegin + file->fileSize > position)
        {
            unsigned int file_to_read = (position + to_read > last_file->addressBegin + file->fileSize ? (last_file->addressBegin + file->fileSize) - position : to_read);

            if (file_to_read > 0)
            {
                fseek(file->file_handle, position - last_file->addressBegin, SEEK_SET);
                fread(buffer, 1, file_to_read, file->file_handle);

                buffer = ((byte*)buffer) + file_to_read;
                length -= file_to_read;
                position += file_to_read;
                bytes_read += file_to_read;
                to_read -= file_to_read;
            }
        }

        if (to_read > 0)
        {
            memset(buffer, 0, to_read);

            buffer = ((byte*)buffer) + to_read;
            length -= to_read;
            position += to_read;
            bytes_read += to_read;
        }

        return bytes_read;
    }
};

namespace Settings
{
    unsigned int port = 8085;
    fs::path content_path = "L:\\Video";
    std::string volume_label = "VIRTUAL FAT";
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "Russian");

    

    fs::path executable_path(argv[0]);

    fs::path config_path = executable_path.parent_path() / (executable_path.filename().replace_extension("config"));
    std::ifstream infile(config_path);
    std::string line;
    while (std::getline(infile, line))
    {
        line.erase(line.find_last_not_of(" \t") + 1);
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.size() && line[0] != ';' && line[0] != '#')
        {
            std::string key = line.substr(0, line.find('='));
            if (key.size() != line.size())
            {
                std::string value = line.substr(line.find('=') + 1);
                if (key == "path")
                {
                    Settings::content_path = value;
                }
                else if (key == "port")
                {
                    Settings::port = std::stoi(value);
                }
                else if (key == "label")
                {
                    Settings::volume_label = value;
                }
            }
        }
    }

    if (!fs::exists(Settings::content_path))
    {
        std::cout << "Error: path " << Settings::content_path << " not exist" << std::endl;
        return 1;
    }

    CFAT32 fat32image;
    fat32image.volumeLabel = Settings::volume_label;
    fat32image.create(Settings::content_path);

    /*
    FILE* f = fopen("V:\\VFLASH_VURTUAL_DUMP.IMG", "wb");
    byte buffer[512];
    for (__int64 i = 0; i < fat32image.get_image_size(); i += 512)
    {
        if (i == 7471616)
        {
            int KKK = 0;
        }
        unsigned int bytes_read = fat32image.read_data(i, buffer, 512);
        fwrite(buffer, 1, bytes_read, f);
        if (i % (65536*64) == 0)
        {
            std::cout << i << "\r";
        }
    }
    std::cout << std::endl;
    fclose(f);
    //*/

    //fat32image.debug_test_image();

    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    int wsaerr = WSAStartup(wVersionRequested, &wsaData);

    int server_fd, new_socket, valread;
    struct sockaddr_in address = {};
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == 0)
    {
        std::cout << "Error: Create socket failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(Settings::port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        std::cout << "Error: Can't bind on port " << Settings::port << std::endl;
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        std::cout << "Error: Can't listen on port " << Settings::port << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Server: Started on port " << Settings::port << std::endl;

    while ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) >= 0)
    {
        tcp_keepalive ka = { 1, 1000, 1000 };
        DWORD dwSize;
        WSAIoctl(new_socket, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), 0, 0, &dwSize, 0, 0);

        // setting socket options
        int flag = 1;
        if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag)) == -1)
        {
            
        }

        std::cout << "Server: Client connected" << std::endl;

        while (true)
        {
            remote_request request = {};
            char* buffer = (char*)&request;
            int length = sizeof(request);
            
            bool closed = false;
            while (length > 0)
            {
                int bytes_recv = recv(new_socket, buffer, length, 0);
                if (bytes_recv <= 0)
                {
                    closed = true;
                    break;
                }
                length -= bytes_recv;
                buffer += bytes_recv;
            }
            if (closed)
            {
                break;
            }

            if (request.length == 0)
            {
                unsigned int sectors_count = fat32image.get_sectors_count();
                std::cout << "Client: Sectors Count request (" << sectors_count << ")" << std::endl;
                if (send(new_socket, (char*)&sectors_count, 4, 0) != 4)
                {
                    std::cout << "Error: Cant send data to client" << std::endl;
                    closed = true;
                }
            }
            else
            {
                std::cout << "Client: Data request (addr=" << request.position << ", size=" << request.length << ")" << std::endl;

                std::vector<byte> buffer;
                if (request.length > 16777216)
                {
                    std::cout << "Error: request.length > 16777216" << std::endl;
                    closed = true;
                }
                buffer.resize(request.length);
                fat32image.read_data(request.position, &buffer[0], request.length);
                if (send(new_socket, (char*)&buffer[0], request.length, 0) != request.length)
                {
                    std::cout << "Error: Cant send data to client" << std::endl;
                    closed = true;
                }
            }
            if (closed)
            {
                break;
            }
        }
        closesocket(new_socket);
        std::cout << "Client: Connection closed" << std::endl;
    }

    closesocket(server_fd);

    return 0;
}
