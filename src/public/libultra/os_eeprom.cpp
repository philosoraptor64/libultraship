#include "libultraship/libultraship.h"
#include <cstdio>
#ifndef _WIN32
#include <unistd.h>
#endif

extern "C" {

int32_t osEepromProbe(OSMesgQueue* mq) {
    return EEPROM_TYPE_4K;
}

int32_t osEepromLongRead(OSMesgQueue* mq, uint8_t address, uint8_t* buffer, int32_t length) {
    u8 content[512];
    s32 ret = -1;

    const std::string save_file = Ship::Context::GetPathRelativeToAppDirectory("default.sav");
    FILE* fp = fopen(save_file.c_str(), "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fread(content, 1, 512, fp) == 512) {
        memcpy(buffer, content + address * 8, length);
        ret = 0;
    }
    fclose(fp);

    return ret;
}

int32_t osEepromRead(OSMesgQueue* mq, u8 address, u8* buffer) {
    return osEepromLongRead(mq, address, buffer, 8);
}

int32_t osEepromLongWrite(OSMesgQueue* mq, uint8_t address, uint8_t* buffer, int32_t length) {
    u8 content[512] = { 0 };
    if (address != 0 || length != 512) {
        osEepromLongRead(mq, 0, content, 512);
    }
    memcpy(content + address * 8, buffer, length);

    // Written to a temporary file that then replaces the save in one step (rename): if the
    // app is stopped mid-write (a crash, iOS ending it, a dropped online match), the old save
    // stays whole. Writing it in place could leave it empty, and the next partial write
    // would then fill the rest of the save with zeros.
    const std::string save_file = Ship::Context::GetPathRelativeToAppDirectory("default.sav");
    const std::string tmp_file = save_file + ".tmp";
    FILE* fp = fopen(tmp_file.c_str(), "wb");
    if (fp == NULL) {
        return -1;
    }
    s32 ret = fwrite(content, 1, 512, fp) == 512 ? 0 : -1;
    if (fflush(fp) != 0) {
        ret = -1;
    }
#ifndef _WIN32
    if (ret == 0 && fsync(fileno(fp)) != 0) {
        ret = -1;
    }
#endif
    if (fclose(fp) != 0) {
        ret = -1;
    }
#ifdef _WIN32
    if (ret == 0) {
        remove(save_file.c_str()); // rename doesn't replace an existing file on Windows
    }
#endif
    if (ret == 0 && rename(tmp_file.c_str(), save_file.c_str()) != 0) {
        ret = -1;
    }
    if (ret != 0) {
        remove(tmp_file.c_str());
    }
    return ret;
}

int32_t osEepromWrite(OSMesgQueue* mq, uint8_t address, uint8_t* buffer) {
    return osEepromLongWrite(mq, address, buffer, 8);
}
}
