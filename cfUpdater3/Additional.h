#pragma once
#include "rain.h"
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
	DL,
	TRANSFER,
	COPY
} MESSAGEID;

typedef enum {
	lceOK,
	lceTerminated,
	lceConnectionFailed
} LASTCOPYERROR;

extern const char messages[][50];
extern const char messages2[][50];
extern bool isNcf;

typedef enum
{
	CreateArchive,
	CreatePatch,
	ApplyPatch,
	DLFromSteamServers,
	DLFromCFTContentServer
} ACTION;

typedef struct
{
	ACTION Action;
	GCF remoteGcf;
	GCF localGcf;
	char *CommonPath;

	int AppID;
	int ToVersion;
	char *gdsServer;

	unsigned __int64 TotalSize;
	unsigned __int64 ProcessedSize;

	LASTCOPYERROR LastErr;
} DLDATASTRUCT;

GCF RAIN_Reconnect(DLDATASTRUCT DLDS, MESSAGEID Msg = DL);
void printChecksums(GCF gcf, char * filePath);
bool copyFile(DLDATASTRUCT &DLDS, GCF fromGcf, GCF toGcf, char * filePath, char *CommonPath, MESSAGEID Message = DL);
bool FileExists(char *FN);
UINT GetLastVersion(int AppID);
CDR LoadCDR();
bool GetFile(char *CFfile, int len);
bool DataMismatch(GCF remoteGcf, GCF localGcf, char *File, bool IgnoreFilesSize=false);
bool IsAppID(char *str);
void GetBaseGCFName(char *src, char *dst, int times);
void GetParentDir(char *src, char *dst);
void GetFileName(char *src, char *dst);
void die(GCF localGcf, GCF remoteGcf);
void convert(char *unix_style_path, char *win_style_path, bool toWin);
bool ForceDirectories(char *Dir);
void PrintUsage();
//void CFTAPI_init();
void DLDataSize(DLDATASTRUCT &DLDS, char *Prefix, unsigned __int64 &DLSize, bool IgnoreFilesSize = false);
bool DLData(DLDATASTRUCT &DLDS, char *Prefix, GCF updGcf=0, bool IgnoreFilesSize = false, MESSAGEID Message=DL);
