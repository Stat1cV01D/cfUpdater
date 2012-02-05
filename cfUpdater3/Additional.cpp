#include "stdafx.h"
#include "Additional.h"

bool inited=false;

const char messages[][50] = { "Downloading", "Transferring", "Copying" };
const char messages2[][50] = { "downloaded", "transferred", "copied" };
bool isNcf;

/*void CFTAPI_init()
{
	if (inited)
		return;
	CFTAPI_initLogs(NULL, true);
	CFTAPI_setLogLevel(0);
	inited=true;
}*/

GCF RAIN_Reconnect(DLDATASTRUCT DLDS, MESSAGEID Msg)
{
	if (DLDS.remoteGcf)
		RAIN_unmountGcf(DLDS.remoteGcf);

	if (Msg == DL)
		return RAIN_mountGcfOnContentServer(DLDS.gdsServer, DLDS.AppID, DLDS.ToVersion);
	else
		return RAIN_mountGcfOnCFTContentServer(DLDS.gdsServer, DLDS.AppID, DLDS.ToVersion);
}

bool FileExists(char *FN)
{
	WIN32_FIND_DATAA FD;
	HANDLE hFind = FindFirstFileA(FN, &FD);
	FindClose(hFind);
	return (hFind != INVALID_HANDLE_VALUE);
}

void printChecksums(GCF gcf, char * filePath)
{
	Checksums* checksums=RAIN_getChecksums(gcf,filePath);
	
	printf("%s has %u checksums :\n",filePath,checksums->count);
	
	for (unsigned int ind=0;ind<checksums->count;ind++)
	{
		printf("[%08X]",checksums->checksums[ind]);
	}
	printf("\n");
	RAIN_freeChecksums(checksums);
}

