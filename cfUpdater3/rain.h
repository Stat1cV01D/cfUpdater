/*
Rain API - by SteamCooker
*/

//#define RAIN_DEBUG
#define RAIN_PUBLIC_BUILD

#define RAIN_FS
#define RAIN_CHAT
#define RAIN_VIRTUAL_FRIEND
#define RAIN_IMPORT
#define RAIN_CM

#ifdef RAIN_DEBUG
#define RAIN_API
#else
#ifdef RAIN_EXPORTS
#define RAIN_API __declspec(dllexport)
#else
#define RAIN_API __declspec(dllimport)
#endif
#endif

RAIN_API void RAIN_enableLogs(const char * logFile);
// write logs in the specified log file

RAIN_API void RAIN_disableLogs();
// disable logs

RAIN_API void RAIN_printLog(const char * entry);
// add an entry to the log 

RAIN_API void RAIN_stats(unsigned int * pointersCount, unsigned int * objectsCount, unsigned int * memoryUsed, unsigned int * threadsCount);
// for release version : always return -1 for pointersCount, objectCount, memoryUsed 

extern "C" typedef void (__cdecl *RAIN_onError)(const char * error);
RAIN_API void RAIN_setErrorHandler(RAIN_onError errorHandler);
// register an handler to be called in case of errors

#ifdef RAIN_FS

typedef int GCF;			// a mounted file system (gcf, ncf, or from content server)
typedef int GCF_MANIFEST;	// file system version descritor (manifest+checksums)
typedef int GCF_ENTRY;		// a file in the mounted file system
typedef int GCF_TOOL;		// a file system tool
typedef int CDR;			// a loaded CDR
typedef int SESSIONID;		// a session idenfifier used for different purposes

typedef struct
{
	unsigned int appId;
	bool isNcf;
	char name[200];
	char installDirName[200];
	unsigned int currentVersion;
} APPInfo;

typedef enum 
{
	GcfEntryStatus_noContent     = 0, // no content expected
	GcfEntryStatus_empty         = 1, // empty, to be filled
	GcfEntryStatus_incomplete    = 2, // not 100% filled
	GcfEntryStatus_complete      = 3, // 100% filled
	GcfEntryStatus_tooBig        = 4, // bigger than the manifest
	GcfEntryStatus_notInManifest = 5, // not descriped in the manifest
	GcfEntryStatus_noManifest    = 6, // no manifest available
	GcfEntryStatus_error		 = 7 
} GcfEntryStatus;

typedef struct
{
	unsigned int appId;
	bool isOptional;
} APPFileSystem;

#define APPFileSystems_MAX	20

typedef struct
{
	unsigned int count;
	APPFileSystem fileSystems[APPFileSystems_MAX];
} APPFileSystems;

typedef struct
{
	unsigned int count;
	unsigned int * checksums;
} Checksums;

RAIN_API APPInfo * RAIN_getAppInfoFromZippedGcf(char * zipPath);
// reads APPInfo from a zipped gcf

RAIN_API GCF RAIN_mountGcf(char * gcfPath, bool safeMode=true);
// mount a gcf file system. disable safemode to improve performance on writing

RAIN_API GCF RAIN_mountNcf(char * ncfPath, char * commonFolder, bool safeMode=true);
// mount a ncf file system. disable safemode to improve performance on writing

RAIN_API GCF RAIN_mountGcfOnCFTContentServer(char * contentServer,unsigned int cfId, unsigned int cfVersion);
// mount a gcf or ncf file system from a CF Toolbox anonymous content server.

RAIN_API GCF RAIN_newEmptyGcf(char * gcfPath,GCF toCopy, bool safeMode=true);
// create a copy of a gcf file system. disable safemode to improve performance on writing

RAIN_API GCF RAIN_newSizedEmptyGcf(char * gcfPath,GCF toCopy, unsigned long long storageSize=-1L, bool safeMode=true);
// mount a gcf file system but allocates only the specified storageSize (due to clustering, actual available size might be less). disable safemode to improve performance on writing

RAIN_API bool RAIN_unmountGcf(GCF gcf);
// unmount a mounted file system

RAIN_API GCF_MANIFEST RAIN_getManifest(GCF gcf);
// retrieve a mounted file system manifest (manifest+checksums)

RAIN_API bool RAIN_updateManifest(GCF gcf, GCF_MANIFEST newManifest);
// updates a mounted file system version (content is not updated)

RAIN_API GCF RAIN_newSizedEmptyGcfFromManifest(char * gcfPath,GCF_MANIFEST manifest, unsigned long long storageSize=-1L, bool safeMode=true);
// creates a new gcf file using a manifest

RAIN_API GCF RAIN_newEmptyNcfFromManifest(char * ncfPath, char * commonFolder, GCF_MANIFEST manifest, bool safeMode=true);
// creates a new ncf file using a manifest

RAIN_API void RAIN_writeManifest(const char * filename, GCF_MANIFEST manifest);
// saves a manifest on disk

RAIN_API GCF_MANIFEST RAIN_readManifest(const char * filename);
// load a manifest from disk

