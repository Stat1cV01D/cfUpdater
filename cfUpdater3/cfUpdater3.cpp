#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "Additional.h"
#include <Windows.h>
#include <time.h>

#define LoggerName "SV_CFUpdater"

void onError(const char * error)
{
	printf("\nAn error was raised : %s\n",error);
}

typedef struct
{
	unsigned int appId;
	char installDirName[MAX_PATH];
	unsigned int currentVersion;
	unsigned int GCFVersion;
} GCFINFO;

int main(int argc, TCHAR* argv[])
{
	const bool safemount = false;

	if (argc<2)
	{
		PrintUsage();
		return 1;
	}	

	RAIN_setErrorHandler(onError);

	DLDATASTRUCT DLDS;
	memset(&DLDS, 0, sizeof(DLDS));
	char CFfile[MAX_PATH];
	char _CommonPath[MAX_PATH];
	ACTION Action = (ACTION)atoi(argv[1]);
	lstrcpyA(&CFfile[0], argv[2]);

	if (!GetFile(&CFfile[0], MAX_PATH) && Action<DLFromSteamServers)
	{
		printf("Error opening %s\n", &CFfile[0]);
		return 1;
	}
	isNcf = (lstrcmp(&CFfile[ strlen(&CFfile[0]) -4 ], ".ncf")==0);
	int iNcfArg = (int)isNcf;
	if (isNcf)
		GetFullPathName(argv[3], MAX_PATH, &_CommonPath[0], 0);

	DLDS.gdsServer = NULL; // gdsServer here is not needed
	DLDS.ToVersion = -1; // ToVersion here is not needed
	DLDS.AppID = -1; // AppID here is not needed
	DLDS.LastErr = lceOK;
	DLDS.CommonPath = _CommonPath;
	DLDS.Action = Action;
	DLDS.localGcf = (isNcf ? RAIN_mountNcf(&CFfile[0], DLDS.CommonPath, safemount) : RAIN_mountGcf(&CFfile[0], safemount));

	//RAIN_unmountGcf(localGcf);

	switch (Action)
	{
	case CreateArchive:
		{
			// Here localGcf is a handle of GCF cache to create archive from (old GCF)
			char ManifestFileName[MAX_PATH], tmp[MAX_PATH];
					
			GetBaseGCFName(CFfile, &tmp[0], 1);
			sprintf(&ManifestFileName[0], "%s.%d.CFUmanifest", tmp, RAIN_getAppVersion(DLDS.localGcf));
			
			DeleteFileA(&ManifestFileName[0]);
			//updGcf=RAIN_newSizedEmptyGcf(&ManifestFileName[0], localGcf, 0, true);
			//RAIN_unmountGcf(updGcf);

			GCF_MANIFEST manifest = RAIN_getManifest(DLDS.localGcf);	
			//int File = _open(ManifestFileName, _O_BINARY | _O_CREAT | _O_TRUNC);
			//_close(File);
			RAIN_writeManifest(&ManifestFileName[0], manifest);
			//RAIN_freeManifest(manifest);
			RAIN_unmountGcf(DLDS.localGcf);
		}
		break;
	case CreatePatch:
		{
			// NCF: 0x1C-0x1f - size of the NCF|archive file
			//
			// Here localGcf is a handle of a newer GCF cache
			// So now we'll change the src and dest as we'll copy from the local GCF
			DLDS.remoteGcf = DLDS.localGcf;

			if (argc<3+iNcfArg)
			{
				die(DLDS.localGcf, 0);
				PrintUsage();
				return 1;
			}
			char szArchiveFile[MAX_PATH]; 
			char TmpFilename[MAX_PATH];
			GetFullPathName(argv[3+iNcfArg], MAX_PATH, &szArchiveFile[0], NULL);

			GCF_MANIFEST manifest = RAIN_readManifest(szArchiveFile);
			GetParentDir(szArchiveFile, &TmpFilename[0]);
			GetTempFileName(&TmpFilename[0], "CFU", 0, &TmpFilename[0]);
			DLDS.localGcf = (isNcf ? 
								RAIN_newEmptyNcfFromManifest(&TmpFilename[0], DLDS.CommonPath, manifest) : 
								RAIN_newSizedEmptyGcfFromManifest(&TmpFilename[0], manifest, 1));

			char UpdGCF[MAX_PATH];
			char tmp[MAX_PATH];
			//char Path[MAX_PATH];
			//GetCurrentDirectory(MAX_PATH, &Path[0]);
			GetBaseGCFName(szArchiveFile, &tmp[0], 1);

			sprintf(&UpdGCF[0], "%s_to_%d.update.gcf", tmp, RAIN_getAppVersion(DLDS.remoteGcf));
			
			printf("Calculating data to be %s\n", messages2[COPY]);
			DLDS.TotalSize = 0;
			DLDataSize(DLDS, "/", DLDS.TotalSize, true); // Ignore file size as manifest has no files in it
			GCF updGcf = RAIN_newSizedEmptyGcf(&UpdGCF[0], DLDS.remoteGcf, DLDS.TotalSize, false);

			printf("%s data:\n", messages[COPY]);
			DLDS.LastErr = lceOK;

			bool DataTransfer = DLData(DLDS, "/", updGcf, true, COPY);
			printf("%s data finished\n", messages[COPY]);

			RAIN_unmountGcf(updGcf);
			RAIN_unmountGcf(DLDS.localGcf);
			RAIN_freeManifest(manifest);
			RAIN_unmountGcf(DLDS.remoteGcf);
			DeleteFile(&TmpFilename[0]);
		}
		break;

	case ApplyPatch:
		{
			// Here localGcf is a handle of an older GCF cache
			if (argc<3+iNcfArg)
			{
				PrintUsage();
				return 1;
			}
			char *szUpdateFile = argv[3+iNcfArg];
			GCF updGcf = RAIN_mountGcf(szUpdateFile, safemount);
			
			GCF_MANIFEST updManifest = RAIN_getManifest(updGcf);
			if (!RAIN_updateManifest(DLDS.localGcf, updManifest))
			{
				die(DLDS.localGcf, updGcf);
				printf("Error opening applying changes to %s\n", &CFfile[0]);
				return 1;
			}
			RAIN_unmountGcf(updGcf);
			RAIN_unmountGcf(DLDS.localGcf);
			
			FILE *f = fopen(szUpdateFile, "r");
			if (!f)
			{
				die(0, 0);
				printf("Error opening update file\n");
				return 1;
			}
			_fseeki64(f, 0, SEEK_END);  
			DLDS.TotalSize = _ftelli64(f);
			fclose(f);

			updGcf = RAIN_mountGcf(szUpdateFile, safemount);
			DLDS.localGcf = (isNcf ? RAIN_mountNcf(&CFfile[0], DLDS.CommonPath, safemount) : RAIN_mountGcf(&CFfile[0], safemount));
			
			DLDS.remoteGcf = updGcf;

			bool DataTransfer = DLData(DLDS, "/", 0, false, COPY);
			printf("%s data finished\n", messages[COPY]);
			
			RAIN_unmountGcf(updGcf);
			RAIN_unmountGcf(DLDS.localGcf);
		}
		break;
	case DLFromSteamServers:
			if (argc<8)
			{
				die(DLDS.localGcf, 0);
				PrintUsage();
				return 1;
			}
			else
			{
				if (!RAIN_startClient(argv[7+iNcfArg], argv[8+iNcfArg]))
				{
					printf("Error logging in with the current login/password.\n"
							"Either there's something with Steam servers or incorrect login/password\n");
					die(DLDS.localGcf, 0);
					return 2;
				}
			}
	case DLFromCFTContentServer:
		{
			// Here localGcf is a handle of an older GCF cache
			DLDS.gdsServer = argv[3+iNcfArg];
			DLDS.ToVersion = atoi(argv[4+iNcfArg]);
			bool CreateUpdateFile = (bool)(atoi(argv[5+iNcfArg]) != 0);
			
			if (IsAppID(argv[6+iNcfArg]))
			{
				DLDS.AppID = atoi(argv[6+iNcfArg]);

				CDR Cdr = LoadCDR();
				APPInfo *info = RAIN_getAppInfo(Cdr, DLDS.AppID);
				//sprintf(&CFfile[0], ".\\%s", info->installDirName);
				if (DLDS.ToVersion == -1)
					DLDS.ToVersion = info->currentVersion;
				if (!strchr(&CFfile[0], ':')) 
				{
					if (strncmp(&CFfile[0], ".\\", 2)!=0 || strncmp(&CFfile[0], "..\\", 3)!=0)
					{
						lstrcpy(&CFfile[2], &CFfile[0]);
						CFfile[0] = '.';
						CFfile[1] = '\\';
					}
					GetFullPathName(&CFfile[0], MAX_PATH, &CFfile[0], 0);
				}
				RAIN_freeCDR(Cdr);

				DLDS.remoteGcf = RAIN_Reconnect(DLDS); 

				if (!FileExists(CFfile) && !DLDS.localGcf)
				{
					if (isNcf)
					{
						ForceDirectories(DLDS.CommonPath);
						GCF_MANIFEST remoteManifest = RAIN_getManifest(DLDS.remoteGcf);
						DLDS.localGcf = RAIN_newEmptyNcfFromManifest(&CFfile[0], DLDS.CommonPath, remoteManifest, safemount);
						RAIN_freeManifest(remoteManifest);
					}
					else
						DLDS.localGcf = RAIN_newEmptyGcf(&CFfile[0], DLDS.remoteGcf, safemount);
				}
				else if (DLDS.AppID != RAIN_getAppId(DLDS.localGcf))
				{
					die(DLDS.remoteGcf, DLDS.localGcf);
					printf("Error: specified AppID (%d) doesn\'t the one that is of GCF (%d)\n", DLDS.AppID, RAIN_getAppId(DLDS.localGcf));
					return 1;
				}
			}
			else
			{
				DLDS.AppID = RAIN_getAppId(DLDS.localGcf);
				DLDS.remoteGcf = RAIN_Reconnect(DLDS); 
			}
			
			if (!DLDS.remoteGcf)
			{
				die(DLDS.remoteGcf, DLDS.localGcf);
				printf("Error connecting to %s\n", DLDS.gdsServer);
				return 1;
			}
			else
				printf("Connected\n");
			
			int OldVersion = RAIN_getAppVersion(DLDS.localGcf);
			//if (OldVersion==NewVersion)
			//{
			//	die(remoteGcf, localGcf);
			//	printf("Version hasn\'t changed (%d)\n", NewVersion);
			//	return 2;				
			//}
		
			if (CreateUpdateFile && OldVersion!=DLDS.ToVersion)
			{
				printf("Updating new GCF\n");

				char UpdGCF[MAX_PATH];
				char tmp[MAX_PATH];
				GCF updGcf=0;

				GetBaseGCFName(&CFfile[0], &tmp[0], 1);

				sprintf(&UpdGCF[0], "%s.%d_to_%d.update.gcf", tmp, OldVersion, DLDS.ToVersion);
			
				if (FileExists(&UpdGCF[0]))
				{
					updGcf=RAIN_mountGcf(&UpdGCF[0], safemount);
					if (RAIN_getAppVersion(updGcf) != DLDS.ToVersion)
					{
						RAIN_unmountGcf(updGcf);
						DLDS.TotalSize = 0;
						DLDataSize(DLDS, "/", DLDS.TotalSize);
						updGcf=RAIN_newSizedEmptyGcf(&UpdGCF[0], DLDS.remoteGcf, DLDS.TotalSize, safemount);
					}
				}
				else
				{
					DLDS.TotalSize = 0;
					DLDataSize(DLDS, "/", DLDS.TotalSize);
					updGcf=RAIN_newSizedEmptyGcf(&UpdGCF[0], DLDS.remoteGcf, DLDS.TotalSize, safemount);
				}

				if (!updGcf)
				{
					die(DLDS.remoteGcf, DLDS.localGcf);
					printf("Error creating temporary update file\n");
					return 1;
				}
				bool real_isNcf = isNcf;

				isNcf = false; // for the function to download data into .update.gcf
				
				DLData(DLDS, "/", updGcf);
				DLDS.ProcessedSize = 0;
				isNcf = real_isNcf;

				printf("Downloading data finished\n");
				//Now make the .update.gcf the source for applying updates to the main GCF
				RAIN_unmountGcf(DLDS.remoteGcf);
				DLDS.remoteGcf = updGcf;
				isNcf = real_isNcf; // revert the var back
			}

			GCF_MANIFEST updManifest = RAIN_getManifest(DLDS.remoteGcf);
			if (!RAIN_updateManifest(DLDS.localGcf, updManifest))
			{
				die(DLDS.remoteGcf, DLDS.localGcf);
				printf("Error opening applying changes to %s\n", &CFfile[0]);
				return 1;
			}
			if (!CreateUpdateFile)
				RAIN_freeManifest(updManifest); // remoteGcf - is a server GCF
			
			//time_t Time1, Time2, Time3;
			//time(&Time1);
			RAIN_unmountGcf(DLDS.localGcf);
			//time(&Time2);
			//printf("Unmounting GCF: %.1f sec\n", _difftime64(Time2, Time1));
			DLDS.localGcf = (isNcf ? RAIN_mountNcf(&CFfile[0], DLDS.CommonPath, safemount) : RAIN_mountGcf(&CFfile[0], safemount));
			//time(&Time3);
			//printf("Mounting GCF back: %.1f sec\n", _difftime64(Time3, Time2));
				
			DLData(DLDS, "/", 0, false, (CreateUpdateFile ? COPY : DL ));

			RAIN_unmountGcf(DLDS.remoteGcf);
			RAIN_unmountGcf(DLDS.localGcf);
		}
		break;
	}
	return 0;
}