bool copyFile(DLDATASTRUCT &DLDS, GCF fromGcf, GCF toGcf, char * filePath, char *CommonPath, MESSAGEID Message)
{
	// NOTE: localGcf an remoteGcf of DLDS are not used here!
	__int64 size=RAIN_size(fromGcf,filePath);
	bool IsDestIsNCF = (isNcf && 
						(DLDS.Action == DLFromSteamServers || 
						DLDS.Action == DLFromCFTContentServer || 
						DLDS.Action == ApplyPatch));
	printf("%s %s (%u bytes) :\n", messages[Message], filePath, size);

	GCF_ENTRY fromFile=RAIN_openToRead(fromGcf,filePath);

	if (!fromFile)
	{
		printf("Failed to open origin file\n");
		return false;
	}
	
	HANDLE toFile;
	if (!IsDestIsNCF)
	{
		toFile=(HANDLE)RAIN_openToWrite(toGcf,filePath);

		if (!toFile)
		{
			printf("Failed to open target file\n");
			return false;
		}	
	}
	else
	{
		char File[MAX_PATH];

		lstrcpy(&File[0], CommonPath);	
		convert(filePath, &File[ lstrlen(&File[0]) ], true);
					
		toFile = CreateFile(&File[0], GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (toFile == INVALID_HANDLE_VALUE) 
		{ 
			printf("Could not open or create file (error %d)\n", GetLastError());
			return false;
		}
	}

	int bufsize = (DLDS.Action == CreatePatch ? 100000 : 1000);

	UCHAR *buffer = new UCHAR[bufsize];
	memset(&buffer[0], 0, bufsize);
	__int64 copied=0;
	
	float overallProgress = ((float)DLDS.ProcessedSize/(float)DLDS.TotalSize)*100;
		
	bool result=true;
	while (copied<size)
	{
		unsigned int len = 0;
		
		for (int i=0; i<2; i++)
		{
			len=RAIN_read(fromFile,buffer,bufsize);
			if (len)
				break;
		}

		if (!len)
		{
			printf("\nNo more data available or data corrupted (%d/%d Kb)\n", copied << 10, size << 10/*, messages2[Message]*/);
			result=false;
			break;
		}
		
		if (IsDestIsNCF)
		{
			DWORD written;
			WriteFile(toFile, buffer, len, &written, 0);
		}
		else
			RAIN_write((GCF_ENTRY)toFile,buffer,len);

		copied += len;
		float progress = (float)copied/(float)size*100;
		printf("\r%.2f %% (%.2f %% overall)", progress, overallProgress);
		//printf(".");
	}
	DLDS.ProcessedSize += size;

	delete [] buffer;
	printf("\n");
	RAIN_close(fromFile);
	if (IsDestIsNCF)
		CloseHandle(toFile);
	else
		RAIN_close((GCF_ENTRY)toFile);

	printf("\nFile %s\n", messages2[Message]);
	return result;
}

CDR LoadCDR()
{
	CDR cdr=0;
	if (!FileExists("ContentDescriptionRecord.bin"))
	{
		printf("Downloading CDR\n");

		cdr=RAIN_downloadCDR("gds1.steampowered.com:27030");
    
		if (!cdr)
		{
			printf("Failed to download cdr\n");
			return 0;
		}
	
		printf("Saving CDR\n");
		RAIN_save(cdr,"ContentDescriptionRecord.bin");
		RAIN_freeCDR(cdr);
	}

	printf("Loading CDR\n");
	cdr=RAIN_load("ContentDescriptionRecord.bin");

	return cdr;
}

UINT GetLastVersion(int AppID)
{
	CDR cdr=LoadCDR();
	if (!cdr)
	{
		return -1;
	}
	
	APPInfo* info=RAIN_getAppInfo(cdr, AppID);
	UINT LastVer = info->currentVersion;

	RAIN_freeAppInfo(info);
	
	RAIN_freeCDR(cdr);

	return LastVer;
}

bool DataMismatch(GCF remoteGcf, GCF localGcf, char *File, bool IgnoreFilesSize)
{
	unsigned __int64 RemoteSize;
	// We shouldn't copy the source file if it is missing
	if ( !IgnoreFilesSize && (RemoteSize = RAIN_effectiveSize(remoteGcf, File)) && (RemoteSize != RAIN_effectiveSize(localGcf,File)) )
		return true;

	Checksums* RemoteChecksums=RAIN_getChecksums(remoteGcf, File);
	Checksums* LocalChecksums=RAIN_getChecksums(localGcf, File);

	if (RemoteChecksums->count != LocalChecksums->count)
	{
		RAIN_freeChecksums(RemoteChecksums);
		RAIN_freeChecksums(LocalChecksums);
		return true;
	}

	bool failed = false;
	
	for (unsigned int ind=0;ind<RemoteChecksums->count;ind++)
	{
		if (RemoteChecksums->checksums[ind] != LocalChecksums->checksums[ind])
		{
			 failed = true;
			 break;
		}
	}

	RAIN_freeChecksums(RemoteChecksums);
	RAIN_freeChecksums(LocalChecksums);

	return failed;
}

bool IsAppID(char *str)
{
	int len = strlen(str);
	for(int i=0; i<len; i++)
	{
		if (str[i]<'0' || str[i]>'9')
			return false;
	}
	return true;
}

// in fact it's like getting File Extension several times
void GetBaseGCFName(char *src, char *dst, int times)
{
	lstrcpyA(dst, src);

	for(int j=0; j<times; j++)
	{
		int len = strlen(dst);
		for(int i=len; i>=0; i--)
		{
			if (dst[i] == '.')
			{
				dst[i] = 0;
				break;
			}
		}
	}
}

void GetFileName(char *src, char *dst)
{
	int len = strlen(src);
	int i;
	for(i=len; i>=0; i--)
	{
		if (dst[i] == '\\')
		{
			break;
		}
	}
	i++;

	// now i has offset ot start from
	int j;
	for(j=0; i<len; i++, j++)
	{
		dst[j] = src[i];
	}
	dst[j]=0;
}

void GetParentDir(char *src, char *dst)
{
	lstrcpyA(dst, src);

	int len = strlen(dst);
	for(int i=len; i>=0; i--)
	{
		if (dst[i] == '\\')
		{
			dst[i] = 0;
			break;
		}
	}
}

bool GetFile(char *CFfile, int len)
{
	GCF localGcf=0;
	GetFullPathName(&CFfile[0], len, &CFfile[0], 0); 
	if (!FileExists(&CFfile[0]))
	{
		char Path[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, &Path[0]);
		lstrcatA(&Path[0], "\\SteamApps\\");
		lstrcatA(&Path[0], &CFfile[0]);
		if (!FileExists(&Path[0]))
			return false;
		else
			return true;
	}
	else
		return true;
}

void DLDataSize(DLDATASTRUCT &DLDS, char *Prefix, unsigned __int64 &DLSize, bool IgnoreFilesSize)
{
	char **List = RAIN_listFiles(DLDS.remoteGcf, Prefix);
	char **file = List;
	char Path[MAX_PATH];

	bool WasSomeDataTransferred=false;

	while (*file)
	{	
		lstrcpy(&Path[0], Prefix);
		lstrcat(&Path[1], *file);
		
		if (RAIN_isFile(DLDS.remoteGcf, &Path[0]) && DataMismatch(DLDS.remoteGcf, DLDS.localGcf, &Path[0], IgnoreFilesSize))
		{
			unsigned __int64 size = RAIN_size(DLDS.remoteGcf, &Path[0]); //RAIN_effectiveSize(DLDS.remoteGcf, &Path[0]);
			DLSize+=size + (8192 - (size % 8192));		
		}
		else
		if (RAIN_isDirectory(DLDS.remoteGcf, &Path[0]))
		{
			lstrcat(&Path[0], "/");
			DLDataSize(DLDS, &Path[0], DLSize, IgnoreFilesSize);
		}

		file++;
	}
	RAIN_freeList(List);
}

bool DLData(DLDATASTRUCT &DLDS, char *Prefix, GCF updGcf, bool IgnoreFilesSize, MESSAGEID Message)
{
	char** List = RAIN_listFiles(DLDS.remoteGcf, Prefix);
	char **file = List;
	char Path[MAX_PATH];

	bool WasSomeDataTransferred=false;

	while (*file)
	{
		
		lstrcpy(&Path[0], Prefix);
		lstrcat(&Path[1], *file);

		if (RAIN_isFile(DLDS.remoteGcf, &Path[0]) && DataMismatch(DLDS.remoteGcf, DLDS.localGcf, &Path[0], IgnoreFilesSize))
		{
			if (!updGcf)
				RAIN_delete(DLDS.localGcf, &Path[0]);
			else
			{
				if (RAIN_effectiveSize(DLDS.remoteGcf, &Path[0]) == RAIN_effectiveSize(updGcf, &Path[0]))
				{
					WasSomeDataTransferred=true;
					file++;
					continue;
				}
				else
					RAIN_delete(updGcf, &Path[0]);
			}
			while (true)
			{
				WasSomeDataTransferred=copyFile(DLDS, DLDS.remoteGcf,(updGcf ? updGcf : DLDS.localGcf), &Path[0], DLDS.CommonPath, Message);
				if (WasSomeDataTransferred)
					break;

				if (Message == DL)
				{
					//Failed to read. Probably lost connection to server. Trying to reconnect
					DLDS.remoteGcf = RAIN_Reconnect(DLDS, Message);
					if (!DLDS.remoteGcf)
					{
						DLDS.LastErr = lceConnectionFailed;
						break;
					}
				}
			}
		}
		else
		if (RAIN_isDirectory(DLDS.remoteGcf, &Path[0]))
		{
			if (isNcf)
			{
				char Dir[MAX_PATH];
				lstrcpy(&Dir[0], DLDS.CommonPath);
				convert(&Path[0], &Dir[lstrlen(&Dir[0])], true);
				ForceDirectories(&Dir[0]);
			}
			lstrcat(&Path[0], "/");
			WasSomeDataTransferred |= DLData(DLDS, &Path[0], updGcf, IgnoreFilesSize, Message);
			if (DLDS.LastErr == lceConnectionFailed)
				break;
		}

		file++;
	}
	RAIN_freeList(List);
	return WasSomeDataTransferred;
}

void die(GCF localGcf, GCF remoteGcf)
{
	if (remoteGcf)
	{
		RAIN_stopClient();
		RAIN_unmountGcf(remoteGcf);
	}
	if (localGcf)
		RAIN_unmountGcf(localGcf);
}

void convert(char *unix_style_path, char *win_style_path, bool toWin)
{		
	if (toWin)
	{
		int len=strlen(unix_style_path);
		for(int i=0; i<len; i++)
		{
			if (unix_style_path[i] == '/')
				win_style_path[i] = '\\';
			else
				win_style_path[i] = unix_style_path[i];
		}
		win_style_path[len]=0;
	}
	else
	{
		int len=strlen(win_style_path);
		for(int i=0; i<len; i++)
		{
			if (win_style_path[i] == '\\')
				unix_style_path[i] = '/';
			else
				unix_style_path[i] = win_style_path[i];
		}
		unix_style_path[len]=0;
	}
}

bool DirectoryExists(const char *Name)
{
	int Code;
	Code = GetFileAttributes(Name);
	return ((Code != -1) && ((FILE_ATTRIBUTE_DIRECTORY & Code) != 0));
}

void ExtractFilePath(const char *path, char *lpPath)
{
	char *pos=0;
	int i, len;

	len = strlen(path);
	strncpy(lpPath,path, MAX_PATH);

	for(i=len-1;i>0;i--)
	{
		if (lpPath[i]=='\\' && i!=len-1)
			break;
		else
			lpPath[i]=0;
	}
}

void ExtractFileName(const char *path, char *FN)
{
	char *pos=0;
	int i, len = lstrlen(path);
	
	for(i=len;i>0;i--)
	{
		if (path[i]=='\\')
			break;
	}

	lstrcpy(FN,&path[i+1]);
}

bool ForceDirectories(char *Dir)
{
	bool Result = (strlen(Dir) > 3);
	char Parent[MAX_PATH];
	int LenDir;

	if (!Result)
		return 0;

	if (DirectoryExists(Dir))
		return Result; // avoid 'xyz:\' problem.
	
	ExtractFilePath(Dir, &Parent[0]);

	if (	strlen(Dir) < 3 || 
			DirectoryExists(Dir) ||
			lstrcmp(&Parent[0],Dir)==0
		)
		return Result; 

	LenDir = lstrlen(&Dir[0]);
	if (Dir[LenDir-1]=='\\')
		Dir[LenDir-1] = 0;
	Result =  ForceDirectories(&Parent[0]);
	Result &= (bool)CreateDirectoryA(&Dir[0],0);// ? 1 : 0, printf("CreateDirectory(%s, 0) failed with error code %d\n", &Dir[0], GetLastError()) ) ;
	return ( Result );
}

void PrintUsage()
{
	printf(	"cfUpdater v3 - the GCF and NCF updater by $t@t!c_V()1D\n"
			"Based on the Rain.dll by steamCooker\n"
			"\n"
			"cfUpdater <action> <path to GCF or NCF> [NCF path] <additional commands>\n"
			"Actions and additional commands:\n"
			"0                 Create manifest file (similar to CFT .archive file).\n"
			"1 <manifest>      Create an update file.\n"
			"2 <update file>   Apply an update.\n"
			"3 <DLparameters>  Download GCF updates from Steam server.\n"
			"4 <DLparameters>  Download GCF updates from CFT content server.\n"
			"5 <Fix errors>    Validates the cache file. <Fix errors> must be 0 or 1.\n"
			"\n"
			"  <DLparameters> are:\n"
			"  <IP:Port>          IP and port of either Steam master server\n"
			"                     or the CFT content server.\n"
			"  <Version>          Version of the GCF to request from server.\n"
			"  <CreateUpdFile>    Must be 1 or 0.\n"
			"                     Create an .update.gcf file and apply the updates.\n"
			"  <AppID>            AppID of the GCF to download from scratch|update.\n"
			"                     Can be \"NULL\".\n"
			"                     NOTE: if <Version> is -1 and AppID is specified then the\n"
			"                     latest version is acquired from server.\n" 
			"  [Login]            User account login (for Action==3).\n"
			"  [Password]         Password for the account (for Action==3).\n"
			"\n"
			"Example cfUpdater usage:\n"
			"cfUpdater.exe 0 \"C:\\counter-strike.gcf\"\n"
			"cfUpdater.exe 0 \"C:\\audiosurf content public.ncf\" \"C:\\SteamApps\\common\\audiosurf\"\n"
			"cfUpdater.exe 1 \"C:\\counter-strike.gcf\" \"C:\\counter-strike.0.CFUmanifest\"\n"
			"cfUpdater.exe 2 \"C:\\counter-strike.gcf\" \"C:\\counter-strike.0_to_2.update.gcf\"\n"
			"cfUpdater.exe 2 \"C:\\audiosurf content public.ncf\" \"C:\\SteamApps\\common\\audiosurf\" \"C:\\audiosurf content public.10_to_33.update.gcf\"\n"
			"cfUpdater.exe 3 \"C:\\counter-strike.gcf\" \"gds1.steampowered.com:27030\" 2 0 NULL MyAccount MyPasswordIsblablabla\n"
			"cfUpdater.exe 3 \"C:\\counter-strike.gcf\" \"gds1.steampowered.com:27030\" -1 1 11 MyAccount MyPasswordIsblablabla\n"
			"cfUpdater.exe 4 \"C:\\counter-strike.gcf\" \"123.321.213.312:27030\" -1 0 11\n"
			"cfUpdater.exe 4 \"C:\\counter-strike.gcf\" \"123.321.213.312:27030\" 2 1 11\n"
			"cfUpdater.exe 5 \"C:\\counter-strike.gcf\" 1\n"
			);

	getchar();
}