RAIN_API void RAIN_freeManifest(GCF_MANIFEST manifest); 
// free a manifest. to be used only for manifests read from a file or from content server mounted gcf/ncf

RAIN_API unsigned int RAIN_getAppId(GCF gcf);
// returns the mounted file system app id

RAIN_API unsigned int RAIN_getAppVersion(GCF gcf);
// returns the mounted file system version

RAIN_API char ** RAIN_listFiles(GCF gcf, char * path);
// list the files in a mounted file system directory path

RAIN_API void RAIN_freeList(char ** list);
// free the list returned by RAIN_listFiles

RAIN_API bool RAIN_isFile(GCF gcf, char * path);
// check if the specified path is a file in the mounted file system

RAIN_API bool RAIN_isDirectory(GCF gcf, char * path);
// check if the specified path is a directory in the mounted file system

RAIN_API GcfEntryStatus RAIN_getStatus(GCF gcf, char * path);
// returns the status of the specified path is a file in the mounted file system

RAIN_API unsigned __int64 RAIN_size(GCF gcf, char * path);
// returns the size of the file as described in the manifest of the mounted file system

RAIN_API unsigned __int64 RAIN_effectiveSize(GCF gcf, char * path);
// returns the size of real content of the file in the mounted file system

RAIN_API bool RAIN_delete(GCF gcf, char * path);
// delete a file in the mounted file system

RAIN_API Checksums * RAIN_getChecksums(GCF gcf, char * path);
// returns the file checksums from the mounted file system

RAIN_API void RAIN_freeChecksums(Checksums * checksums);
// free the checksums returned by RAIN_getChecksums

RAIN_API GCF_ENTRY RAIN_openToRead(GCF gcf, char * path);
// opens a file for reading

RAIN_API GCF_ENTRY RAIN_openToWrite(GCF gcf, char * path, bool append=true);
// opens a file for writing

RAIN_API bool RAIN_close(GCF_ENTRY file);
// close the opened file

RAIN_API unsigned int RAIN_read(GCF_ENTRY file, unsigned char * data, unsigned int size);
// read data from a file

RAIN_API unsigned int RAIN_write(GCF_ENTRY file, unsigned char * data, unsigned int size);
// write data to a file

RAIN_API CDR RAIN_downloadCDR(const char * gds);
// download the current CDR from the steam network

RAIN_API void RAIN_freeCDR(CDR cdr);
// free a downloaded/loaded CDR

RAIN_API void RAIN_save(CDR cdr, const char * filename);
// save a CDR to disk

RAIN_API CDR RAIN_load(const char * filename);
// load a CDR from disk

RAIN_API APPInfo * RAIN_getAppInfo(CDR cdr, unsigned int appId);
// returns APPInfo from the CDR

RAIN_API void RAIN_freeAppInfo(APPInfo * appInfo);
// free APPInfo returned by RAIN_getAppInfo

RAIN_API APPFileSystems * RAIN_getAppFileSystems(CDR cdr, unsigned int appId);
// returns the APPFileSystems associated to an appId

RAIN_API void RAIN_freeAppFileSystems(APPFileSystems * fs);
// free the APPFileSystems returned by RAIN_getAppFileSystems

extern "C" typedef void (__cdecl *RAIN_GcfToolHandler)(GCF_TOOL tool, float progress, bool isFinished, int result, const char * resultMessage);

RAIN_API GCF_TOOL RAIN_validate(GCF gcf, RAIN_GcfToolHandler handler, bool validateContent=true, bool fixErrors=false);
// start a gcf validation

RAIN_API bool RAIN_isRunningTool(GCF_TOOL tool);
// check tool status

RAIN_API void RAIN_closeTool(GCF_TOOL tool);
// close the tool

#endif

#ifdef RAIN_IMPORT

enum ImportResult
{
	ImportResult_OK,
		ImportResult_ERROR
};

enum ImportOperation
{
	ImportOperation_notFound,
		ImportOperation_notComplete,
		ImportOperation_error,
		ImportOperation_copying,
		ImportOperation_copied
};
extern "C" typedef void (__cdecl *RAIN_onImportingFile)(ImportOperation operation,const char * filename, const char * info, float progress);
//the importer will call this on each file

RAIN_API ImportResult RAIN_createGcf(char * outputGcfPath, char * miniGcfPath, char * contentFolder,RAIN_onImportingFile callback);
// copy content from a folder into a gcf file. 

#endif

#ifdef RAIN_CHAT

extern "C" typedef const char * (__cdecl *RAIN_OnChatMessageHandler)(unsigned __int64 steamGlobalId, const char * message);
// the handler returns a string to be sent back to the message sender, or 0 if no message is to be sent
// the string will not be freed

enum ChatStatus
{
	ChatStatus_offline=0,
		ChatStatus_online=1,
		ChatStatus_busy=2,
		ChatStatus_away=3
};

RAIN_API bool RAIN_startChatListener(const char * login, const char * password, RAIN_OnChatMessageHandler onMessageHandler);
// connect to steam and the friends chat. onMessageHandler will be called when a message is sent to this user

