#pragma once

#include <windows.h>
#include <atlstr.h>//CString
#include <atlcoll.h>//CAtlArray
#include <atlfile.h>//CAtlFile
#include <atlutil.h>//AtlGetErrorDescription
#include <atlapp.h>//WTL
#include <atlctrls.h>//for atlctrlx.h
#include <atlctrlx.h>//WTL::CWaitCursor
#include "tree.h"

#define MAX_FILELENGTH  0xFFFFFFF0
#define MAX_ONMEM 1024 * 1024
#define MAX_MAPSIZE 1024 * 1024 * 64


typedef enum {	UNDO_INS, UNDO_OVR, UNDO_DEL } UndoMode;
typedef enum {	CHUNK_FILE, /*CHUNK_UNDO,*/ CHUNK_MEM } DataChunkType;
typedef enum {	DC_UNKNOWN, DC_FD, DC_FF, DC_DF, DC_DD, DC_DONE } DataChunkSavingType;
typedef struct _TAMADataBuf
{
	LPBYTE pData;
	DWORD nRefCount;
} TAMADataBuf;
typedef struct _TAMADataChunk
{
	DataChunkType dataType;
	union
	{
		TAMADataBuf *dataMem;
		DWORD dataFileAddr;
	};
	DWORD dwSize;
	DWORD dwSkipOffset;
	DataChunkSavingType savingType;//for Save()
} TAMADataChunk;
typedef struct _TAMAUndoRedo
{
	UndoMode mode;
	DWORD dwStart;
	
	TAMADataChunk **dataNext;
	DWORD nDataNext;
	TAMADataChunk **dataPrev;
	DWORD nDataPrev;
	
	BOOL bHidden;
} TAMAUndoRedo;

typedef struct _TAMAFILECHUNK
{
	RB_ENTRY(_TAMAFILECHUNK) linkage;
	union 
	{
		DWORD dwEnd; //Sort-Key
		DWORD key;
	};
	DWORD dwStart;
	TAMADataChunk* dataChunk;
//	DWORD dwSkipOffset;
	
	BOOL bSaved; // for Save
} TAMAFILECHUNK;
static int cmpTAMAFILECHUNK(TAMAFILECHUNK *c1, TAMAFILECHUNK *c2)
{
  DWORD dw1 = c1->dwEnd;
  DWORD dw2 = c2->dwEnd;
  if(dw1==dw2)return 0;
  return (dw1>dw2)?1:-1;
}
RB_HEAD(_TAMAFILECHUNK_HEAD, _TAMAFILECHUNK);
RB_PROTOTYPE(_TAMAFILECHUNK_HEAD, _TAMAFILECHUNK, linkage, cmpTAMAFILECHUNK);
RB_GENERATE(_TAMAFILECHUNK_HEAD, _TAMAFILECHUNK, linkage, cmpTAMAFILECHUNK);


typedef enum {	OF_NOREF, OF_FD, OF_FF } OldFileType;
typedef struct _TAMAOLDFILECHUNK
{
	RB_ENTRY(_TAMAOLDFILECHUNK) linkage;
	union 
	{
		DWORD dwEnd; //Sort-Key
		DWORD key;
	};
	DWORD dwStart;
	OldFileType type;
	union
	{
		DWORD dwNewFileAddr;
		LPBYTE pMem;
	};
} TAMAOLDFILECHUNK;
static int cmpTAMAOLDFILECHUNK(TAMAOLDFILECHUNK *c1, TAMAOLDFILECHUNK *c2)
{
  DWORD dw1 = c1->dwEnd;
  DWORD dw2 = c2->dwEnd;
  if(dw1==dw2)return 0;
  return (dw1>dw2)?1:-1;
}
RB_HEAD(_TAMAOLDFILECHUNK_HEAD, _TAMAOLDFILECHUNK);
RB_PROTOTYPE(_TAMAOLDFILECHUNK_HEAD, _TAMAOLDFILECHUNK, linkage, cmpTAMAOLDFILECHUNK);
RB_GENERATE(_TAMAOLDFILECHUNK_HEAD, _TAMAOLDFILECHUNK, linkage, cmpTAMAOLDFILECHUNK);



class CSuperFileCon
{
public:

	CSuperFileCon(void)
	{
		RB_INIT(&m_filemapHead);
	}
	~CSuperFileCon(void)
	{
	}

