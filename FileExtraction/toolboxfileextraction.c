/****************************************************************************
 *
 * File:            toolboxfileextraction.c
 *
 * Usage:           toolboxfileextraction <inputPath> <outputDir>
 *                  Example: in.pdf dir/subdir/
 *                  
 * Title:           Extract files embedded from a PDF
 *                  
 * Description:     Extract the embedded files contained in the PDF to the
 *                  file system.
 *                  
 * Author:          PDF Tools AG
 *
 * Copyright:       Copyright (C) 2026 PDF Tools AG, Switzerland
 *                  Permission to use, copy, modify, and distribute this
 *                  software and its documentation for any purpose and without
 *                  fee is hereby granted, provided that the above copyright
 *                  notice appear in all copies and that both that copyright
 *                  notice and this permission notice appear in supporting
 *                  documentation. This software is provided "as is" without
 *                  express or implied warranty.
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PdfTools_Toolbox.h"

#include <locale.h>
#include "compat.h"


#define MIN(a, b)     (((a) < (b) ? (a) : (b)))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a)[0])

#define GOTO_CLEANUP_IF_NULL(inFile, ...) \
    do                                    \
    {                                     \
        if ((inFile) == NULL)             \
        {                                 \
            _tprintf(__VA_ARGS__);        \
            iReturnValue = 1;             \
            goto cleanup;                 \
        }                                 \
    } while (0);

#define GOTO_CLEANUP_IF_NULL_PRINT_ERROR(inVar, ...)                                      \
    do                                                                                    \
    {                                                                                     \
        if ((inVar) == NULL)                                                              \
        {                                                                                 \
            nBufSize = Ptx_GetLastErrorMessage(NULL, 0);                                  \
            Ptx_GetLastErrorMessage(szErrorBuff, MIN(ARRAY_SIZE(szErrorBuff), nBufSize)); \
            _tprintf(__VA_ARGS__);                                                        \
            iReturnValue = 1;                                                             \
            goto cleanup;                                                                 \
        }                                                                                 \
    } while (0);

#define GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(outBool, ...)                                   \
    do                                                                                    \
    {                                                                                     \
        if ((outBool) == FALSE)                                                           \
        {                                                                                 \
            nBufSize = Ptx_GetLastErrorMessage(NULL, 0);                                  \
            Ptx_GetLastErrorMessage(szErrorBuff, MIN(ARRAY_SIZE(szErrorBuff), nBufSize)); \
            _tprintf(__VA_ARGS__);                                                        \
            iReturnValue = 1;                                                             \
            goto cleanup;                                                                 \
        }                                                                                 \
    } while (0);

int Usage()
{
    printf("Usage: toolboxfileextraction <inputPath> <outputDir>.\n");
    printf("       Example: in.pdf dir/subdir/\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

int extractFile(TPtxPdf_FileReference* pFileRef, const TCHAR* szOutputDir)
{
    FILE*                    pOutStream = NULL;
    TPtxSys_StreamDescriptor outDescriptor;
    TCHAR                    szOutPath[512] = {'\0'};
    TCHAR                    szName[256]    = {'\0'};

    // Get file name
    size_t nNameSize = PtxPdf_FileReference_GetName(pFileRef, NULL, 0);
    if (nNameSize == 0)
    {
        _tprintf(_T("Failed to get embedded file name.\n"));
        return FALSE;
    }
    if (nNameSize > ARRAY_SIZE(szName))
        nNameSize = ARRAY_SIZE(szName);
    PtxPdf_FileReference_GetName(pFileRef, szName, nNameSize);

    // Build output path
    _stprintf(szOutPath, _T("%s/%s"), szOutputDir, szName);

    // Open output file
    pOutStream = _tfopen(szOutPath, _T("wb+"));
    if (pOutStream == NULL)
    {
        _tprintf(_T("Failed to open output file \"%s\".\n"), szOutPath);
        return FALSE;
    }
    PtxSysCreateFILEStreamDescriptor(&outDescriptor, pOutStream, 0);

    // Get the data stream and copy it to the output file
    TPtxSys_StreamDescriptor  dataStreamDesc;
    BOOL                      bGetData    = PtxPdf_FileReference_GetData(pFileRef, &dataStreamDesc);
    TPtxSys_StreamDescriptor* pDataStream = bGetData ? &dataStreamDesc : NULL;
    if (pDataStream == NULL)
    {
        nBufSize = Ptx_GetLastErrorMessage(NULL, 0);
        Ptx_GetLastErrorMessage(szErrorBuff, MIN(ARRAY_SIZE(szErrorBuff), nBufSize));
        _tprintf(_T("Failed to get embedded file data for \"%s\". %s (ErrorCode: 0x%08x).\n"), szName, szErrorBuff,
                 Ptx_GetLastError());
        fclose(pOutStream);
        return FALSE;
    }

    // Read from input stream and write to output file
    {
        unsigned char buffer[4096];
        size_t        nBytesRead;
        while ((nBytesRead = pDataStream->pfRead(pDataStream->m_handle, buffer, sizeof(buffer))) > 0)
        {
            fwrite(buffer, 1, nBytesRead, pOutStream);
        }
    }

    _tprintf(_T("Extracted: %s\n"), szName);
    fclose(pOutStream);

    return TRUE;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                      pInStream = NULL;
    TPtxSys_StreamDescriptor   descriptor;
    TPtxPdf_Document*          pInDoc       = NULL;
    TPtxPdf_FileReferenceList* pFileRefList = NULL;
    TPtxPdf_FileReference*     pFileRef     = NULL;
    TCHAR*                     szInPath;
    TCHAR*                     szOutputDir;

    setlocale(LC_CTYPE, "");


    // Check command line parameters
    if (argc < 3 || argc > 3)
    {
        return Usage();
    }

    // Initialize library
    Ptx_Initialize();

    // Set and check license key
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(Ptx_Sdk_Initialize(_T("<-- insert license key -->"), NULL),
                                      _T("Failed to set license key. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    szInPath    = argv[1];
    szOutputDir = argv[2];

    // Open input document
    pInStream = _tfopen(szInPath, _T("rb"));
    GOTO_CLEANUP_IF_NULL(pInStream, _T("Failed to open input file \"%s\".\n"), szInPath);
    PtxSysCreateFILEStreamDescriptor(&descriptor, pInStream, 0);
    pInDoc = PtxPdf_Document_Open(&descriptor, _T(""));
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInDoc, _T("Input file \"%s\" cannot be opened. %s (ErrorCode: 0x%08x).\n"),
                                     szInPath, szErrorBuff, Ptx_GetLastError());

    // Get all embedded files
    pFileRefList = PtxPdf_Document_GetAllEmbeddedFiles(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFileRefList, _T("Failed to get embedded files. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Extract each embedded file
    for (int i = 0; i < PtxPdf_FileReferenceList_GetCount(pFileRefList); i++)
    {
        pFileRef = PtxPdf_FileReferenceList_Get(pFileRefList, i);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFileRef, _T("Failed to get file reference. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(extractFile(pFileRef, szOutputDir),
                                          _T("Error extracting embedded file. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        if (pFileRef != NULL)
        {
            Ptx_Release(pFileRef);
            pFileRef = NULL;
        }
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pFileRef != NULL)
        Ptx_Release(pFileRef);
    if (pFileRefList != NULL)
        Ptx_Release(pFileRefList);
    if (pInDoc != NULL)
        PtxPdf_Document_Close(pInDoc);
    if (pInStream != NULL)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 