RAIN_API bool RAIN_isChatListening();
// check if the client is still online

RAIN_API bool RAIN_setChatStatus(ChatStatus status);
// change the friends chat status

RAIN_API void RAIN_stopChatListener();
// disconnect from steam

#endif		

#ifdef RAIN_VIRTUAL_FRIEND

extern "C" typedef void (__cdecl *RAIN_OnCommandHandler)(SESSIONID sessionId, const char * message, bool isEmote);
// the handler is called on messages sent to the virtual friend


RAIN_API bool RAIN_installVirtualFriend(const char * nickname, RAIN_OnCommandHandler callback);
// call only once in a Steam clientapp to install the virtual friend to your friends list
RAIN_API void RAIN_uninstallVirtualFriend();
// call when the clientapp is closing to uninstall the virtual friend

RAIN_API void RAIN_sendChatMessageToClient(SESSIONID sessionId,const char * message, bool isEmote=false);
RAIN_API void RAIN_sendChatMessageToFriend(SESSIONID sessionId,const char * friendName, const char * message, bool isEmote=false);
// to be called from the RAIN_OnCommandHandler only in order to send chat messages 

#endif		

#ifdef RAIN_CM

RAIN_API bool RAIN_startClient(const char * login, const char * password, const char * steamGuardKey=0);
// connect to steam (required for the functions below)

RAIN_API void RAIN_stopClient();
// disconnect from steam 

RAIN_API GCF RAIN_mountGcfOnContentServer(char * gdsServer, unsigned int cfId, unsigned int cfVersion);
// mount a gcf or ncf file system from a the steam network

enum AppInfoType
{
	AppInfoType_all=1,
	AppInfoType_common=2,
	AppInfoType_first=2,
	AppInfoType_extended=3,
	AppInfoType_config=4,
	AppInfoType_stats=5,
	AppInfoType_install=6,
	AppInfoType_startup=7,
	AppInfoType_VAC=8,
	AppInfoType_DRM=9,
	AppInfoType_UFS=10,
	AppInfoType_OGG=11,
	AppInfoType_items=12,
	AppInfoType_policies=13,
	AppInfoType_platform=14
};  

typedef int REGISTRY;	

extern "C" typedef void (__cdecl * RAIN_OnAppInfoUpdates)(int currentversion,bool allAppInfoUpdated, unsigned int * updatedAppIds, unsigned int updatedAppIdsCount);
    
RAIN_API bool RAIN_requestUpdatedAppInfo(unsigned int fromVersion,RAIN_OnAppInfoUpdates handler);
 
extern "C" typedef void (__cdecl * RAIN_OnAppInfo)(unsigned int appId, unsigned int currentVersion, AppInfoType type, REGISTRY appInfo);
 
RAIN_API bool RAIN_requestAppInfo(unsigned int appId, RAIN_OnAppInfo handler);
 
enum RegistryFormat
{
	RegistryFormat_ini,
	RegistryFormat_vdf,
	RegistryFormat_xml,
	RegistryFormat_kvs,
};

RAIN_API char * RAIN_serializeRegistry(REGISTRY registry, RegistryFormat serializationFormat, unsigned int * serializedSize);
// result pointer must be freed by caller, a trailing 0x00 is added (but not counted) so the result can be used as a string
RAIN_API void RAIN_freeSerializedRegistry(char * registry);

enum LobbyEntry
{
	LobbyEntry_error=-3,
	LobbyEntry_noResult=-1,
	LobbyEntry_first=0,
	LobbyEntry_last=1,
	LobbyEntry_other=2
};

extern "C" typedef void (__cdecl * RAIN_OnLobbyHandler)(int appId, LobbyEntry entry, __int64 lobbyId, int membersCount, int membersMax, const char * metadata);
 
enum LobbyCompare
{
	LobbyCompare_equalOrLessThan=-2,
		LobbyCompare_lessThan=-1,
		LobbyCompare_equal=0,
		LobbyCompare_greaterThan=1,
		LobbyCompare_equalOrGreaterThan=2,
		LobbyCompare_notEqual=3
};

RAIN_API void RAIN_addLobbyFilterStringCompare(char * key, LobbyCompare comparison, char * value);
RAIN_API void RAIN_addLobbyFilterNumericalCompare(char * key, LobbyCompare comparison, char * value);
RAIN_API void RAIN_addLobbyFilterNear(char * key, char * value);
RAIN_API void RAIN_addLobbyFilterFreeSlots(char * value);
RAIN_API void RAIN_addLobbyFilterMaxResults(char * value);
// filters must be applied before requesting lobbies. requesting lobbies reset the filters

RAIN_API bool RAIN_requestLobbies(int appId, RAIN_OnLobbyHandler onLobbyHandler);
// searchfor lobbies using the previously added filters

RAIN_API unsigned long long RAIN_createLobby(int appId, int membersMax, bool isPublic=true);
// create a new lobby

RAIN_API bool RAIN_setLobbyMetadata(__int64 lobbyId, char * metadata);
// configure lobby

#endif
