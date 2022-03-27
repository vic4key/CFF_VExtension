#pragma once

#ifndef QWORD
#define QWORD unsigned __int64
#endif // !QWORD

#pragma pack(push, 8)
typedef struct _EXTEVENTSDATA
{
	DWORD cbSize;

	HINSTANCE hInstance;
	DWORD DlgID;
	LRESULT (CALLBACK *DlgProc)(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

} EXTEVENTSDATA, *PEXTEVENTSDATA;
#pragma pack(pop)

//
// Common APIs
//

#define m_eaGetObjectAddress			1
#define m_eaGetObjectSize					2
#define m_eaObjectChanged					3
#define m_eaReplaceObject					4
#define m_eaFreeObject						5
#define m_eaGetObjectName					6
#define m_eaSaveObject						7


//
// Syntax: VOID *eaGetObjectAddress(HWND hWnd);
//
// Description: retrieves the current object base address
//

typedef VOID * (__cdecl *d_eaGetObjectAddress)(HWND hWnd);

//
// Syntax: UINT eaGetObjectSize(HWND hWnd);
//
// Description: retrieves the current object size
//

typedef UINT (__cdecl *d_eaGetObjectSize)(HWND hWnd);

//
// Syntax: VOID eaObjectChanged(HWND hWnd);
//
// Description: tells the CFF Explorer that the current object has been modified
//

typedef VOID (__cdecl *d_eaObjectChanged)(HWND hWnd);

//
// Syntax: BOOL eaReplaceObject(HWND hWnd, VOID *pNewObject, UINT uNewSize);
//
// Description: replaces the current object
//

typedef BOOL (__cdecl *d_eaReplaceObject)(HWND hWnd, VOID *pNewObject, UINT uNewSize);

//
// Syntax: VOID eaFreeObject(VOID *pObject);
//
// Description: frees memory that has been allocated by others CFF APIs
//

typedef VOID (__cdecl *d_eaFreeObject)(VOID *pObject);

//
// Syntax: BOOL eaGetObjectName(HWND hWnd, WCHAR *ObjName);
//
// Description: gets the name of the current object
//

typedef BOOL (__cdecl *d_eaGetObjectName)(HWND hWnd, WCHAR *ObjName);

//
// Syntax: BOOL eaSaveObject(HWND hWnd);
//
// Description: saves the current object. All changes will be permanent
//

typedef BOOL (__cdecl *d_eaSaveObject)(HWND hWnd);

//
// PE APIs
//

#define m_eaPEAPIBase						1000

#define m_eaIsPE64							(m_eaPEAPIBase)

#define m_eaIsRvaValid						(m_eaPEAPIBase + 10)
#define m_eaRvaToOffset						(m_eaPEAPIBase + 11)
#define m_eaVaToRva							(m_eaPEAPIBase + 12)
#define m_eaVaToRva64						(m_eaPEAPIBase + 13)
#define m_eaVaToOffset						(m_eaPEAPIBase + 14)
#define m_eaVaToOffset64					(m_eaPEAPIBase + 15)			
#define m_eaOffsetToRva						(m_eaPEAPIBase + 16)

#define m_eaSectionFromRva					(m_eaPEAPIBase + 30)

#define m_eaEntryPoint						(m_eaPEAPIBase + 40)
#define m_eaGetDataDirectory				(m_eaPEAPIBase + 41)

#define m_eaAddSectionHeader				(m_eaPEAPIBase + 100)
#define m_eaAddSection						(m_eaPEAPIBase + 101)
#define m_eaDeleteSectionHeader				(m_eaPEAPIBase + 102)
#define m_eaDeleteSection					(m_eaPEAPIBase + 103)
#define m_eaAddDataToLastSection			(m_eaPEAPIBase + 104)

#define m_eaRebuildImageSize				(m_eaPEAPIBase + 300)
#define m_eaRebuildPEHeader					(m_eaPEAPIBase + 301)
#define m_eaRealignPE						(m_eaPEAPIBase + 302)
#define m_eaRemoveRelocSection				(m_eaPEAPIBase + 303)
#define m_eaBindImports						(m_eaPEAPIBase + 304)
#define m_eaRemoveStrongNameSignature		(m_eaPEAPIBase + 305)
#define m_eaSetImageBase					(m_eaPEAPIBase + 306)
#define m_eaAfterDumpHeaderFix				(m_eaPEAPIBase + 307)


//
// Syntax: BOOL eaIsPE64(VOID *pPE, UINT uSize);
//
// Description: returns TRUE if the PE is 64bit
//

typedef BOOL (__cdecl *d_eaIsPE64)(VOID *pPE, UINT uSize);

//
// Syntax: BOOL eaIsRvaValid(VOID *pPE, UINT uSize, DWORD Rva);
//
// Description: returns TRUE if the given RVA is valid
//

typedef BOOL (__cdecl *d_eaIsRvaValid)(VOID *pPE, UINT uSize, DWORD Rva);

//
// Syntax: DWORD eaRvaToOffset(VOID *pPE, UINT uSize, DWORD Rva)
//
// Description: converts a RVA to an Offset
//

typedef DWORD (__cdecl *d_eaRvaToOffset)(VOID *pPE, UINT uSize, DWORD Rva);

//
// Syntax: DWORD eaVaToRva(VOID *pPE, UINT uSize, DWORD Va);
//
// Description: converts a 32 bit virtual address to a RVA
//

typedef DWORD (__cdecl *d_eaVaToRva)(VOID *pPE, UINT uSize, DWORD Va);

//
// Syntax: DWORD eaVaToRva64(VOID *pPE, UINT uSize, QWORD Va);
//
// Description: converts a 64 bit virtual address to a RVA
//

typedef DWORD (__cdecl *d_eaVaToRva64)(VOID *pPE, UINT uSize, QWORD Va);

//
// Syntax: DWORD eaVaToOffset(VOID *pPE, UINT uSize, DWORD Va);
//
// Description: converts a 32 bit virtual address to an offset
//

typedef DWORD (__cdecl *d_eaVaToOffset)(VOID *pPE, UINT uSize, DWORD Va);

//
// Syntax: DWORD eaVaToOffset64(VOID *pPE, UINT uSize, QWORD Va);
//
// Description: converts a 64 bit virtual address to an offset
//

typedef DWORD (__cdecl *d_eaVaToOffset64)(VOID *pPE, UINT uSize, QWORD Va);

//
// Syntax: DWORD eaOffsetToRva(VOID *pPE, UINT uSize, DWORD Offset);
//
// Description: converts an offset to a RVA
//

typedef DWORD (__cdecl *d_eaOffsetToRva)(VOID *pPE, UINT uSize, DWORD Offset);

//
// Syntax: WORD eaSectionFromRva(VOID *pPE, UINT uSize, DWORD Rva);
//
// Description: converts a RVA to a section entry
//

typedef WORD (__cdecl *d_eaSectionFromRva)(VOID *pPE, UINT uSize, DWORD Rva);

//
// Syntax: VOID *eaEntryPoint(VOID *pPE, UINT uSize);
//
// Description: returns the PE entry point address
//

typedef VOID * (__cdecl *d_eaEntryPoint)(VOID *pPE, UINT uSize);

//
// Syntax: BOOL eaGetDataDirectory(VOID *pPE, UINT uSize, UINT uDataDir, IMAGE_DATA_DIRECTORY *pDataDir);
//
// Description: returns a given data directory
//

typedef BOOL (__cdecl *d_eaGetDataDirectory)(VOID *pPE, UINT uSize, UINT uDataDir, IMAGE_DATA_DIRECTORY *pDataDir);


//
// Syntax: VOID *eaAddSectionHeader(VOID *pPE, UINT uSize, UINT *uNewSize);
//
// Description: adds a section header a the end of the section table
//

typedef VOID * (__cdecl *d_eaAddSectionHeader)(VOID *pPE, UINT uSize, UINT *uNewSize);

//
// Syntax: VOID *eaAddSection(VOID *pPE, UINT uSize, UINT *uNewSize, BYTE *pData, DWORD DataSize, 
//							  CHAR *pName, DWORD Characteristics);
//
// Description: adds a section
//

typedef VOID * (__cdecl *d_eaAddSection)(VOID *pPE, UINT uSize, UINT *uNewSize, BYTE *pData, DWORD DataSize, 
									     CHAR *pName, DWORD Characteristics);

//
// Syntax: VOID *eaDeleteSectionHeader(VOID *pPE, UINT uSize, UINT *uNewSize, WORD nEntry);
//
// Description: deletes a section header
//

typedef VOID * (__cdecl *d_eaDeleteSectionHeader)(VOID *pPE, UINT uSize, UINT *uNewSize, WORD nEntry);

//
// Syntax: VOID *eaDeleteSection(VOID *pPE, UINT uSize, UINT *uNewSize, WORD nEntry);
//
// Description: deletes a section with all its data
//

typedef VOID * (__cdecl *d_eaDeleteSection)(VOID *pPE, UINT uSize, UINT *uNewSize, WORD nEntry);

//
// Syntax: VOID *eaAddDataToLastSection(VOID *pPE, UINT uSize, UINT *uNewSize, BYTE *pData, DWORD DataSize);
//
// Description: adds data to the last section
//

typedef VOID * (__cdecl *d_eaAddDataToLastSection)(VOID *pPE, UINT uSize, UINT *uNewSize, BYTE *pData, DWORD DataSize);

//
// Syntax: VOID *eaRebuildImageSize(VOID *pPE, UINT uSize, UINT *uNewSize);
//
// Description: rebuilds the PE image size
//

typedef VOID * (__cdecl *d_eaRebuildImageSize)(VOID *pPE, UINT uSize, UINT *uNewSize);

//
// Syntax: VOID *eaRebuildPEHeader(VOID *pPE, UINT uSize, UINT *uNewSize);
//
// Description: rebuilds the PE header
//

typedef VOID * (__cdecl *d_eaRebuildPEHeader)(VOID *pPE, UINT uSize, UINT *uNewSize);

//
// Syntax: VOID *eaRealignPE(VOID *pPE, UINT uSize, UINT *uNewSize, DWORD Alignment);
//
// Description: realigns the sections file alignment to a given value
//

typedef VOID * (__cdecl *d_eaRealignPE)(VOID *pPE, UINT uSize, UINT *uNewSize, DWORD Alignment);

//
// Syntax: VOID *eaRemoveRelocSection(VOID *pPE, UINT uSize, UINT *uNewSize);
//
// Description: removes the reloc section if possible
//

typedef VOID * (__cdecl *d_eaRemoveRelocSection)(VOID *pPE, UINT uSize, UINT *uNewSize);

//
// Syntax: VOID *eaBindImports(VOID *pPE, UINT uSize, UINT *uNewSize);
//
// Description: binds the imports
//

typedef VOID * (__cdecl *d_eaBindImports)(VOID *pPE, UINT uSize, UINT *uNewSize);

//
// Syntax: VOID *eaRemoveStrongNameSignature(VOID *pPE, UINT uSize, UINT *uNewSize);
//
// Description: removes .NET strong name signature
//

typedef VOID * (__cdecl *d_eaRemoveStrongNameSignature)(VOID *pPE, UINT uSize, UINT *uNewSize);

//
// Syntax: VOID *eaSetImageBase(VOID *pPE, UINT uSize, UINT *uNewSize, QWORD NewImageBase);
//
// Description: sets a new image base for the PE
//

typedef VOID * (__cdecl *d_eaSetImageBase)(VOID *pPE, UINT uSize, UINT *uNewSize, QWORD NewImageBase);

//
// Syntax: VOID *eaAfterDumpHeaderFix(VOID *pPE, UINT uSize, UINT *uNewSize);
//
// Description: fixes PE headers after a memory dump
//

typedef VOID *(__cdecl *d_eaAfterDumpHeaderFix)(VOID *pPE, UINT uSize, UINT *uNewSize);

/**
 * Extension.h
 */

#ifndef EXTINITDATA
typedef struct _EXTINITDATA
{
  VOID(__cdecl* RetrieveExtensionApi)(UINT* ApiMask, VOID* pApi);

} EXTINITDATA, * PEXTINITDATA;
#endif

UINT nCFFApiMask[] =
{
  m_eaGetObjectAddress,
  m_eaGetObjectSize,
  m_eaObjectChanged,
  m_eaReplaceObject,
  m_eaFreeObject,
  m_eaGetObjectName,
  m_eaSaveObject,

  m_eaIsPE64,
  m_eaIsRvaValid,
  m_eaRvaToOffset,
  m_eaVaToRva,
  m_eaVaToRva64,
  m_eaVaToOffset,
  m_eaVaToOffset64,
  m_eaOffsetToRva,
  m_eaSectionFromRva,
  m_eaEntryPoint,
  m_eaGetDataDirectory,
  m_eaAddSectionHeader,
  m_eaAddSection,
  m_eaDeleteSectionHeader,
  m_eaDeleteSection,
  m_eaAddDataToLastSection,
  m_eaRebuildImageSize,
  m_eaRebuildPEHeader,
  m_eaRealignPE,
  m_eaRemoveRelocSection,
  m_eaBindImports,
  m_eaRemoveStrongNameSignature,
  m_eaSetImageBase,
  m_eaAfterDumpHeaderFix,

  NULL
};

typedef struct _CFFAPI
{
  d_eaGetObjectAddress eaGetObjectAddress;
  d_eaGetObjectSize eaGetObjectSize;
  d_eaObjectChanged eaObjectChanged;
  d_eaReplaceObject eaReplaceObject;
  d_eaFreeObject eaFreeObject;
  d_eaGetObjectName eaGetObjectName;
  d_eaSaveObject eaSaveObject;

  d_eaIsPE64 eaIsPE64;
  d_eaIsRvaValid eaIsRvaValid;
  d_eaRvaToOffset eaRvaToOffset;
  d_eaVaToRva eaVaToRva;
  d_eaVaToRva64 eaVaToRva64;
  d_eaVaToOffset eaVaToOffset;
  d_eaVaToOffset64 eaVaToOffset64;
  d_eaOffsetToRva eaOffsetToRva;
  d_eaSectionFromRva eaSectionFromRva;
  d_eaEntryPoint eaEntryPoint;
  d_eaGetDataDirectory eaGetDataDirectory;
  d_eaAddSectionHeader eaAddSectionHeader;
  d_eaAddSection eaAddSection;
  d_eaDeleteSectionHeader eaDeleteSectionHeader;
  d_eaDeleteSection eaDeleteSection;
  d_eaAddDataToLastSection eaAddDataToLastSection;
  d_eaRebuildImageSize eaRebuildImageSize;
  d_eaRebuildPEHeader eaRebuildPEHeader;
  d_eaRealignPE eaRealignPE;
  d_eaRemoveRelocSection eaRemoveRelocSection;
  d_eaBindImports eaBindImports;
  d_eaRemoveStrongNameSignature eaRemoveStrongNameSignature;
  d_eaSetImageBase eaSetImageBase;
  d_eaAfterDumpHeaderFix eaAfterDumpHeaderFix;

} CFFAPI, * PCFFAPI;

static CFFAPI CFF;

BOOL CFF_Initialize(EXTINITDATA* pExtInitData)
{
  if (pExtInitData == nullptr)
  {
    return FALSE;
  }

  pExtInitData->RetrieveExtensionApi(nCFFApiMask, &CFF);

  VOID** ppApi = (VOID**)&CFF;

  for (UINT x = 0; ; x++)
  {
    if (nCFFApiMask[x] == 0) break;

    if (ppApi[x] == NULL) return FALSE;
  }

  return TRUE;
}