	void FatalError()
	{
		exit(-1);
	}

// Open/Overwrite/Save to another file
	BOOL Open(LPCTSTR lpszPathName)
  {
		CWaitCursor wait;
    BOOL bReadOnly = FALSE;
    CAtlFile file;
    if((FAILED(file.Create(lpszPathName, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING))))  // Open (+RW)
    {
      if (FAILED(file.Create(lpszPathName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING))) //Retry open (Read-Only)
      {
        LastErrorMessageBox();
        return FALSE; //Failed open
      }
      bReadOnly = TRUE;
    }
    DWORD dwFileSize = _GetFileLengthLimit4G(&file);
		if (dwFileSize != 0)
      _FileMap_InsertFile(0, m_dwTotal, 0);

		_DeleteContents();
    m_dwTotalSavedFile = m_dwTotal = dwFileSize;
    m_bReadOnly = bReadOnly;
    m_file = file;
		m_filePath = lpszPathName;
		m_bModified = FALSE;

		return TRUE;
	}
	void _FileMap_DestroyAll()
	{
		_FileMap_Del(0, m_dwTotal);
		ATLASSERT(__FileMap_LowMin()==NULL);
	}
	void _UndoRedo_DestroyAll()
	{
		_UndoRedo_RemoveRange(0, m_undo.GetCount());
		ATLASSERT(m_undo.GetCount()==0);
	}
	void _ClearSavedFlags()
	{
		TAMAFILECHUNK *pChunk;
		RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
		{
			ATLASSERT(pChunk->dataChunk);
			pChunk->bSaved = FALSE;
			if(pChunk->dataChunk->dataType == CHUNK_FILE && pChunk->dwStart == _TAMAFILECHUNK_GetRealStartAddr(pChunk) /*�T�C�Y���m�F�����ق��������H*/)
				pChunk->bSaved = TRUE; //file change nothing
		}
	}
	void _ClearDataChunkSavingFlag()
	{
		TAMAFILECHUNK *pChunk;
		RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
		{
			ATLASSERT(pChunk->dataChunk);
			pChunk->dataChunk->savingType = DC_UNKNOWN;
		}
		size_t nUndo = m_undo.GetCount();
		for(size_t i=0; i < nUndo; i++)
		{
			TAMAUndoRedo *undo = m_undo[i];
			ATLASSERT(undo);
			if(undo->dataNext)
			{
				DWORD nDataNext = undo->nDataNext;
				TAMADataChunk **dataNext = undo->dataNext;
				for(DWORD i=0; i<nDataNext; i++)
					dataNext[i]->savingType = DC_UNKNOWN;
			}
			if(undo->dataPrev)
			{
				DWORD nDataPrev = undo->nDataPrev;
				TAMADataChunk **dataPrev = undo->dataPrev;
				for(DWORD i=0; i<nDataPrev; i++)
					dataPrev[i]->savingType = DC_UNKNOWN;
			}
		}
	}
	BOOL _SetFileSize(DWORD newSize)
	{
		m_file.SetSize(newSize);
	}
	BOOL _ExtendFileSize()
	{
		if(m_dwTotal <= m_dwTotalSavedFile)return TRUE;
		_SetFileSize(m_dwTotal);
		TAMAFILECHUNK *fileChunk = RB_MAX(_TAMAFILECHUNK_HEAD, &m_filemapHead);
		while(fileChunk && fileChunk->dwEnd >= m_dwTotalSavedFile)
		{
			if(!fileChunk->bSaved)
			{
				switch(fileChunk->dataChunk->dataType)
				{
				case CHUNK_FILE:
					{
						if(!_TAMAFILECHUNK_ShiftFileChunk(fileChunk))return FALSE;
					}
				case CHUNK_MEM:
					{
						if(fileChunk->dwStart < m_dwTotalSavedFile-1)
						{
							DWORD dwShiftStart = m_dwTotalSavedFile-1;
							_FileMap_SplitPoint(dwShiftStart);
							fileChunk = RB_MAX(_TAMAFILECHUNK_HEAD, &m_filemapHead);
						}
						if(!_TAMAFILECHUNK_WriteMemChunk(fileChunk))return FALSE;
					}
				default:
					ATLASSERT(FALSE);
					break;
				}
			}
			fileChunk = RB_PREV(_TAMAFILECHUNK_HEAD, &m_filemapHead, fileChunk);
		}
		return TRUE;
	}
	BOOL _TAMAFILECHUNK_WriteMemChunk(TAMAFILECHUNK *memChunk)
	{
		ATLASSERT(memChunk);
		ATLASSERT(memChunk->dataChunk->dataType == CHUNK_MEM);
		ATLASSERT(memChunk->dataChunk->dataMem->pData);
		m_file.Seek(memChunk->dwStart);
		DWORD dwWriteSize = memChunk->dwEnd - memChunk->dwStart +1;
		return SUCCEEDED(m_file.Write(_TAMAFILECHUNK_GetRealStartPointer(memChunk), dwWriteSize));
	}
	BOOL _TAMAFILECHUNK_isRightShift(TAMAFILECHUNK *fileChunk)
	{
		ATLASSERT(fileChunk->dwStart != _TAMAFILECHUNK_GetRealStartAddr(fileChunk)); //No move
		if(fileChunk->dwStart < _TAMAFILECHUNK_GetRealStartAddr(fileChunk))return FALSE;
		return TRUE;
	}
	BOOL _ShiftAllFF()
	{
		TAMAFILECHUNK *pLastFileChunk = NULL;
		TAMAFILECHUNK *pChunk;
		RB_FOREACH_REVERSE(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
		{
			if(!pChunk->bSaved && pChunk->dataChunk->dataType == CHUNK_FILE)
			{
				pLastFileChunk = pChunk;
				if(_TAMAFILECHUNK_isRightShift(pChunk))
				{
					if(!_ShiftAllFileChunksAfterArg(pChunk))
					{
						ATLASSERT(FALSE);
            FatalError();
						return FALSE;
					}
				}
			}
		}
		if(pLastFileChunk && !pLastFileChunk->bSaved)
		{
			if(!_ShiftAllFileChunksAfterArg(pChunk))
			{
				ATLASSERT(FALSE);
        FatalError();
				return FALSE;
			}
		}
		return TRUE;
	}
	BOOL _WriteAllDF()
	{
		TAMAFILECHUNK *pChunk;
		RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
		{
			if(!pChunk->bSaved)
			{
				ATLASSERT(pChunk->dataChunk->dataType == CHUNK_MEM);
				if(_TAMAFILECHUNK_WriteMemChunk(pChunk)==FALSE)return FALSE;
				pChunk->bSaved = TRUE;
			}
		}
	}
	BOOL _Debug_SearchUnSavedChunk()
	{
		TAMAFILECHUNK *pChunk;
		RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
		{
			if(!pChunk->bSaved)
			{
				return TRUE;
			}
		}
		return FALSE;
	}
	void _UpdateAllDataChunkSavingType(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead)
	{
		_ClearDataChunkSavingFlag();
		
		TAMAFILECHUNK *pChunk;
		RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
		{
			ATLASSERT(pChunk);
			TAMADataChunk *dataChunk = pChunk->dataChunk;
			ATLASSERT(dataChunk);
			if(dataChunk->savingType == DC_UNKNOWN)
			{
				switch(dataChunk->dataType)
				{
				case CHUNK_FILE:
					dataChunk->savingType = DC_FF;
					break;
				case CHUNK_MEM:
					dataChunk->savingType = DC_DF;
					break;
				default:
					ATLASSERT(FALSE);
					break;
				}
			}
		}
		size_t nUndo = m_undo.GetCount();
		for(size_t i=0; i < nUndo; i++)
		{
			TAMAUndoRedo *undo = m_undo[i];
			ATLASSERT(undo);
			_OldFileMap_ConvFD(pOldFilemapHead, &undo->dataNext, &undo->nDataNext);
			_OldFileMap_ConvFD(pOldFilemapHead, &undo->dataPrev, &undo->nDataPrev);
		}
	}
	BOOL _OldFileMap_ConvFD(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, TAMADataChunk ***pDataChunks, DWORD *pNumDataChunks)
	{
		for(DWORD i=0;i<*pNumDataChunks;i++)
		{
			TAMADataChunk *dataChunk = (*pDataChunks)[i];
			ATLASSERT(dataChunk);
			if(dataChunk && dataChunk->savingType == DC_UNKNOWN && dataChunk->dataType==CHUNK_FILE)
			{
				ATLASSERT(dataChunk->dwSize > 0);
				DWORD dwOldFileAddrS = _TAMADataChunk_GetRealStartAddr(dataChunk);
				TAMAOLDFILECHUNK *pOldFileChunkS = _OldFileMap_LookUp(pOldFilemapHead, dwOldFileAddrS);
				DWORD nOldFileChunkS = _TAMAOLDFILECHUNK_GetSize(pOldFileChunkS);
				ATLASSERT(nOldFileChunkS > 0);
				DWORD nRemain = dataChunk->dwSize;
				//if(nOldFileChunkS < dataChunk->dwSize)nRemain = dataChunk->dwSize - nOldFileChunkS;
				//else nRemain=0;
				DWORD iS = i;
				TAMAOLDFILECHUNK *pOldFileChunkNext = pOldFileChunkS;
				ATLASSERT(nRemain > 0);
				while(1)
				{
					TAMADataChunk *newDataChunk = _OldFileMap_Conv2TAMADataChunk(pOldFileChunkNext, &nRemain);
					ATLASSERT(newDataChunk);
					if(iS==i)
					{
						(*pDataChunks)[i] = newDataChunk;
						_TAMADataChunk_Release(dataChunk);
					} else {
						BOOL bRetInsert = _TAMADATACHUNKS_Insert(pDataChunks, pNumDataChunks, i+1, newDataChunk);
						ATLASSERT(bRetInsert);
						ATLASSERT(i<0xFFffFFff);
						i++;
					}
					//nRemain -= nNextChunkSize;
					
					if(nRemain < 0)break;
					
					pOldFileChunkNext = RB_NEXT(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pOldFileChunkNext);
					if(pOldFileChunkNext==NULL)
					{
						ATLASSERT(FALSE);
						return FALSE;
					}
					ATLASSERT(pOldFileChunkNext->type == OF_FD || pOldFileChunkNext->type == OF_FF);
				}
			}
		}
	}
	TAMADataChunk* _OldFileMap_Conv2TAMADataChunk(TAMAOLDFILECHUNK *pOldFileChunk, DWORD *pNRemain)
	{
		TAMADataChunk *dataChunk = NULL;
		if(pOldFileChunk==NULL)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		ATLASSERT(pOldFileChunk->type == OF_FD || pOldFileChunk->type == OF_FF);
		DWORD nChunkSize = _TAMAOLDFILECHUNK_GetSize(pOldFileChunk);
		nChunkSize = min(nChunkSize, *pNRemain);
		switch(pOldFileChunk->type)
		{
		case OF_FD:
		{
			TAMADataBuf *pTAMADataBuf = _TAMADataBuf_CreateAssign(pOldFileChunk->pMem, 0);
			ATLASSERT(pTAMADataBuf);
			dataChunk = _TAMADataChunk_CreateMemAssignTAMADataBuf(nChunkSize, 0, pTAMADataBuf);
			ATLASSERT(dataChunk);
			dataChunk->savingType = DC_DONE;
			break;
		}
		case OF_FF:
			dataChunk = _TAMADataChunk_CreateFileChunk(nChunkSize, pOldFileChunk->dwNewFileAddr);
			ATLASSERT(dataChunk);
			dataChunk->savingType = DC_DONE;
			break;
		default:
			ATLASSERT(FALSE);
			return FALSE;
		}
		*pNRemain -= nChunkSize;
		return dataChunk;
	}
	TAMADataBuf* _TAMADataBuf_CreateAssign(LPBYTE pData, DWORD nRefCount = 0)
	{
		TAMADataBuf *pDataBuf = (TAMADataBuf *)malloc(sizeof(TAMADataBuf));
		if(pDataBuf)
		{
			pDataBuf->pData = pData;
			pDataBuf->nRefCount = nRefCount;
		}
		return pDataBuf;
	}
	TAMADataBuf* _TAMADataBuf_CreateNew(DWORD dwNewAlloc, DWORD nRefCount = 0)
	{
		TAMADataBuf *pDataBuf = (TAMADataBuf *)malloc(sizeof(TAMADataBuf));
		ATLASSERT(pDataBuf);
		if(!pDataBuf)return NULL;
		
		pDataBuf->pData = (LPBYTE)malloc(sizeof(dwNewAlloc));
		ATLASSERT(pDataBuf->pData);
		if(!pDataBuf->pData)
		{
			free(pDataBuf);
			return NULL;
		}
		pDataBuf->nRefCount = nRefCount;
		return pDataBuf;
	}
	DWORD _TAMAOLDFILECHUNK_GetSize(TAMAOLDFILECHUNK *oldFileChunk)
	{
		ATLASSERT(oldFileChunk);
		return oldFileChunk->dwEnd - oldFileChunk->dwStart + 1;
	}
	BOOL _TAMADATACHUNKS_Insert(TAMADataChunk ***pDataChunks, DWORD *pNumDataChunks, DWORD nInsertIndex, TAMADataChunk *pInsertDataChunk)
	{
		ATLASSERT(*pNumDataChunks!=0xffFFffFF);
		ATLASSERT(nInsertIndex<=*pNumDataChunks);
		*pNumDataChunks++;
		*pDataChunks = (TAMADataChunk **)realloc(*pDataChunks, *pNumDataChunks);
		if(*pDataChunks==NULL)
		{
			*pNumDataChunks=0;
			return FALSE;
		}
		TAMADataChunk **pMoveStart = (*pDataChunks)+nInsertIndex;
		size_t nCopy = *pNumDataChunks - nInsertIndex - 1;
		if(nCopy>0)memmove(pMoveStart+1, pMoveStart, nCopy);
		return TRUE;
	}
	BOOL _ProccessAllChunks()
	{
		_ClearSavedFlags();
		struct _TAMAOLDFILECHUNK_HEAD oldFilemapHead;
		_OldFileMap_Make(&oldFilemapHead, &m_file);
		_UpdateAllDataChunkSavingType(&oldFilemapHead);
		_OldFileMap_FreeAll(&oldFilemapHead, FALSE);
		if(!_ExtendFileSize()
      || !_ShiftAllFF()
      || !_WriteAllDF() )
    {
			ATLASSERT(FALSE);
			return FALSE;
		}
#ifdef DEBUG
		ATLASSERT(!_Debug_SearchUnSavedChunk());
#endif
		//_UndoRedo_CreateRefSrcFileDataChunk();
	}
	BOOL _ShiftAllFileChunksAfterArg(TAMAFILECHUNK *fileChunk)
	{
		while(fileChunk) {
			if(!fileChunk->bSaved && fileChunk->dataChunk->dataType == CHUNK_FILE)
			{
				if(!_TAMAFILECHUNK_ShiftFileChunk(fileChunk))
				{
					ATLASSERT(FALSE);
					return FALSE;
				}
			}
			fileChunk = RB_NEXT(_TAMAFILECHUNK_HEAD, &m_filemapHead, fileChunk);
		}
		return TRUE;
	}
	BOOL _TAMAFILECHUNK_ShiftFileChunk(TAMAFILECHUNK *fileChunk)
	{
		ATLASSERT(fileChunk);
		ATLASSERT(fileChunk->bSaved==FALSE);
		ATLASSERT(fileChunk->dataChunk->dataType == CHUNK_FILE);
		BOOL bRet;
		if(_TAMAFILECHUNK_isRightShift(fileChunk))
		{
			bRet = _TAMAFILECHUNK_ShiftFileChunkR(fileChunk);
		} else {
			bRet = _TAMAFILECHUNK_ShiftFileChunkL(fileChunk);
		}
		if(!bRet)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		fileChunk->bSaved = TRUE;
		return bRet;
	}

	DWORD _TAMAFILECHUNK_GetRealStartAddr(TAMAFILECHUNK* fileChunk)
	{
		ATLASSERT(fileChunk->dataChunk->dataType==CHUNK_FILE);
		return _TAMADataChunk_GetRealStartAddr(fileChunk->dataChunk);
	}
	DWORD _TAMADataChunk_GetRealStartAddr(TAMADataChunk* fileDataChunk)
	{
		ATLASSERT(fileDataChunk->dataType==CHUNK_FILE);
		return fileDataChunk->dwSkipOffset + fileDataChunk->dataFileAddr;
	}

	LPBYTE _TAMAFILECHUNK_GetRealStartPointer(TAMAFILECHUNK* memChunk)
	{
		ATLASSERT(memChunk->dataChunk->dataType==CHUNK_MEM);
		return _TAMADataChunk_GetRealStartPointer(memChunk->dataChunk);
	}
	LPBYTE _TAMADataChunk_GetRealStartPointer(TAMADataChunk* memDataChunk)
	{
		ATLASSERT(memDataChunk->dataType==CHUNK_MEM);
		return memDataChunk->dwSkipOffset + memDataChunk->dataMem->pData;
	}

	BOOL Save() //�㏑���ۑ�
	{
		CWaitCursor wait;

//		if(IsFileMapping())
//		{
//			//pFile = m_pFileMapping;
//
//			BOOL bResult = (m_pMapStart) ? ::FlushViewOfFile(m_pMapStart, m_dwMapSize) : ::FlushViewOfFile(m_pData, m_dwTotal);
//			if(!bResult) {
//				LastErrorMessageBox();
//				return FALSE;
//			}
//		} else {
//			if (FAILED(m_file.Create(m_filePath, GENERIC_READ | GENERIC_WRITE, 0, CREATE_ALWAYS)))
//			{
//				LastErrorMessageBox();
//				return FALSE;
//			}
//
//			if(		FAILED(m_file.Write(m_pData, m_dwTotal))
//				/*	|| FAILED(m_file.Flush())*/)
//			{
//				LastErrorMessageBox();
//				m_file.Close();
//				return FALSE;
//			}
//			m_file.Close();
//		}
//		//m_dwUndoSaved = m_dwUndo;		// ###1.54
//		//TouchDoc();
//
//		m_bModified = FALSE;//�����܂���
//
//		return TRUE;
	}
//	BOOL SaveAs(LPCTSTR lpszPathName) //���O��t���ĕۑ�
//	{
//		CWaitCursor wait;
//
//		if(IsFileMapping())
//		{
//			//pFile = m_pFileMapping;
//
//			BOOL bResult = (m_pMapStart) ? ::FlushViewOfFile(m_pMapStart, m_dwMapSize) : ::FlushViewOfFile(m_pData, m_dwTotal);
//			if(!bResult) {
//				LastErrorMessageBox();
//				return FALSE;
//			}
//		} else {
//			CAtlFile newFile;
//			if (FAILED(newFile.Create(lpszPathName, GENERIC_READ | GENERIC_WRITE, 0, CREATE_ALWAYS)))
//			{
//				LastErrorMessageBox();
//				return FALSE;
//			}
//
//			if(FAILED(newFile.Write(m_pData, m_dwTotal))
//				/*	|| FAILED(m_file.Flush())*/)
//			{
//				LastErrorMessageBox();
//				newFile.Close();
//				return FALSE;
//			}
//			newFile.Close();
//		}
//		//m_dwUndoSaved = m_dwUndo;		// ###1.54
//		//TouchDoc();
//
//		m_bModified = FALSE;
//
//		return TRUE;
//	}
	void Close()
	{
		_DeleteContents();
		m_file.Close();
	}

	DWORD _TAMAFILECHUNK_GetRemain(TAMAFILECHUNK *pFileChunk, DWORD dwStartOffset)
	{
		if(pFileChunk->dwStart <= dwStartOffset && dwStartOffset <= pFileChunk->dwEnd) return pFileChunk->dwEnd - dwStartOffset + 1;
		return 0; //Error
	}

	DWORD _TAMAFILECHUNK_Read(LPBYTE dst, TAMAFILECHUNK *pSrcFileChunk, DWORD dwStartOffset, DWORD dwMaxRead)
	{
		DWORD dwCanRead = _TAMAFILECHUNK_GetRemain(pSrcFileChunk, dwStartOffset);
		DWORD dwRemain = min(dwCanRead, dwMaxRead);
    ATLASSERT(dwStartOffset >= pSrcFileChunk->dwStart);
		DWORD dwShift = dwStartOffset - pSrcFileChunk->dwStart;

		TAMADataChunk *dataChunk = pSrcFileChunk->dataChunk;

		switch(dataChunk->dataType)
		{
		case CHUNK_FILE:
			{
				DWORD dwFileStart = _TAMAFILECHUNK_GetRealStartAddr(pSrcFileChunk) + dwShift;
        if(SUCCEEDED(m_file.Read(dst, dwRemain)))return dwRemain;
        else return 0;
			}
		case CHUNK_MEM:
			{
				void *pSrcStart = _TAMAFILECHUNK_GetRealStartPointer(pSrcFileChunk) + dwShift;
				memcpy(dst, pSrcStart, dwRemain);
				return dwRemain;
			}
		}
		return 0;
	}

// Read/OverWrite
	BOOL Read(void *dst1, DWORD dwStart, DWORD dwSize)
	{
		TAMAFILECHUNK *pReadChunk;
		pReadChunk = _FileMap_LookUp(dwStart);
		if(!pReadChunk)return FALSE;

		LPBYTE lpDst1 = (LPBYTE)dst1;
		DWORD dwRemain = dwSize;
    DWORD dwReadedAll = 0;

		while(dwRemain > 0 && pReadChunk)
		{
      if(!pReadChunk)
      {
        ATLASSERT(FALSE); //Corrupt filemap
        return FALSE;
      }
			DWORD dwReaded = _TAMAFILECHUNK_Read(lpDst1, pReadChunk, dwStart+dwReadedAll, dwRemain);
			ATLASSERT(dwRemain >= dwReaded);
			ATLASSERT(dwReaded);
			if(dwReaded==0)return FALSE;
			dwRemain -= dwReaded;
      dwReadedAll += dwReaded;
      lpDst1 += dwReaded;
			pReadChunk = RB_NEXT(_TAMAFILECHUNK_HEAD, &m_filemapHead, pReadChunk);
		}

		return (dwRemain==0);
	}
	BOOL ReadTwin(void *dst1, void *dst2, DWORD dwStart, DWORD dwSize)
	{
		BOOL bRet = Read(dst1, dwStart, dwSize);
		if(bRet)memcpy(dst2, dst1, dwSize);
		return bRet;
	}
	BOOL Write(DWORD dwStart, void *srcDataDetached/*Write()���s�����ꍇ�͌Ăяo�����ŊJ�����邱��*/, DWORD dwSize/*, BOOL bNotCopy = FALSE*/)
	{
		DWORD dwNewTotal = dwStart + dwSize;
		if(dwNewTotal < dwStart)
		{
			ATLASSERT(FALSE);
			return FALSE;//dwSize too big (overflow)
		}
		BOOL bGlow = FALSE;
		DWORD dwGlow = dwNewTotal - m_dwTotal;
		if(dwNewTotal > m_dwTotal)bGlow = TRUE;
		size_t nPrevSize = bGlow? dwSize-dwGlow : dwSize;
		
		TAMAUndoRedo *pNewUndo = _TAMAUndoRedo_Create(UNDO_OVR, dwStart, NULL, 0, NULL, 0);//, nPrevSize, dwSize);
		if(!pNewUndo)
		{
			ATLASSERT(FALSE);
			return FALSE; //memory full
		}
		TAMADataChunk **ppNextDataChunks = _TAMADATACHUNKS_CreateWith1MemDataChunk(dwSize, 0, (LPBYTE)srcDataDetached);
		if(!ppNextDataChunks)
		{
			ATLASSERT(FALSE);
			free(pNewUndo);
			return FALSE;
		}
		pNewUndo->dataNext = ppNextDataChunks;
		pNewUndo->nDataNext = 1;
		
		TAMADataChunk **pPrevDataChunks = NULL;
		DWORD nPrevDataChunks = 0;
		BOOL bRet = _FileMap_CreateTAMADataChunks(dwStart, nPrevSize, &pPrevDataChunks, &nPrevDataChunks);
		if(!bRet)
		{
			ATLASSERT(FALSE);
			_TAMADataChunks_Release(ppNextDataChunks, 1, FALSE/*bFreeRawPointer*/);
			free(pNewUndo);
			return FALSE;
		}
		pNewUndo->dataPrev = pPrevDataChunks;
		pNewUndo->nDataPrev = nPrevDataChunks;
		
		_PreNewUndo();
		m_undo.Add(pNewUndo);
		
		return _FileMap_OverWriteTAMADataChunks(dwStart, pNewUndo->dataNext, pNewUndo->nDataNext, 0);
	}
	TAMAUndoRedo* _TAMAUndoRedo_Create(UndoMode mode, DWORD dwStart, TAMADataChunk **dataPrev=NULL, DWORD nDataPrev=0, TAMADataChunk **dataNext=NULL, DWORD nDataNext=0)
	{
		TAMAUndoRedo *pNewUndo = NULL;
		pNewUndo = (TAMAUndoRedo *)malloc(sizeof(TAMAUndoRedo));
		if(!pNewUndo) goto err_TAMAUndoRedoCreate;
		pNewUndo->dwStart = dwStart;
		pNewUndo->dataPrev = dataPrev;
		pNewUndo->nDataPrev = nDataPrev;
		pNewUndo->dataNext = dataNext;
		pNewUndo->nDataNext = nDataNext;
		pNewUndo->mode = mode;
		pNewUndo->bHidden = FALSE;
		return pNewUndo;
		
	  err_TAMAUndoRedoCreate:
		ATLASSERT(FALSE);
		if(pNewUndo)
		{
			//if(pNewUndo->dataNext)_TAMADataChunks_Release(pNewUndo->dataNext, pNewUndo->nDataNext);
			//if(pNewUndo->dataPrev)_TAMADataChunks_Release(pNewUndo->dataPrev, pNewUndo->nDataPrev);
			free(pNewUndo);
		}
		return NULL;
	}
	DWORD inline _GetEndOffset(DWORD dwStart, DWORD dwSize) { ATLASSERT(dwSize>0); return dwStart+dwSize-1; };
	
	TAMADataChunk** _TAMADATACHUNKS_CreateWith1MemDataChunk(DWORD dwSize, DWORD dwSkipOffset, LPBYTE srcDataDetached)
	{
		TAMADataChunk *pDataChunk = _TAMADataChunk_CreateMemAssignRawPointer(dwSize, 0, srcDataDetached);
		if(!pDataChunk)
		{
			return NULL;
		}
		TAMADataChunk **ppDataChunks = (TAMADataChunk **)malloc(sizeof(TAMADataChunk *)*1);
		if(!ppDataChunks)
		{
			_TAMADataChunk_Release(pDataChunk, FALSE/*bFreeRawPointer*/);
			return NULL;
		}
		ppDataChunks[0] = pDataChunk;
		return ppDataChunks;
	}

// Insert/Delete
	BOOL Insert(DWORD dwInsStart, LPBYTE srcDataDetached/*Insert()���s�����ꍇ�͌Ăяo�����ŊJ�����邱��*/, DWORD dwInsSize)
	{
		TAMAUndoRedo *pNewUndo = _TAMAUndoRedo_Create(UNDO_INS, dwInsStart, NULL, 0, NULL, 0);
		if(!pNewUndo)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		TAMADataChunk **ppNextDataChunks = _TAMADATACHUNKS_CreateWith1MemDataChunk(dwInsSize, 0, srcDataDetached);
		if(!ppNextDataChunks)
		{
			ATLASSERT(FALSE);
			free(pNewUndo);
			return FALSE;
		}
		pNewUndo->dataNext = ppNextDataChunks;
		pNewUndo->nDataNext = 1;
		_PreNewUndo();
		m_undo.Add(pNewUndo);

		BOOL bRetInsertMem = _FileMap_InsertTAMADataChunks(dwInsStart, pNewUndo->dataNext, 1, dwInsSize);
		ATLASSERT(bRetInsertMem);
		return TRUE;
	}
	BOOL Delete(DWORD dwDelStart, DWORD dwDelSize)
	{
		TAMAUndoRedo *pNewUndo = _TAMAUndoRedo_Create(UNDO_DEL, dwDelStart, NULL, 0, NULL, 0);
		if(!pNewUndo)
		{
			ATLASSERT(FALSE);
			return FALSE; //memory full
		}
		TAMADataChunk **pDataChunks = NULL;
		DWORD nDataChunks = 0;
		BOOL bRet = _FileMap_CreateTAMADataChunks(dwDelStart, dwDelSize, &pDataChunks, &nDataChunks);
		if(!bRet)
		{
			ATLASSERT(FALSE);
			free(pNewUndo);
			return FALSE;
		}
		pNewUndo->dataPrev = pDataChunks;
		pNewUndo->nDataPrev = nDataChunks;
		_PreNewUndo();
		m_undo.Add(pNewUndo);

		BOOL bRetFileMapDel = _FileMap_Del(dwDelStart, dwDelSize);
		ATLASSERT(bRetFileMapDel);
		return TRUE;
	}
	BOOL _PreNewUndo()
	{
		BOOL nRet=	ClearRedo();
		nRet	&=	_MakeRestoreHiddenNodeFromDiskFile();
		return nRet;
	}
	BOOL _MakeRestoreHiddenNodeFromDiskFile()
	{
		if(m_redoIndex<m_savedIndex)
		{
			_RemoveNeedlessHiddenNode(m_redoIndex, m_savedIndex);//�����ō����Ă�H
			_HideNodes(m_redoIndex, m_savedIndex);
			//compacthiddennode (too fast) + change m_savedIndex
			for(size_t i = m_savedIndex-1; i>=m_redoIndex; i--)
			{
				TAMAUndoRedo *reverseUndo = _TAMAUndoRedo_ReverseCopy(m_undo[i]);
				if(reverseUndo==NULL)
				{
					ATLASSERT(FALSE);
					return FALSE;
				}
				m_undo.Add(reverseUndo);
			}
			//compacthiddennode (too safe
		}
		return TRUE;
	}
	BOOL _CompactHiddenNode()
	{
		return TRUE;
	}
	inline void _RemoveNeedlessHiddenNode(size_t nStartIndex = 0, size_t nEndIndex = 0)
	{
		if(nEndIndex==0)nEndIndex = m_undo.GetCount()-1;
		ATLASSERT(nStartIndex<=nEndIndex);

		BOOL bHiddenStarted = FALSE;
		size_t nHiddenStarted;
		BOOL bFoundSavedIndex = FALSE;
		for(size_t i=nStartIndex; i<=nEndIndex; i++)
		{
			TAMAUndoRedo *undo = m_undo[i];
			if(!bHiddenStarted)
			{
				if(undo->bHidden)
				{
					bHiddenStarted = TRUE;
					nHiddenStarted = i;
					if(i==m_savedIndex)bFoundSavedIndex = TRUE;
				}
			} else {
				if(!undo->bHidden)
				{
					if(!bFoundSavedIndex)
					{
						size_t delSize = i-nHiddenStarted;
						_UndoRedo_RemoveRange(nHiddenStarted, delSize);
						i-=delSize;
					}
					bFoundSavedIndex = FALSE;
					bHiddenStarted = FALSE;
				} else {
					if(i==m_savedIndex)bFoundSavedIndex = TRUE;
				}
			}
		}
	}
	inline void _HideNodes(size_t nStartIndex, size_t nEndIndex)
	{
		ATLASSERT(nStartIndex<=nEndIndex);
		for(size_t i=nStartIndex; i<=nEndIndex; i++)
			m_undo[i]->bHidden = TRUE;
	}
	inline TAMAUndoRedo* _TAMAUndoRedo_ReverseCopy(TAMAUndoRedo *srcUndo)
	{
		TAMAUndoRedo *newUndo = (TAMAUndoRedo*)malloc(sizeof(TAMAUndoRedo));
		if(newUndo==NULL)return NULL;
		switch(srcUndo->mode)
		{
		case UNDO_INS:
			newUndo->mode = UNDO_DEL;
			break;
		case UNDO_DEL:
			newUndo->mode = UNDO_INS;
			break;
		case UNDO_OVR:
			break;
		}
		newUndo->dwStart = srcUndo->dwStart;
		newUndo->bHidden = srcUndo->bHidden;
		newUndo->dataNext = _TAMADATACHUNKS_Copy(srcUndo->dataPrev, srcUndo->nDataPrev);
		ATLASSERT(newUndo->dataNext);
		if(!newUndo->dataNext)
		{
			free(newUndo);
			return NULL;
		}
		newUndo->nDataNext = srcUndo->nDataPrev;
		newUndo->dataPrev = _TAMADATACHUNKS_Copy(srcUndo->dataNext, srcUndo->nDataNext);
		ATLASSERT(newUndo->dataPrev);
		if(!newUndo->dataPrev)
		{
			_TAMADataChunks_Release(newUndo->dataNext, newUndo->nDataNext);
			free(newUndo);
			return NULL;
		}
		newUndo->nDataPrev = srcUndo->nDataPrev;
		
		return newUndo;
	}
	TAMADataChunk** _TAMADATACHUNKS_Copy(TAMADataChunk **dataChunks, DWORD nData)
	{
		TAMADataChunk **pDataChunks = (TAMADataChunk **)malloc(sizeof(TAMADataChunk *)*nData);
		if(pDataChunks==NULL)return NULL;
		for(DWORD i=0; i<nData; i++)
		{
			ATLASSERT(dataChunks[i]);
			pDataChunks[i] = _TAMADataChunk_Copy(dataChunks[i]);
		}
		return pDataChunks;
	}
	TAMADataChunk* _TAMADataChunk_Copy(TAMADataChunk *dataChunk)
	{
		ATLASSERT(dataChunk);
		if(!dataChunk)return NULL;
		TAMADataChunk *newChunk = (TAMADataChunk *)malloc(sizeof(TAMADataChunk));
		if(newChunk==NULL)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		memcpy(newChunk, dataChunk, sizeof(TAMADataChunk));
		switch(dataChunk->dataType)
		{
		  case CHUNK_MEM:
			_TAMADataBuf_IncRef(newChunk->dataMem);
			break;
		  case CHUNK_FILE:
			break;
		  default:
			ATLASSERT(FALSE);
			free(newChunk);
			return NULL;
		}
		return newChunk;
	}
	void _TAMADataChunk_IncRef(TAMADataChunk *dataChunk)
	{
		if(dataChunk->dataType == CHUNK_MEM && dataChunk->dataMem)_TAMADataBuf_IncRef(dataChunk->dataMem);
	}
	void _TAMADataBuf_IncRef(TAMADataBuf *pDataBuf)
	{
		ATLASSERT(pDataBuf->nRefCount<0xFFffFFff);
		pDataBuf->nRefCount++;
	}

// Undo/Redo
	BOOL Undo()
	{
		if(GetUndoCount()==0)return FALSE;
		TAMAUndoRedo *undo = m_undo[--m_redoIndex];
		switch(undo->mode)
		{
		case UNDO_INS:
			ATLASSERT(undo->dataNext);
			ATLASSERT(undo->dataNext[0]);
			_FileMap_Del(undo->dwStart, undo->dataNext[0]->dwSize);
			break;
		case UNDO_DEL:
			ATLASSERT(undo->dataPrev);
			_FileMap_InsertTAMADataChunks(undo->dwStart, undo->dataPrev, undo->nDataPrev);
			break;
		case UNDO_OVR:
			ATLASSERT(undo->dataPrev);
			ATLASSERT(undo->nDataPrev);
			_FileMap_OverWriteTAMADataChunks(undo->dwStart, undo->dataPrev, undo->nDataPrev, 0);
			break;
		default:
			ATLASSERT(FALSE);
			break;
		}
	}
	BOOL Redo()
	{
		if(GetRedoCount()==0)return FALSE;
		TAMAUndoRedo *undo = m_undo[m_redoIndex++];
		switch(undo->mode)
		{
		case UNDO_INS:
			ATLASSERT(undo->dataNext);
			_FileMap_InsertTAMADataChunks(undo->dwStart, undo->dataNext, undo->nDataNext);
			break;
		case UNDO_DEL:
			ATLASSERT(undo->dataPrev);
			ATLASSERT(undo->dataPrev[0]);
			_FileMap_Del(undo->dwStart, undo->dataPrev[0]->dwSize);
			break;
		case UNDO_OVR:
			ATLASSERT(undo->dataNext);
			ATLASSERT(undo->nDataNext);
			_FileMap_OverWriteTAMADataChunks(undo->dwStart, undo->dataNext, undo->nDataNext, 0);
			break;
		default:
			ATLASSERT(FALSE);
			break;
		}
	}
	size_t GetUndoCount(){ return m_redoIndex; }
	size_t GetRedoCount(){ return m_undo.GetCount() - m_redoIndex; }
	size_t GetUndoCountCanRemove(){ return min(m_savedIndex, m_redoIndex); }
	size_t GetRedoCountCanRemove(size_t *pDelIndex = NULL){ size_t delIndex = max(m_savedIndex, m_redoIndex);
															if(pDelIndex)*pDelIndex = delIndex;
															return m_undo.GetCount() - delIndex; }
	BOOL ClearUndoRedoAll()
	{
		BOOL nRet=	ClearRedo();
		nRet	&=	ClearUndo();
		return nRet;
	}
	BOOL ClearRedo()
	{
		size_t delIndex;
		size_t nDelSize = GetRedoCountCanRemove(&delIndex);
		if(nDelSize>0)_UndoRedo_RemoveRange(delIndex, nDelSize);
		return TRUE;
	}
	BOOL ClearUndo()
	{
		size_t nDelSize = GetUndoCountCanRemove();
		if(nDelSize>0)_UndoRedo_RemoveRange(0, nDelSize);
		return TRUE;
	}


// 
	DWORD GetDocSize()
	{
		return m_dwTotal;
	}

  protected:

protected:
	CAtlFile m_file;
	CString m_filePath;
//	LPBYTE	m_pData; //���������̃A�h���X�B�t�@�C�����A�h���X��0�ɓ�����A�h���X�������B�����������I�ȃt�@�C���}�b�s���O�̏ꍇ������B���̏ꍇm_pData����}�b�s���O�̈悪�n�܂��Ă���Ƃ͌���Ȃ��Am_pData�͋[���I�ȃA�h���X�ɂȂ��Ă���
	DWORD	m_dwTotal;
	DWORD	m_dwTotalSavedFile;
	HANDLE	m_hMapping;
	LPBYTE	m_pMapStart;	// ###1.61�@�}�b�s���O�̈��[�i���������̃A�h���X�j
	DWORD   m_dwFileOffset; //�f�[�^�t�@�C���̃}�b�s���O�J�n�A�h���X�i�t�@�C�����̃A�h���X�j
	DWORD	m_dwMapSize; //�f�[�^�t�@�C���̃}�b�s���O�T�C�Y
	DWORD	m_dwAllocationGranularity;
	BOOL	m_bReadOnly;
	DWORD	m_dwBase;		// ###1.63

	BOOL	m_bModified;
	CSuperFileCon* m_pDupDoc;
	CAtlArray<TAMAUndoRedo*> m_undo;
	size_t m_savedIndex;
	size_t m_redoIndex;

	struct _TAMAFILECHUNK_HEAD m_filemapHead;

protected:


	void _DeleteContents() 
	{
//		if(m_pData) {
//			if(IsFileMapping()) {
//				ATLVERIFY(::UnmapViewOfFile(m_pMapStart ? m_pMapStart : m_pData));
//				m_pMapStart = NULL;
//				m_dwFileOffset = 0;
//				m_dwMapSize = 0;
//			}
			//else
			//	free(m_pData);
//			m_pData = NULL;
//			m_dwTotal = 0;
//			m_dwBase = 0;
//			UpdateAllViews(NULL);
//		}
//		if(IsFileMapping()) {
//			if(m_pDupDoc)
//			{
//				m_pDupDoc->m_pDupDoc = NULL;
//				m_pDupDoc = NULL;
//				m_hMapping = NULL;
//				m_file.Detach();//m_pFileMapping = NULL;
//			} else {
//				ATLVERIFY(::CloseHandle(m_hMapping));
//				m_hMapping = NULL;
//				m_file.Close();
//			}
//		}

		m_bReadOnly = FALSE;
	}

  void* mallocMax(size_t *nIdealSize)
  {
    void *pAlloc = NULL;
    for(size_t nTrySize = *nIdealSize; nTrySize > 2; nTrySize /= 2) {
      pAlloc = malloc(nTrySize);
      if(pAlloc)
      {
        *nIdealSize = nTrySize;
        return pAlloc;
      }
    }
    ATLASSERT(FALSE);
    *nIdealSize = 0;
    return NULL;
  }
  void* mallocMax(DWORD *dwIdealSize)
  {
    size_t nIdealSize = *dwIdealSize;
    void *pAlloc = mallocMax(&nIdealSize);
    *dwIdealSize = nIdealSize;
    return pAlloc;
  }

#define SHIFTBUFSIZE 8*1024*1024
	BOOL _TAMAFILECHUNK_ShiftFileChunkR(TAMAFILECHUNK *fileChunk)
  {
		ATLASSERT(fileChunk);
		ATLASSERT(fileChunk->bSaved==FALSE);
		ATLASSERT(fileChunk->dataChunk->dataType == CHUNK_FILE);

		DWORD dwInsStart = _TAMAFILECHUNK_GetRealStartAddr(fileChunk);
    ATLASSERT(fileChunk->dwStart >= dwInsStart);
		DWORD dwInsSize = fileChunk->dwStart - dwInsStart;
    ATLASSERT(fileChunk->dwEnd >= fileChunk->dwStart);
    DWORD dwShiftSize = fileChunk->dwEnd - fileChunk->dwStart +1;

    DWORD dwRemain = dwShiftSize;
    if(dwInsSize==0 || dwRemain==0)return TRUE;
    DWORD dwBufSize = SHIFTBUFSIZE;
    DWORD dwCopySize = min(dwBufSize, dwRemain);
    LPBYTE buf = (LPBYTE)mallocMax(&dwCopySize);
    if(!buf)
    {
      ATLASSERT(FALSE);
      return FALSE;
    }
    DWORD dwMoveStart = dwInsStart + dwShiftSize - dwCopySize;
    BOOL bRet = FALSE;
    while(dwRemain!=0)
    {
      if(FAILED(m_file.Seek(dwMoveStart, FILE_BEGIN)) || FAILED(m_file.Read(buf, dwCopySize)))
      {
        ATLASSERT(FALSE);
        break;
      }
      if(FAILED(m_file.Seek(dwMoveStart+dwInsSize, FILE_BEGIN)) || FAILED(m_file.Write(buf, dwCopySize)))
      {
        ATLASSERT(FALSE);
        break;
      }
      dwRemain -= dwCopySize;
#ifdef DEBUG
      if(dwRemain==0)ATLASSERT(dwMoveStart==dwInsStart);
#endif
      dwMoveStart -= dwCopySize;
      dwCopySize = min(dwCopySize, dwRemain);
    }
    free(buf);
    return bRet;
	}
	BOOL _TAMAFILECHUNK_ShiftFileChunkL(TAMAFILECHUNK *fileChunk)
	{
		ATLASSERT(fileChunk);
		ATLASSERT(fileChunk->bSaved==FALSE);
		ATLASSERT(fileChunk->dataChunk->dataType == CHUNK_FILE);

		DWORD dwDelStart = fileChunk->dwStart;
    ATLASSERT(_TAMAFILECHUNK_GetRealStartAddr(fileChunk) >= dwDelStart);
		DWORD dwDelSize = _TAMAFILECHUNK_GetRealStartAddr(fileChunk) - dwDelStart;
    ATLASSERT(fileChunk->dwEnd >= fileChunk->dwStart);
    DWORD dwShiftSize = fileChunk->dwEnd - fileChunk->dwStart +1;

    DWORD dwRemain = dwShiftSize;
    if(dwDelSize==0 || dwRemain==0)return TRUE;
    DWORD dwBufSize = SHIFTBUFSIZE;
    DWORD dwCopySize = min(dwBufSize, dwRemain);
    LPBYTE buf = (LPBYTE)mallocMax(&dwCopySize);
    if(!buf)
    {
      ATLASSERT(FALSE);
      return FALSE;
    }
    DWORD dwMoveStart = dwDelStart;
    BOOL bRet = FALSE;
    while(dwRemain!=0)
    {
      if(FAILED(m_file.Seek(dwMoveStart, FILE_BEGIN)) || FAILED(m_file.Read(buf, dwCopySize)))
      {
        ATLASSERT(FALSE);
        break;
      }
      if(FAILED(m_file.Seek(dwMoveStart-dwDelSize, FILE_BEGIN)) || FAILED(m_file.Write(buf, dwCopySize)))
      {
        ATLASSERT(FALSE);
        break;
      }
      dwRemain -= dwCopySize;
      dwMoveStart += dwCopySize;
      dwCopySize = min(dwCopySize, dwRemain);
    }
    free(buf);
    return bRet;
	}


	DWORD _GetFileLengthLimit4G(CAtlFile *file = NULL, BOOL bErrorMsg = FALSE)
	{
    CAtlFile *_file = &m_file;
    if(file)_file = file;
		DWORD dwSize = 0;
		ULARGE_INTEGER ulFileSize;
		ulFileSize.QuadPart = 0;
		if(SUCCEEDED(_file->GetSize(ulFileSize.QuadPart)))
		{
			if(ulFileSize.QuadPart > MAX_FILELENGTH)
			{
				if(bErrorMsg)
				{
					//CString strErrOver4G;
					//strErrOver4G.LoadString(IDS_ERR_OVER4G);
					//MessageBox(NULL, strErrOver4G, _T("Error"), MB_OK);
				}
				dwSize = MAX_FILELENGTH;
			} else dwSize = ulFileSize.LowPart;
		}
		return dwSize;
	}

	inline void _UndoRedo_RemoveRange(size_t delStartIndex, size_t nDelSize)
	{
		for(size_t i=0;i<nDelSize;i++)
		{
			_UndoRedo_ReleaseByIndex(delStartIndex+i);
		}
		m_undo.RemoveAt(delStartIndex, nDelSize);
		if(delStartIndex<m_savedIndex)m_savedIndex -= nDelSize;
		if(delStartIndex<m_redoIndex)m_redoIndex -= nDelSize;
	}
	BOOL inline _UndoRedo_RemoveByIndex(size_t nIndex)	{	if(_UndoRedo_Release(m_undo.GetAt(nIndex))){ m_undo.RemoveAt(nIndex); return TRUE; }
															return FALSE;}
	BOOL inline _UndoRedo_ReleaseByIndex(size_t nIndex)	{ return _UndoRedo_Release(m_undo.GetAt(nIndex)); }
	BOOL inline _UndoRedo_Release(TAMAUndoRedo *undo)
	{
		if(undo==NULL)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		if(undo->dataNext)_TAMADataChunks_Release(undo->dataNext, undo->nDataNext);
		if(undo->dataPrev)_TAMADataChunks_Release(undo->dataPrev, undo->nDataPrev);
		free(undo);
		return TRUE;
	}
	BOOL _TAMADataChunks_Release(TAMADataChunk **chunks, DWORD dwChunks, BOOL bFreeRawPointer = TRUE)
	{
		for(DWORD i=0; i<dwChunks; i++)
		{
			ATLASSERT(chunks[i]);
			if(chunks[i])_TAMADataChunk_Release(chunks[i], bFreeRawPointer);
		}
		free(chunks);
	}
	// TRUE  - nRefCount==0
	// FALSE - nRefCount!=0
	BOOL inline _TAMADataChunk_Release(TAMADataChunk *chunk, BOOL bFreeRawPointer = TRUE)
	{
		BOOL bRet = FALSE;
		if(chunk==NULL)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		if(chunk->dataType==CHUNK_MEM && chunk->dataMem) bRet=_TAMADataBuf_Release(chunk->dataMem, bFreeRawPointer);
		else bRet = TRUE;
		free(chunk);
		return bRet;
	}
	BOOL inline _TAMADataBuf_Release(TAMADataBuf *pDataBuf, BOOL bFreeRawPointer = TRUE)
	{
		if(pDataBuf==NULL)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		if(pDataBuf->nRefCount==0 || --(pDataBuf->nRefCount) == 0)
		{
			if(pDataBuf->pData && bFreeRawPointer)free(pDataBuf->pData);
			free(pDataBuf);
			return TRUE;
		}
		return FALSE;
	}

	void LastErrorMessageBox()
	{
		MessageBox(NULL, AtlGetErrorDescription(::GetLastError(), LANG_USER_DEFAULT), _T("Error"), MB_OK | MB_ICONERROR);
	}

	inline TAMAFILECHUNK * _FileMap_LookUp(DWORD dwSearchOffset)
	{
		TAMAFILECHUNK findChunk, *pSplitChunk;
		findChunk.key = dwSearchOffset;
		pSplitChunk = RB_NFIND(_TAMAFILECHUNK_HEAD, &m_filemapHead, &findChunk);
		return pSplitChunk;
	}
	
	TAMAFILECHUNK* _TAMAFILECHUNK_Copy(TAMAFILECHUNK *fileChunk)
	{
		TAMAFILECHUNK *copyFileChunk = (TAMAFILECHUNK *)malloc(sizeof(TAMAFILECHUNK));
		if(!copyFileChunk)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		copyFileChunk->dwEnd = fileChunk->dwEnd;
		copyFileChunk->dwStart = fileChunk->dwStart;
		copyFileChunk->dataChunk = _TAMADataChunk_Copy(fileChunk->dataChunk);
		if(!copyFileChunk->dataChunk)
		{
			ATLASSERT(FALSE);
			free(copyFileChunk);
			return NULL;
		}
		copyFileChunk->bSaved = fileChunk->bSaved;
		return copyFileChunk;
	}

	//pSplitChunk[dwStart,(dwSplitPoint),dwEnd] >>>
	// pNewFirstChunk[dwStart,dwSplitPoint-1] -(Split)- pSplitChunk[dwSplitPoint,dwEnd]
	BOOL _FileMap_SplitPoint(DWORD dwSplitPoint)
	{
		TAMAFILECHUNK *pSplitChunk;
		pSplitChunk = _FileMap_LookUp(dwSplitPoint);
		if(!pSplitChunk)
		{
			//ATLASSERT(FALSE);
			return TRUE;//return FALSE;
		}
		if(pSplitChunk->dwStart != dwSplitPoint)
		{
			TAMAFILECHUNK *pNewFirstChunk = _TAMAFILECHUNK_Copy(pSplitChunk);//(TAMAFILECHUNK *)malloc(sizeof(TAMAFILECHUNK));
			if(pNewFirstChunk==NULL)
			{
				ATLASSERT(FALSE);
				return FALSE;
			}
			DWORD dwFirstSize = dwSplitPoint - pSplitChunk->dwStart;
			pNewFirstChunk->dwEnd   = dwSplitPoint-1;
			ATLASSERT(pSplitChunk->dataChunk);
			DataChunkType type = pSplitChunk->dataChunk->dataType;
			//pNewFirstChunk->dataChunk->dataType = type;
			switch(type)
			{
			case CHUNK_FILE:
				pSplitChunk->dataChunk->dataFileAddr += dwFirstSize;
				pSplitChunk->dataChunk->dwSize -= dwFirstSize;
				pNewFirstChunk->dataChunk->dwSize = dwFirstSize;
				break;
			case CHUNK_MEM:
				pSplitChunk->dataChunk->dwSkipOffset += dwFirstSize;
				pSplitChunk->dataChunk->dwSize -= dwFirstSize;
				pNewFirstChunk->dataChunk->dwSize = dwFirstSize;
				break;
			default:
				ATLASSERT(FALSE);
				break;
			}
			__FileMap_LowInsert(pNewFirstChunk);
			pSplitChunk->dwStart = dwSplitPoint;
		}
		return TRUE;
	}
	
	inline TAMAFILECHUNK* __FileMap_LowInsert(TAMAFILECHUNK* pInsert)
	{
		return RB_INSERT(_TAMAFILECHUNK_HEAD, &m_filemapHead, pInsert);
	}

	TAMAFILECHUNK* _FileMap_BasicInsert(DWORD dwStart, DWORD dwSize)
	{
		TAMAFILECHUNK *pNewChunk = (TAMAFILECHUNK *)malloc(sizeof(TAMAFILECHUNK));
		//DWORD dwStart = dwEnd - dwSize +1;
		DWORD dwEnd = _GetEndOffset(dwStart, dwSize);
		if(dwSize==0 || !pNewChunk)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		_FileMap_Shift(dwStart, dwSize);

		pNewChunk->dwStart = dwStart;
		pNewChunk->dwEnd = dwEnd;
		__FileMap_LowInsert(pNewChunk);
		return pNewChunk;
	}
	BOOL _FileMap_InsertMemAssign(DWORD dwInsStart, LPBYTE srcDataDetached, DWORD dwInsSize)
	{
		//DWORD dwInsEnd = _GetEndOffset(dwInsStart, dwInsSize);
		TAMADataChunk *dataChunk = _TAMADataChunk_CreateMemAssignRawPointer(dwInsSize, 0, srcDataDetached);
		if(!dataChunk)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		TAMAFILECHUNK *insChunk = _FileMap_BasicInsert(dwInsStart, dwInsSize);
		if(!insChunk)
		{
			ATLASSERT(FALSE);
      _TAMADataChunk_Release(dataChunk);
			return FALSE;
		}
		insChunk->dataChunk = dataChunk;

		//OptimizeFileMap(insChunk);
		return TRUE;
	}
	BOOL _FileMap_InsertMemCopy(DWORD dwInsStart, LPBYTE pSrcData, DWORD dwInsSize)
	{
		LPBYTE pMem = (LPBYTE)malloc(dwInsSize);
    if(!pMem)
    {
      ATLASSERT(FALSE);
      return FALSE;
    }
    memcpy(pMem, pSrcData, dwInsSize);
    if(!_FileMap_InsertMemAssign(dwInsStart, pMem, dwInsSize))
    {
      ATLASSERT(FALSE);
      free(pMem);
      return FALSE;
    }
    return TRUE;
  }
	
	TAMAFILECHUNK* _TAMADataChunk_CreateFileChunk(TAMADataChunk *pDataChunk, DWORD dwStart)
	{
		TAMAFILECHUNK *pFileChunk = (TAMAFILECHUNK *)malloc(sizeof(TAMAFILECHUNK));
		if(!pFileChunk)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		pFileChunk->dwStart = dwStart;
		pFileChunk->dataChunk = _TAMADataChunk_Copy(pDataChunk);
		if(!pFileChunk->dataChunk)
		{
			ATLASSERT(FALSE);
			free(pFileChunk);
			return NULL;
		}
		pFileChunk->dwEnd = dwStart + pDataChunk->dwSize;
		return pFileChunk;
	}
	
	//return 0 ---- Failed
	DWORD _TAMADATACHUNKS_GetSumSize(TAMADataChunk **pDataChunks, DWORD nDataChunks)
	{
		DWORD dwSum = 0;
		for(DWORD i=0; i<nDataChunks; i++)
		{
			TAMADataChunk *pDC = pDataChunks[i];
			ATLASSERT(pDC->dwSize!=0);
			dwSum += pDC->dwSize;
			if(pDC->dwSize > dwSum)//Overflow check
			{
				ATLASSERT(FALSE);//Overflow
				return 0; //Failed
			}
		}
		return dwSum;
	}
	BOOL __FileMap_BasicInsertTAMADataChunks(DWORD dwInsStart, TAMADataChunk **ppDataChunks, DWORD nDataChunks)
	{
		DWORD dwFCStart = dwInsStart;
		for(DWORD i=0; i<nDataChunks; i++)
		{
			TAMADataChunk *pDC = ppDataChunks[i];
			ATLASSERT(pDC);
			TAMAFILECHUNK *pFileChunk = _TAMADataChunk_CreateFileChunk(pDC, dwFCStart);
			if(!pFileChunk)
			{
				ATLASSERT(FALSE);//FileMap����
				return FALSE;
			}
			__FileMap_LowInsert(pFileChunk);
			dwFCStart += pDC->dwSize;
			if(dwFCStart < pDC->dwSize)
			{
				ATLASSERT(FALSE);
				return FALSE;
			}
		}
		return TRUE;
	}
	BOOL _FileMap_InsertTAMADataChunks(DWORD dwInsStart, TAMADataChunk **pDataChunks, DWORD nDataChunks, DWORD dwInsSize=0)
	{
#ifdef _DEBUG
		ATLASSERT(dwInsSize==0 || dwInsSize==_TAMADATACHUNKS_GetSumSize(pDataChunks, nDataChunks));
#endif
		if(dwInsSize==0) //0 --- Automatic Calc
		{
			dwInsSize = _TAMADATACHUNKS_GetSumSize(pDataChunks, nDataChunks);
			if(dwInsSize==0)
			{
				ATLASSERT(FALSE);
				return FALSE;
			}
		}
		_FileMap_Shift(dwInsStart, dwInsSize);
		BOOL bRetIns = __FileMap_BasicInsertTAMADataChunks(dwInsStart, pDataChunks, nDataChunks);
		if(!bRetIns)//Insert Failed
		{
			ATLASSERT(FALSE);//Try to fix
			if(!__FileMap_DeleteRange(dwInsStart, dwInsSize) || !_FileMap_Shift(dwInsStart, dwInsSize, FALSE))
			{
				ATLASSERT(FALSE);//fatal error
				FatalError();
				return FALSE;
			}
			return FALSE;
		}
		//OptimizeFileMap(insChunk);
		
		return TRUE;
	}
	BOOL _FileMap_InsertFile(DWORD dwInsStart, DWORD dwInsSize, DWORD dwStartFileSpace)
	{
		//DWORD dwInsEnd = _GetEndOffset(dwInsStart, dwInsSize);
		TAMAFILECHUNK *insChunk = _FileMap_BasicInsert(dwInsStart, dwInsSize);
		if(!insChunk)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		TAMADataChunk *dataChunk = _TAMADataChunk_CreateFileChunk(dwInsSize, dwStartFileSpace);
		if(!dataChunk)
		{
			ATLASSERT(FALSE);
			FatalError();
			return FALSE;
		}
		insChunk->dataChunk = dataChunk;
		//insChunk->dwSkipOffset = 0;

		//OptimizeFileMap(insChunk);
		return TRUE;
	}
	TAMADataChunk* _TAMADataChunk_CreateFileChunk(DWORD dwSize, DWORD dwStartFileSpace)
	{
		TAMADataChunk *dataChunk = (TAMADataChunk *)malloc(sizeof(TAMADataChunk));
		if(dataChunk==NULL)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		dataChunk->dataType = CHUNK_FILE;
		dataChunk->dataFileAddr = dwStartFileSpace;
		dataChunk->dwSize = dwSize;
		dataChunk->dwSkipOffset = 0;
		return dataChunk;
	}
	
	TAMADataChunk* _TAMADataChunk_CreateMemAssignRawPointer(DWORD dwSize, DWORD dwSkipOffset, LPBYTE srcDataDetached)
	{
		TAMADataBuf *pTAMADataBuf = _TAMADataBuf_CreateAssign(srcDataDetached, 0/*nRefCount*/);
		if(!pTAMADataBuf)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		TAMADataChunk *retDataChunk = _TAMADataChunk_CreateMemAssignTAMADataBuf(dwSize, dwSkipOffset, pTAMADataBuf);
		if(!retDataChunk)
		{
			ATLASSERT(FALSE);
			_TAMADataBuf_Release(pTAMADataBuf);
			return NULL;
		}
		return retDataChunk;
	}
	
	TAMADataChunk* _TAMADataChunk_CreateMemAssignTAMADataBuf(DWORD dwSize, DWORD dwSkipOffset, TAMADataBuf *pTAMADataBuf)
	{
		TAMADataChunk *dataChunk = (TAMADataChunk *)malloc(sizeof(TAMADataChunk));
		if(dataChunk==NULL)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		if(pTAMADataBuf)
		{
			pTAMADataBuf->nRefCount++;
			dataChunk->dataMem = pTAMADataBuf;
		}
		dataChunk->dataType = CHUNK_MEM;
		dataChunk->dwSize = dwSize;
		dataChunk->dwSkipOffset = dwSkipOffset;
		return dataChunk;
	}
	TAMADataChunk* _TAMADataChunk_CreateMemNew(DWORD dwSize)
	{
		TAMADataChunk *dataChunk = (TAMADataChunk *)malloc(sizeof(TAMADataChunk));
		if(dataChunk==NULL)
		{
			ATLASSERT(FALSE);
			return NULL;
		}
		TAMADataBuf *pTAMADataBuf = _TAMADataBuf_CreateNew(dwSize, 1/*nRefCount*/);
		if(!pTAMADataBuf)
		{
			ATLASSERT(FALSE);
			free(dataChunk);
			return NULL;
		}
		dataChunk->dataMem = pTAMADataBuf;
		dataChunk->dataType = CHUNK_MEM;
		dataChunk->dwSize = dwSize;
		dataChunk->dwSkipOffset = 0;
		return dataChunk;
	}

	BOOL __FileMap_Del2(DWORD dwEnd, DWORD dwSize)
	{
		DWORD dwStart = dwEnd - dwSize +1;
		if(dwSize==0 || dwEnd==0xFFffFFff)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		DWORD dwShiftStart = dwEnd + 1;
		__FileMap_DeleteRange(dwStart, dwSize);
		_FileMap_Shift(dwShiftStart, dwSize, FALSE/*bPlus==FALSE (Shift-left: dwSize)*/);
		return TRUE;
	}
	BOOL _FileMap_Del(DWORD dwDelStart, DWORD dwDelSize)
	{
		DWORD dwDelEnd = _GetEndOffset(dwDelStart, dwDelSize);
		__FileMap_Del2(dwDelEnd, dwDelSize);
	}

	BOOL _FileMap_Shift(DWORD dwShiftStart, DWORD dwShiftSize, BOOL bPlus = TRUE)
	{
		if(!_FileMap_SplitPoint(dwShiftStart))
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		if(bPlus)
		{
			TAMAFILECHUNK *pChunk;
			RB_FOREACH_REVERSE(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
			{
				if(pChunk->dwStart < dwShiftStart)break;
				pChunk->dwStart += dwShiftSize;
				pChunk->dwEnd   += dwShiftSize;
			}
		} else {
			TAMAFILECHUNK *pChunk;
			RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
			{
				if(pChunk->dwStart >= dwShiftStart)
				{
					pChunk->dwStart -= dwShiftSize;
					pChunk->dwEnd   -= dwShiftSize;
				}
			}
		}
		return TRUE;
	}
	
	inline TAMAFILECHUNK* __FileMap_LowFind(TAMAFILECHUNK* pFind)
	{
		return RB_FIND(_TAMAFILECHUNK_HEAD, &m_filemapHead, pFind);
	}
	
	inline TAMAFILECHUNK* __FileMap_LowPrev(TAMAFILECHUNK* pFind)
	{
		return RB_PREV(_TAMAFILECHUNK_HEAD, &m_filemapHead, pFind);
	}
	
	inline TAMAFILECHUNK* __FileMap_LowNext(TAMAFILECHUNK* pFind)
	{
		return RB_NEXT(_TAMAFILECHUNK_HEAD, &m_filemapHead, pFind);
	}
	
	inline TAMAFILECHUNK* __FileMap_LowMin()
	{
		return RB_MIN(_TAMAFILECHUNK_HEAD, &m_filemapHead);
	}
	
	//low-level internal function for _FileMap_Del()... (not include to shift chunks)
	BOOL __FileMap_DeleteRange(DWORD dwStart, DWORD dwSize)
	{
		//DWORD dwStart = dwEnd - dwSize +1;
		DWORD dwEnd = _GetEndOffset(dwStart, dwSize);
		if(dwSize==0 || dwEnd==0xFFffFFff || !_FileMap_SplitPoint(dwStart) || !_FileMap_SplitPoint(dwEnd+1))
		{
			ATLASSERT(FALSE);
			return FALSE;
		}

		TAMAFILECHUNK findChunk;
		findChunk.key = dwEnd;
		TAMAFILECHUNK *pDeleteChunk, *pChunk;
		pDeleteChunk = __FileMap_LowFind(&findChunk);
		if(pDeleteChunk==NULL)
		{
			//Merge (Restore split)
			ATLASSERT(FALSE);
			return FALSE;
		}
		pChunk = __FileMap_LowPrev(pDeleteChunk);
		while(pChunk && pChunk->dwEnd < dwStart)
		{
			pChunk = __FileMap_LowPrev(pDeleteChunk);
			_FileMap_Remove(pDeleteChunk);
			pDeleteChunk = pChunk;
		}
		if(pDeleteChunk)_FileMap_Remove(pDeleteChunk);
	}
	
	TAMAFILECHUNK* __FileMap_LowRemove(TAMAFILECHUNK *pRemove)
	{
		return RB_REMOVE(_TAMAFILECHUNK_HEAD, &m_filemapHead, pRemove);
	}
	
	void _FileMap_Remove(TAMAFILECHUNK *pDeleteChunk)
	{
		switch(pDeleteChunk->dataChunk->dataType)
		{
			case CHUNK_FILE:
				//nothing to do
				break;
			case CHUNK_MEM:
				_TAMADataChunk_Release(pDeleteChunk->dataChunk);
				break;
			default:
				ATLASSERT(FALSE);
				break;
		}
		__FileMap_LowRemove(pDeleteChunk);
		free(pDeleteChunk);
	}

	//TAMAFILECHUNK * _FileMap_BasicOverWriteMem(DWORD dwStart, DWORD dwSize)
	//{
	//	TAMAFILECHUNK *pNewChunk = (TAMAFILECHUNK *)malloc(sizeof(TAMAFILECHUNK));
	//	//DWORD dwStart = dwEnd - dwSize +1;
	//	DWORD dwEnd = _GetEndOffset(dwStart, dwSize);
	//	if(dwSize==0 || !pNewChunk)return NULL;
	//	__FileMap_DeleteRange(dwStart, dwSize);
//
	//	pNewChunk->dwStart = dwStart;
	//	pNewChunk->dwEnd = dwEnd;
	//	pNewChunk->dataChunk = NULL;
	//	pNewChunk->dwSkipOffset = 0;
	//	__FileMap_LowInsert(pNewChunk);
	//	return pNewChunk;
	//}
	
	//BOOL _FileMap_OverWriteMem(DWORD dwStart, DWORD dwSize, TAMADataChunk *dataChunk, DWORD dwSkipOffset)
	//{
	//	TAMAFILECHUNK *chunk = _FileMap_BasicOverWriteMem(dwStart, dwSize);
	//	if(chunk==NULL)
	//	{
	//		ATLASSERT(FALSE);
	//		return FALSE;
	//	}
	//	chunk->dwSkipOffset = dwSkipOffset;
	//	dataChunk->nRefCount++;
	//	chunk->dataChunk = dataChunk;
//
	//	return TRUE;
	//}
	BOOL _FileMap_OverWriteTAMADataChunks(DWORD dwStart, TAMADataChunk **pDataChunks, DWORD nDataChunks, DWORD dwSize=0)
	{
#ifdef _DEBUG
		ATLASSERT(dwSize==0 || dwSize==_TAMADATACHUNKS_GetSumSize(pDataChunks, nDataChunks));
#endif
		if(dwSize==0) //0 --- Automatic Calc
		{
			dwSize = _TAMADATACHUNKS_GetSumSize(pDataChunks, nDataChunks);
			if(dwSize==0)
			{
				ATLASSERT(FALSE);
				return FALSE;
			}
		}
		if(		!__FileMap_DeleteRange(dwStart, dwSize)
			|| 	!__FileMap_BasicInsertTAMADataChunks(dwStart, pDataChunks, nDataChunks)
		)
		{
			ATLASSERT(FALSE);//fatal error
			exit(-1);
			return FALSE;
		}
		//OptimizeFileMap(sideChunk);
		
		return TRUE;
	}
	
	BOOL _TAMAFILECHUNK_IsContainPoint(TAMAFILECHUNK *fileChunk, DWORD dwPoint) { return fileChunk->dwStart <= dwPoint && dwPoint <= fileChunk->dwEnd; }
	
	BOOL _FileMap_CreateTAMADataChunks(DWORD dwStart, DWORD dwSize, TAMADataChunk*** ppDataChunks, DWORD *nDataChunks)
	{
		TAMAFILECHUNK *pFCStart = _FileMap_LookUp(dwStart);
		if(!pFCStart)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		DWORD dwEnd = _GetEndOffset(dwStart, dwSize);
		TAMAFILECHUNK *pFCCur = pFCStart;
		DWORD dwDataChunksSize = 1;
		while(1)
		{
			if(_TAMAFILECHUNK_IsContainPoint(pFCCur, dwEnd))break;
			pFCCur = __FileMap_LowNext(pFCCur);
			if(!pFCCur)
			{
				ATLASSERT(FALSE);
				return FALSE;
			}
			ATLASSERT(dwDataChunksSize<0xFFffFFff);
			dwDataChunksSize++;
			ATLASSERT(dwEnd > pFCCur->dwStart);
			ATLASSERT(dwStart < pFCCur->dwEnd);
		}
		TAMAFILECHUNK *pFCEnd = pFCCur;
		TAMADataChunk **pDataChunks = (TAMADataChunk **)malloc(sizeof(TAMADataChunk *)*dwDataChunksSize);
		if(!pDataChunks)
		{
			ATLASSERT(FALSE);
			return FALSE;
		}
		pFCCur = pFCStart;
		TAMADataChunk *pDC = _TAMADataChunk_Copy(pFCStart->dataChunk);
		if(!pDC)
		{
			ATLASSERT(FALSE);
			free(pDataChunks);
			return FALSE;
		}
		pDataChunks[0] = pDC;
		ATLASSERT(pFCStart->dwStart >= dwStart);
		DWORD dwSkipS = pFCStart->dwStart - dwStart;
		pDC->dwSkipOffset += dwSkipS;
		pDC->dwSize -= dwSkipS;
		
		for(DWORD i=1; ; i++)
		{
			if(pFCCur==pFCEnd)
			{
				*nDataChunks = i;
				ATLASSERT(dwEnd<=pFCEnd->dwEnd);
				DWORD dwDecE = pFCEnd->dwEnd - dwEnd;
				ATLASSERT(pDC->dwSize > dwDecE);
				pDC->dwSize -= dwDecE;
				break;
			}
			pFCCur = __FileMap_LowNext(pFCCur);
			if(!pFCCur || !(pDC = _TAMADataChunk_Copy(pFCCur->dataChunk)))
			{
				ATLASSERT(FALSE);
				_TAMADataChunks_Release(pDataChunks, i);
				return FALSE;
			}
			ATLASSERT(i<dwDataChunksSize);
			pDataChunks[i] = pDC;
		}
		*ppDataChunks = pDataChunks;
		return TRUE;
	}
	
	
	BOOL _OldFileMap_Make(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, CAtlFile *pFile)
	{
		RB_INIT(pOldFilemapHead);
		{//init map
			TAMAOLDFILECHUNK * c = (TAMAOLDFILECHUNK *)malloc(sizeof(TAMAOLDFILECHUNK));
			if(c==NULL)
			{
				_OldFileMap_FreeAll(pOldFilemapHead, TRUE);
				return FALSE;
			}
			c->type = OF_NOREF;
			c->dwStart=0;
			c->dwEnd=m_dwTotalSavedFile-1;
			RB_INSERT(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, c);
		}
		
		//searching...
		TAMAFILECHUNK *pChunk;
		RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
		{
			TAMADataChunk *dataChunk = pChunk->dataChunk;
			ATLASSERT(dataChunk);
			switch(dataChunk->dataType)
			{
			case CHUNK_FILE:
			{
				ATLASSERT(dataChunk->dwSize>0);
				if(dataChunk->dwSize > 0)
				{
					DWORD dwOldFileAddrS = _TAMADataChunk_GetRealStartAddr(dataChunk);
					DWORD dwOldFileAddrE = dwOldFileAddrE + dataChunk->dwSize - 1;
					_OldFileMap_FF(pOldFilemapHead, dwOldFileAddrS, dwOldFileAddrE, pChunk->dwStart);
					//dataChunk->savingType = DC_FF;
				}
				break;
			}
			case CHUNK_MEM:
				break;
			default:
				ATLASSERT(FALSE);
				break;
			}
		}
		size_t nUndo = m_undo.GetCount();
		for(size_t i=0; i < nUndo; i++)
		{
			TAMAUndoRedo *undo = m_undo[i];
			ATLASSERT(undo);
			TAMADataChunk **pDataChunks = undo->dataNext;
			DWORD nDataChunks = undo->nDataNext;
			for(size_t j=0; j<nDataChunks; j++)
			{
				TAMADataChunk *dataChunk = pDataChunks[j];
				if(dataChunk && dataChunk->dataType==CHUNK_FILE)
				{
					ATLASSERT(dataChunk->dwSize>0);
					if(dataChunk->dwSize > 0)
					{
						DWORD dwOldFileAddrS = _TAMADataChunk_GetRealStartAddr(dataChunk);
						DWORD dwOldFileAddrE = dwOldFileAddrE + dataChunk->dwSize - 1;
						_OldFileMap_FD(pOldFilemapHead, dwOldFileAddrS, dwOldFileAddrE);
						//dataChunk->savingType = DC_FD;
					}
				}
			}
			pDataChunks = undo->dataPrev;
			nDataChunks = undo->nDataPrev;
			for(size_t j=0; j<nDataChunks; j++)
			{
				TAMADataChunk *dataChunk = pDataChunks[j];
				if(dataChunk && dataChunk->dataType==CHUNK_FILE)
				{
					ATLASSERT(dataChunk->dwSize>0);
					if(dataChunk->dwSize > 0)
					{
						DWORD dwOldFileAddrS = _TAMADataChunk_GetRealStartAddr(dataChunk);
						DWORD dwOldFileAddrE = dwOldFileAddrE + dataChunk->dwSize - 1;
						_OldFileMap_FD(pOldFilemapHead, dwOldFileAddrS, dwOldFileAddrE);
						//dataChunk->savingType = DC_FD;
					}
				}
			}
		}
		
		
		TAMAOLDFILECHUNK *pOldFileChunk;
		RB_FOREACH(pOldFileChunk, _TAMAOLDFILECHUNK_HEAD, pOldFilemapHead)
		{
			if(pOldFileChunk->type==OF_FD)
			{
				ATLASSERT(pOldFileChunk->dwEnd > pOldFileChunk->dwStart);
				DWORD dwReadSize = pOldFileChunk->dwEnd - pOldFileChunk->dwStart;
				pOldFileChunk->pMem = (LPBYTE)malloc(dwReadSize);
				if(pOldFileChunk->pMem==NULL || FAILED(pFile->Seek(pOldFileChunk->dwStart, FILE_BEGIN)) || FAILED(pFile->Read(pOldFileChunk->pMem, dwReadSize)))
				{
					_OldFileMap_FreeAll(pOldFilemapHead, TRUE);
					return FALSE;
				}
			}
		}
		return TRUE;
	}
	
	void _OldFileMap_FreeAll(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, BOOL bFreeBuffer = TRUE)
	{
		TAMAOLDFILECHUNK *pChunk, *pChunkPrev = NULL;
		RB_FOREACH(pChunk, _TAMAOLDFILECHUNK_HEAD, pOldFilemapHead)
		{
			if(pChunkPrev)_OldFileMap_Free(pOldFilemapHead, pChunkPrev);
			pChunkPrev = pChunk;
		}
		if(pChunkPrev)_OldFileMap_Free(pOldFilemapHead, pChunkPrev);
	}
	
	void _OldFileMap_Free(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, TAMAOLDFILECHUNK *pChunk, BOOL bFreeBuffer = TRUE)
	{
		if(bFreeBuffer && pChunk->type==OF_FD && pChunk->pMem)free(pChunk->pMem);
		RB_REMOVE(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk);
		free(pChunk);
	}

	inline TAMAOLDFILECHUNK * _OldFileMap_LookUp(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, DWORD dwSearchOffset)
	{
		TAMAOLDFILECHUNK findChunk, *pSplitChunk;
		findChunk.key = dwSearchOffset;
		pSplitChunk = RB_NFIND(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, &findChunk);
		return pSplitChunk;
	}
	
	BOOL _OldFileMap_SplitPoint(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, DWORD dwSplitPoint)
	{
		TAMAOLDFILECHUNK *pSplitChunk;
		pSplitChunk = _OldFileMap_LookUp(pOldFilemapHead, dwSplitPoint);
		if(!pSplitChunk)return FALSE;
		if(pSplitChunk->dwStart != dwSplitPoint)
		{
			TAMAOLDFILECHUNK *pNewFirstChunk = (TAMAOLDFILECHUNK *)malloc(sizeof(TAMAOLDFILECHUNK));
			if(pNewFirstChunk==NULL)return FALSE;
			DWORD dwFirstSize = dwSplitPoint - pSplitChunk->dwStart;
			pNewFirstChunk->dwStart = pSplitChunk->dwStart;
			pNewFirstChunk->dwEnd   = dwSplitPoint-1;
			pNewFirstChunk->type = pSplitChunk->type;
			pNewFirstChunk->dwNewFileAddr = pSplitChunk->dwNewFileAddr;
			RB_INSERT(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pNewFirstChunk);
			pSplitChunk->dwStart = dwSplitPoint;
			if(pSplitChunk->type==OF_FF)pSplitChunk->dwNewFileAddr += dwFirstSize;
			ATLASSERT(pSplitChunk->dwNewFileAddr >= pNewFirstChunk->dwNewFileAddr);
		}
		return TRUE;
	}
	
	void _OldFileMap_MeltFF(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, TAMAOLDFILECHUNK *pChunk)
	{
		ATLASSERT(pChunk->type==OF_FF);
		TAMAOLDFILECHUNK *pChunk2 = RB_NEXT(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk);
		if(pChunk2 && pChunk2->type == OF_FF && pChunk2->dwNewFileAddr - pChunk->dwNewFileAddr == pChunk2->dwStart - pChunk->dwStart)
		{
			DWORD dwNewEnd = pChunk2->dwEnd;
			RB_REMOVE(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk2);
			free(pChunk2);
			pChunk->dwEnd = dwNewEnd;
		}
		
		pChunk2 = RB_PREV(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk);
		if(pChunk2 && pChunk2->type == OF_FF && pChunk->dwNewFileAddr - pChunk2->dwNewFileAddr == pChunk->dwStart - pChunk2->dwStart)
		{
			DWORD dwNewStart = pChunk2->dwStart;
			DWORD dwNewFileAddr = pChunk2->dwNewFileAddr;
			RB_REMOVE(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk2);
			free(pChunk2);
			pChunk->dwStart = dwNewStart;
			pChunk->dwNewFileAddr = dwNewFileAddr;
		}
	}
	
	void _OldFileMap_FF(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, DWORD dwStart, DWORD dwEnd, DWORD dwNewFileAddr)
	{
		TAMAOLDFILECHUNK *pOldFileChunkS = _OldFileMap_LookUp(pOldFilemapHead, dwStart);
		ATLASSERT(pOldFileChunkS);
		if(pOldFileChunkS->type == OF_FF && pOldFileChunkS->dwEnd >= dwEnd && dwNewFileAddr - pOldFileChunkS->dwNewFileAddr == dwStart - pOldFileChunkS->dwStart)return;
		if(pOldFileChunkS->dwStart < dwStart && (pOldFileChunkS->type != OF_FF || dwNewFileAddr - pOldFileChunkS->dwNewFileAddr != dwStart - pOldFileChunkS->dwStart))
		{
			_OldFileMap_SplitPoint(pOldFilemapHead, dwStart);
			pOldFileChunkS = _OldFileMap_LookUp(pOldFilemapHead, dwStart);
		}
		
		TAMAOLDFILECHUNK *pOldFileChunkE = _OldFileMap_LookUp(pOldFilemapHead, dwEnd);
		ATLASSERT(pOldFileChunkE);
		if(pOldFileChunkE->dwEnd > dwEnd && (pOldFileChunkE->type != OF_FF || dwNewFileAddr - pOldFileChunkE->dwNewFileAddr != dwStart - pOldFileChunkE->dwStart))
		{
			_OldFileMap_SplitPoint(pOldFilemapHead, dwEnd+1);
			pOldFileChunkE = _OldFileMap_LookUp(pOldFilemapHead, dwEnd);
		}
		
		TAMAOLDFILECHUNK *pChunk = pOldFileChunkS;
		while(1)
		{
			if(pChunk->type != OF_FF || (pChunk->dwStart >= dwStart && pChunk->dwEnd <= dwEnd))
			{
				pChunk->type = OF_FF;
				pChunk->dwNewFileAddr = dwNewFileAddr - (dwStart - pChunk->dwStart);
				ATLASSERT(pChunk->dwNewFileAddr < dwNewFileAddr);
			}
			_OldFileMap_MeltFF(pOldFilemapHead, pChunk);
			if(pChunk->dwEnd >= dwEnd)break;
			pChunk = RB_NEXT(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk);
			ATLASSERT(pChunk);
		}
	}
	
	void _OldFileMap_MeltFD(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, TAMAOLDFILECHUNK *pChunk)
	{
		ATLASSERT(pChunk->type==OF_FD);
		TAMAOLDFILECHUNK *pChunk2 = RB_NEXT(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk);
		if(pChunk2 && pChunk2->type == OF_FD)
		{
			DWORD dwNewEnd = pChunk2->dwEnd;
			RB_REMOVE(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk2);
			free(pChunk2);
			pChunk->dwEnd = dwNewEnd;
		}
		
		pChunk2 = RB_PREV(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk);
		if(pChunk2 && pChunk2->type == OF_FD)
		{
			DWORD dwNewStart = pChunk2->dwStart;
			RB_REMOVE(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk2);
			free(pChunk2);
			pChunk->dwStart = dwNewStart;
		}
	}
	
	void _OldFileMap_FD(struct _TAMAOLDFILECHUNK_HEAD *pOldFilemapHead, DWORD dwStart, DWORD dwEnd)
	{
		TAMAOLDFILECHUNK *pOldFileChunkS = _OldFileMap_LookUp(pOldFilemapHead, dwStart);
		ATLASSERT(pOldFileChunkS);
		if(pOldFileChunkS->type == OF_FD && pOldFileChunkS->dwEnd >= dwEnd)return;
		if(pOldFileChunkS->type == OF_NOREF && pOldFileChunkS->dwStart < dwStart)
		{
			_OldFileMap_SplitPoint(pOldFilemapHead, dwStart);
			pOldFileChunkS = _OldFileMap_LookUp(pOldFilemapHead, dwStart);
		}
		
		TAMAOLDFILECHUNK *pOldFileChunkE = _OldFileMap_LookUp(pOldFilemapHead, dwEnd);
		ATLASSERT(pOldFileChunkE);
		if(pOldFileChunkE->type == OF_NOREF && pOldFileChunkE->dwEnd > dwEnd)
		{
			_OldFileMap_SplitPoint(pOldFilemapHead, dwEnd+1);
			pOldFileChunkE = _OldFileMap_LookUp(pOldFilemapHead, dwEnd);
		}
		
		TAMAOLDFILECHUNK *pChunk = pOldFileChunkS;
		while(1)
		{
			if(pChunk->type == OF_NOREF)
			{
				pChunk->type = OF_FD;
				pChunk->pMem = NULL;
				_OldFileMap_MeltFD(pOldFilemapHead, pChunk);
			}
			if(pChunk->dwEnd >= dwEnd)break;
			pChunk = RB_NEXT(_TAMAOLDFILECHUNK_HEAD, pOldFilemapHead, pChunk);
			ATLASSERT(pChunk);
		}
	}

#ifdef _DEBUG
  bool _FileMap_DEBUG_ValidationCheck()
  {
    DWORD nextStart = 0;
    TAMAFILECHUNK *pChunk;
    RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
    {
      if(pChunk->dwStart!=nextStart)
      {
        ATLASSERT(FALSE);
        return false;
      }
      if(!_TAMAFILECHUNK_TAMADataChunk_DEBUG_SizeCheck(pChunk))
      {
        ATLASSERT(FALSE);
        return false;
      }
      nextStart=pChunk->dwEnd+1;
    }
    return true;
  }

  bool _TAMAFILECHUNK_TAMADataChunk_DEBUG_SizeCheck(TAMAFILECHUNK *fileChunk)
  {
    ATLASSERT(fileChunk->dwEnd >= fileChunk->dwStart);
    DWORD dwFileChunkSize = fileChunk->dwEnd - fileChunk->dwStart + 1;
    ATLASSERT(fileChunk->dataChunk->dwSize >= dwFileChunkSize);
    return fileChunk->dwEnd >= fileChunk->dwStart && fileChunk->dataChunk->dwSize >= dwFileChunkSize;
  }

  DWORD _FileMap_DEBUG_GetCount()
  {
    DWORD c = 0;
    TAMAFILECHUNK *pChunk;
    RB_FOREACH(pChunk, _TAMAFILECHUNK_HEAD, &m_filemapHead)
    {
      c++;
    }
    return c;
  }
#endif


};