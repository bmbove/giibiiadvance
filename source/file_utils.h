// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (c) 2011-2015, 2019, Antonio Niño Díaz
//
// GiiBiiAdvance - GBA/GB emulator

#ifndef FILE_UTILS__
#define FILE_UTILS__

void DirSetRunningPath(char *path);
char *DirGetRunningPath(void);
char *DirGetBiosFolderPath(void);
char *DirGetScreenshotFolderPath(void);

void FileLoad_NoError(const char *filename, void **buffer, unsigned int *size_);
void FileLoad(const char *filename, void **buffer, unsigned int *size_);

int FileExists(const char *filename); // Returns 1 if file exists

int DirCheckExistence(char *path);
int DirCreate(char *path);

char *FU_GetNewTimestampFilename(const char *basename);

#endif // FILE_UTILS__
