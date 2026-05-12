/****************************************************************************
 *
 * File:            toolboxmergewithoutlines.c
 *
 * Usage:           toolboxmergewithoutlines <inputPath> [<inputPath2> ...] <outputPath>
 *                  Example: in1.pdf in2.pdf out.pdf
 *                  
 * Title:           Merge multiple PDFs with outlines
 *                  
 * Description:     Merge several PDF documents to one, while creating an
 *                  outline item for each input document.
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
    printf("Usage: toolboxmergewithoutlines <inputPath> [<inputPath2> ...] <outputPath>.\n");
    printf("       Example: in1.pdf in2.pdf out.pdf\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

/**
 * Get the file name from a path string.
 */
const TCHAR* getFileName(const TCHAR* szPath)
{
    const TCHAR* szSlash     = _tcsrchr(szPath, _T('/'));
    const TCHAR* szBackslash = _tcsrchr(szPath, _T('\\'));
    const TCHAR* szLast      = NULL;
    if (szSlash != NULL && szBackslash != NULL)
        szLast = (szSlash > szBackslash) ? szSlash : szBackslash;
    else if (szSlash != NULL)
        szLast = szSlash;
    else if (szBackslash != NULL)
        szLast = szBackslash;
    if (szLast != NULL)
        return szLast + 1;
    return szPath;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                          pInStream = NULL;
    TPtxSys_StreamDescriptor       inDescriptor;
    TPtxPdf_Document*              pInDoc     = NULL;
    FILE*                          pOutStream = NULL;
    TPtxSys_StreamDescriptor       outDescriptor;
    TPtxPdf_Document*              pOutDoc          = NULL;
    TPtxPdf_PageList*              pInPageList      = NULL;
    TPtxPdf_PageList*              pOutPageList     = NULL;
    TPtxPdf_PageList*              pCopiedPages     = NULL;
    TPtxPdf_PageCopyOptions*       pCopyOptions     = NULL;
    TPtxPdfNav_OutlineCopyOptions* pOutlineCopyOpts = NULL;
    TPtxPdfNav_OutlineItemList*    pOutOutline      = NULL;
    TCHAR*                         szOutPath;

    setlocale(LC_CTYPE, "");


    // Check command line parameters
    if (argc < 3)
    {
        return Usage();
    }

    // Initialize library
    Ptx_Initialize();

    // Set and check license key
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(Ptx_Sdk_Initialize(_T("<-- insert license key -->"), NULL),
                                      _T("Failed to set license key. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    szOutPath = argv[argc - 1];

    // Create output document
    pOutStream = _tfopen(szOutPath, _T("wb+"));
    GOTO_CLEANUP_IF_NULL(pOutStream, _T("Failed to open output file \"%s\".\n"), szOutPath);
    PtxSysCreateFILEStreamDescriptor(&outDescriptor, pOutStream, 0);
    pOutDoc = PtxPdf_Document_Create(&outDescriptor, NULL, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutDoc, _T("Output file \"%s\" cannot be created. %s (ErrorCode: 0x%08x).\n"),
                                     szOutPath, szErrorBuff, Ptx_GetLastError());

    // Define page copy options, skip outline
    pCopyOptions = PtxPdf_PageCopyOptions_New();
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pCopyOptions, _T("Failed to create page copy options. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageCopyOptions_SetCopyOutlineItems(pCopyOptions, FALSE),
                                      _T("Failed to set copy outline items. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Define outline copy options
    pOutlineCopyOpts = PtxPdfNav_OutlineCopyOptions_New();
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutlineCopyOpts,
                                     _T("Failed to create outline copy options. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Get output page list
    pOutPageList = PtxPdf_Document_GetPages(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageList,
                                     _T("Failed to get pages of the output document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Get the output document outline
    pOutOutline = PtxPdf_Document_GetOutline(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutOutline,
                                     _T("Failed to get outline of the output document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Merge input documents
    for (int i = 1; i < argc - 1; i++)
    {
        TCHAR*                      szInPath      = argv[i];
        TPtxPdf_Metadata*           pMetadata     = NULL;
        TPtxPdf_Page*               pFirstPage    = NULL;
        TPtxPdfNav_Destination*     pDestination  = NULL;
        TPtxPdfNav_OutlineItem*     pOutlineItem  = NULL;
        TPtxPdfNav_OutlineItemList* pInOutline    = NULL;
        TPtxPdfNav_OutlineItemList* pItemChildren = NULL;
        TPtxGeomReal_Size           pageSize;
        TCHAR*                      szTitle = NULL;

        // Open input document
        pInStream = _tfopen(szInPath, _T("rb"));
        GOTO_CLEANUP_IF_NULL(pInStream, _T("Failed to open input file \"%s\".\n"), szInPath);
        PtxSysCreateFILEStreamDescriptor(&inDescriptor, pInStream, 0);
        pInDoc = PtxPdf_Document_Open(&inDescriptor, _T(""));
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInDoc, _T("Input file \"%s\" cannot be opened. %s (ErrorCode: 0x%08x).\n"),
                                         szInPath, szErrorBuff, Ptx_GetLastError());

        // Copy all pages and append to output document
        pInPageList = PtxPdf_Document_GetPages(pInDoc);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList,
                                         _T("Failed to get pages of the input document. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        pCopiedPages = PtxPdf_PageList_Copy(pOutDoc, pInPageList, pCopyOptions);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pCopiedPages, _T("Failed to copy pages. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_AddRange(pOutPageList, pCopiedPages),
                                          _T("Failed to add page range. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        // Get title from metadata, fall back to file name
        pMetadata = PtxPdf_Document_GetMetadata(pInDoc);
        if (pMetadata != NULL)
        {
            size_t nTitleLen = PtxPdf_Metadata_GetTitle(pMetadata, NULL, 0);
            if (nTitleLen > 0)
            {
                szTitle = (TCHAR*)malloc(nTitleLen * sizeof(TCHAR));
                if (szTitle != NULL)
                    PtxPdf_Metadata_GetTitle(pMetadata, szTitle, nTitleLen);
            }
        }
        if (szTitle == NULL || _tcslen(szTitle) == 0)
        {
            if (szTitle != NULL)
                free(szTitle);
            const TCHAR* szBaseName = getFileName(szInPath);
            szTitle                 = (TCHAR*)malloc((_tcslen(szBaseName) + 1) * sizeof(TCHAR));
            if (szTitle != NULL)
                _tcscpy(szTitle, szBaseName);
        }

        // Create outline item for this document
        pFirstPage = PtxPdf_PageList_Get(pCopiedPages, 0);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFirstPage, _T("Failed to get first copied page. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pFirstPage, &pageSize),
                                          _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        {
            double dLeft = 0;
            double dTop  = pageSize.dHeight;
            pDestination = (TPtxPdfNav_Destination*)PtxPdfNav_LocationZoomDestination_Create(pOutDoc, pFirstPage,
                                                                                             &dLeft, &dTop, NULL);
        }
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pDestination, _T("Failed to create destination. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        pOutlineItem = PtxPdfNav_OutlineItem_Create(pOutDoc, szTitle, pDestination);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutlineItem, _T("Failed to create outline item. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfNav_OutlineItemList_Add(pOutOutline, pOutlineItem),
                                          _T("Failed to add outline item. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        // Add outline items from input document as children
        pInOutline = PtxPdf_Document_GetOutline(pInDoc);
        if (pInOutline != NULL)
        {
            pItemChildren = PtxPdfNav_OutlineItem_GetChildren(pOutlineItem);
            if (pItemChildren != NULL)
            {
                int nInOutlineCount = PtxPdfNav_OutlineItemList_GetCount(pInOutline);
                for (int j = 0; j < nInOutlineCount; j++)
                {
                    TPtxPdfNav_OutlineItem* pInItem = PtxPdfNav_OutlineItemList_Get(pInOutline, j);
                    if (pInItem != NULL)
                    {
                        TPtxPdfNav_OutlineItem* pCopiedItem =
                            PtxPdfNav_OutlineItem_Copy(pOutDoc, pInItem, pOutlineCopyOpts);
                        if (pCopiedItem != NULL)
                        {
                            PtxPdfNav_OutlineItemList_Add(pItemChildren, pCopiedItem);
                            Ptx_Release(pCopiedItem);
                        }
                        Ptx_Release(pInItem);
                    }
                }
                Ptx_Release(pItemChildren);
                pItemChildren = NULL;
            }
            Ptx_Release(pInOutline);
        }

        // Cleanup per-iteration resources
        if (szTitle != NULL)
        {
            free(szTitle);
            szTitle = NULL;
        }
        if (pOutlineItem != NULL)
        {
            Ptx_Release(pOutlineItem);
            pOutlineItem = NULL;
        }
        if (pDestination != NULL)
        {
            Ptx_Release(pDestination);
            pDestination = NULL;
        }
        if (pFirstPage != NULL)
        {
            Ptx_Release(pFirstPage);
            pFirstPage = NULL;
        }
        if (pCopiedPages != NULL)
        {
            Ptx_Release(pCopiedPages);
            pCopiedPages = NULL;
        }
        if (pInPageList != NULL)
        {
            Ptx_Release(pInPageList);
            pInPageList = NULL;
        }
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Document_Close(pInDoc),
                                          _T("Failed to close input document. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
        pInDoc = NULL;
        fclose(pInStream);
        pInStream = NULL;
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pCopiedPages != NULL)
        Ptx_Release(pCopiedPages);
    if (pOutOutline != NULL)
        Ptx_Release(pOutOutline);
    if (pOutPageList != NULL)
        Ptx_Release(pOutPageList);
    if (pInPageList != NULL)
        Ptx_Release(pInPageList);
    if (pOutlineCopyOpts != NULL)
        Ptx_Release(pOutlineCopyOpts);
    if (pCopyOptions != NULL)
        Ptx_Release(pCopyOptions);
    if (pOutDoc != NULL)
        PtxPdf_Document_Close(pOutDoc);
    if (pOutStream != NULL)
        fclose(pOutStream);
    if (pInDoc != NULL)
        PtxPdf_Document_Close(pInDoc);
    if (pInStream)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 