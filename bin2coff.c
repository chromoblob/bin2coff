
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

const char *basename(const char *name) {
    const char *result = name;
    for (; *name; name++)
        if (*name == '/') result = name + 1;
    return result;
}

const char *progName;

void printProgNameColon() {
    if (*progName) fprintf(stderr, "%s: ", progName);
}

void Write(int fd, const void *data, size_t size) {
    const char *p = data;
    while (size) {
        ssize_t written = write(fd, p, size);
        if (written == -1) {
            if (errno != EINTR) {
                printProgNameColon();
                perror("write()");
                exit(1);
            }
        } else
            { p += written; size -= written; }
    }
}

int main(int argc, char **argv) {
    progName = basename(argv[0]);
    if (argc > 4 || argc < 2) {
        fprintf(stderr, "Usage: %s FILENAME [ SYMBOL_PREFIX [ END_SYMBOL_PREFIX ] ]\n"
                        "Default values: File_ for symbol prefix, FileEnd_ for end symbol prefix\n"
                      , *progName ? progName : "bin2coff");
        return -1;
    }

    const char *symbolPrefix = "File_", *endSymbolPrefix = "FileEnd_";
    if (argv[2]) {
        symbolPrefix = argv[2];
        if (argv[3]) endSymbolPrefix = argv[3];
    }
    if (!strcmp(symbolPrefix, endSymbolPrefix)) {
        printProgNameColon();
        fprintf(stderr, "symbol prefix and end symbol prefix are same\n");
        return -1;
    }
    const size_t symbolPrefixLen = strlen(symbolPrefix),
                 endSymbolPrefixLen = strlen(endSymbolPrefix);

    const size_t l1 = strlen(argv[1]);
    char outFilename[l1 + 3];
    strcpy(outFilename, argv[1]);
    outFilename[l1]     = '.';
    outFilename[l1 + 1] = 'o';
    outFilename[l1 + 2] = 0;

    const char * const inBasename = basename(argv[1]);
    const size_t inBasenameLen = strlen(inBasename);

    int inFd = open(argv[1], O_RDONLY);
    if (inFd < 0) {
        printProgNameColon();
        fprintf(stderr, "open(\"%s\"): ", argv[1]);
        perror(NULL);
        return 1;
    }

    struct stat Stat;
    if (fstat(inFd, &Stat)) {
        printProgNameColon();
        fprintf(stderr, "stat(\"%s\"): ", argv[1]);
        perror(NULL);
        return 1;
    }

    if (Stat.st_size > UINT32_MAX) {
    InputTooBig:
        printProgNameColon();
        fprintf(stderr, "input file \"%s\" too big\n", argv[1]);
        return -2;
    }

    const uint32_t dataSize = Stat.st_size;

    const char *data = mmap(NULL, dataSize, PROT_READ, MAP_PRIVATE, inFd, 0);
    if (data == MAP_FAILED) {
        printProgNameColon();
        perror("mmap()");
        return 1;
    }

    close(inFd);

    const char * const symbolInner = inBasename;
    const size_t symbolInnerLen = inBasenameLen;

    struct {
        uint16_t Machine;
        uint16_t NumberOfSections;
        uint32_t TimeDateStamp;
        uint32_t PointerToSymbolTable;
        uint32_t NumberOfSymbols;
        uint16_t SizeOfOptionalHeader;
        uint16_t Characteristics;
    } header;

    struct {
        char Name[8];
        uint32_t VirtualSize;
        uint32_t VirtualAddress;
        uint32_t SizeOfRawData;
        uint32_t PointerToRawData;
        uint32_t PointerToRelocations;
        uint32_t PointerToLinenumbers;
        uint16_t NumberOfRelocations;
        uint16_t NumberOfLinenumbers;
        uint32_t Characteristics;
    } section;

    struct __attribute__((packed)) {
        union {
            char ShortName[8];
            struct {
                uint32_t Zeroes;
                uint32_t Offset;
            };
        } Name;
        uint32_t Value;
        uint16_t SectionNumber;
        uint16_t Type;
        uint8_t StorageClass;
        uint8_t NumberOfAuxSymbols;
    } symbols[2];

    const size_t appendZeros = (dataSize + 3 & -4) - dataSize;
    const size_t symbolTableOffset = sizeof header + sizeof section + dataSize + appendZeros;

    if (symbolTableOffset > UINT32_MAX)
        goto InputTooBig;

    header.Machine = 0;
    header.NumberOfSections = 1;
    header.TimeDateStamp = time(NULL);
    header.PointerToSymbolTable = symbolTableOffset;
    header.NumberOfSymbols = 2;
    header.SizeOfOptionalHeader = 0;
    header.Characteristics = 0;

    size_t stringTablePtr = 4;
    bool writeSectionName = inBasenameLen > 8;
    if (writeSectionName) {
        strncpy(section.Name, "/4", 8);
        stringTablePtr += inBasenameLen + 1;
    } else
        strncpy(section.Name, inBasename, 8);

    section.VirtualSize = 0;
    section.VirtualAddress = 0;
    section.SizeOfRawData = dataSize;
    section.PointerToRawData = sizeof header + sizeof section;    // % 4 = 0
    section.PointerToRelocations = 0;
    section.PointerToLinenumbers = 0;
    section.NumberOfRelocations = 0;
    section.NumberOfLinenumbers = 0;
    section.Characteristics = 0x50000040;

    bool writeSymbol = symbolPrefixLen + symbolInnerLen > 8;
    if (writeSymbol) {
        if (stringTablePtr > UINT32_MAX) {
        StringTableOverflow:
            printProgNameColon();
            fprintf(stderr, "arguments too long, string table overflow\n");
            return -2;
        }
        symbols[0].Name.Zeroes = 0;
        symbols[0].Name.Offset = stringTablePtr;
        stringTablePtr += symbolPrefixLen + symbolInnerLen + 1;
    } else {
        strcpy(symbols[0].Name.ShortName, symbolPrefix);
        strncpy(symbols[0].Name.ShortName + symbolPrefixLen, symbolInner, 8 - symbolPrefixLen);
    }
    symbols[0].Value = 0;
    symbols[0].SectionNumber = 1;
    symbols[0].Type = 0;
    symbols[0].StorageClass = 2;
    symbols[0].NumberOfAuxSymbols = 0;

    bool writeEndSymbol = endSymbolPrefixLen + symbolInnerLen > 8;
    if (writeEndSymbol) {
        if (stringTablePtr > UINT32_MAX)
            goto StringTableOverflow;
        symbols[1].Name.Zeroes = 0;
        symbols[1].Name.Offset = stringTablePtr;
        stringTablePtr += endSymbolPrefixLen + symbolInnerLen + 1;
    } else {
        strcpy(symbols[1].Name.ShortName, endSymbolPrefix);
        strncpy(symbols[1].Name.ShortName + endSymbolPrefixLen, symbolInner, 8 - endSymbolPrefixLen);
    }
    symbols[1].Value = dataSize;
    symbols[1].SectionNumber = 1;
    symbols[1].Type = 0;
    symbols[1].StorageClass = 2;
    symbols[1].NumberOfAuxSymbols = 0;

    if (stringTablePtr > UINT32_MAX) goto StringTableOverflow;

    int outFd = creat(outFilename, 0644);
    if (outFd < 0) {
        printProgNameColon();
        fprintf(stderr, "creat(\"%s\"): ", outFilename);
        perror(NULL);
        return 1;
    }

    Write(outFd, &header, sizeof header);
    Write(outFd, &section, sizeof section);
    Write(outFd, data, dataSize);
    {
        uint32_t null = 0;
        Write(outFd, &null, appendZeros);
    }
    Write(outFd, &symbols, sizeof symbols);
    {
        uint32_t stringTableSize = stringTablePtr;
        Write(outFd, &stringTableSize, sizeof stringTableSize);
    }
    if (writeSectionName)
        Write(outFd, inBasename, inBasenameLen + 1);
    if (writeSymbol) {
        Write(outFd, symbolPrefix, symbolPrefixLen);
        Write(outFd, symbolInner, symbolInnerLen + 1);
    }
    if (writeEndSymbol) {
        Write(outFd, endSymbolPrefix, endSymbolPrefixLen);
        Write(outFd, symbolInner, symbolInnerLen + 1);
    }

    return 0;
}

