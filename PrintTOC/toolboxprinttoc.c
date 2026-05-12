/****************************************************************************
 *
 * File:            toolboxprinttoc.c
 *
 * Usage:           toolboxprinttoc <inputPath>
 *                  
 * Title:           Print a table of content
 *                  
 * Description:     Print a formatted table of content from the document
 *                  outline.
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
    printf("Usage: toolboxprinttoc <inputPath>.\n");
    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

void printOutlineItems(TPtxPdfNav_OutlineItemList* pOutlineItems, const TCHAR* szIndentation, TPtxPdf_Document* pDoc);

/* Find the 1-based page number of a page object by iterating the document's page list.
   Returns 0 if the page was not found. */
int findPageNumber(TPtxPdf_Document* pDoc, TPtxPdf_Page* pTargetPage)
{
    TPtxPdf_PageList* pPageList = PtxPdf_Document_GetPages(pDoc);
    int               iCount;
    int               i;

    if (pPageList == NULL || pTargetPage == NULL)
        return 0;

    iCount = PtxPdf_PageList_GetCount(pPageList);
    for (i = 0; i < iCount; i++)
    {
        TPtxPdf_Page* pPage = PtxPdf_PageList_Get(pPageList, i);
        if (pPage == pTargetPage)
        {
            Ptx_Release(pPage);
            Ptx_Release(pPageList);
            return i + 1;
        }
        if (pPage != NULL)
            Ptx_Release(pPage);
    }

    Ptx_Release(pPageList);
    return 0;
}
void printOutlineItem(TPtxPdfNav_OutlineItem* pItem, const TCHAR* szIndentation, TPtxPdf_Document* pDoc)
{
    TCHAR  szTitle[512]       = {'\0'};
    TCHAR  szChildIndent[256] = {'\0'};
    size_t nTitleSize;
    int    iPageNumber;

    // Get title
    nTitleSize = PtxPdfNav_OutlineItem_GetTitle(pItem, NULL, 0);
    if (nTitleSize > 0 && nTitleSize <= ARRAY_SIZE(szTitle))
    {
        PtxPdfNav_OutlineItem_GetTitle(pItem, szTitle, nTitleSize);
    }

    _tprintf(_T("%s%s"), szIndentation, szTitle);

    // Get destination
    {
        TPtxPdfNav_Destination* pDest = PtxPdfNav_OutlineItem_GetDestination(pItem);
        if (pDest != NULL)
        {
            // Get the direct destination target, then the page
            TPtxPdfNav_DirectDestination* pDirectDest = PtxPdfNav_Destination_GetTarget(pDest);
            if (pDirectDest != NULL)
            {
                TPtxPdf_Page* pPage = PtxPdfNav_DirectDestination_GetPage(pDirectDest);
                if (pPage != NULL)
                {
                    iPageNumber = findPageNumber(pDoc, pPage);
                    if (iPageNumber > 0)
                    {
                        // Print dots between title and page number
                        TCHAR szPageNum[16] = {'\0'};
                        int   nIndentLen, nTitleLen, nPageNumLen, nDotsLen;
                        TCHAR szDots[256] = {'\0'};
                        int   k;

                        _stprintf(szPageNum, _T("%d"), iPageNumber);
                        nIndentLen  = (int)_tcslen(szIndentation);
                        nTitleLen   = (int)_tcslen(szTitle);
                        nPageNumLen = (int)_tcslen(szPageNum);
                        nDotsLen    = 78 - nIndentLen - nTitleLen - nPageNumLen;
                        if (nDotsLen < 1)
                            nDotsLen = 1;

                        for (k = 0; k < nDotsLen && k < (int)(ARRAY_SIZE(szDots) - 1); k++)
                            szDots[k] = '.';
                        szDots[nDotsLen < (int)(ARRAY_SIZE(szDots) - 1) ? nDotsLen : (int)(ARRAY_SIZE(szDots) - 1)] =
                            '\0';

                        _tprintf(_T(" %s %d"), szDots, iPageNumber);
                    }
                    Ptx_Release(pPage);
                }
                Ptx_Release(pDirectDest);
            }
            Ptx_Release(pDest);
        }
    }
    _tprintf(_T("\n"));

    // Print children
    {
        TPtxPdfNav_OutlineItemList* pChildren = PtxPdfNav_OutlineItem_GetChildren(pItem);
        if (pChildren != NULL)
        {
            _tcscpy(szChildIndent, szIndentation);
            _tcscat(szChildIndent, _T("  "));
            printOutlineItems(pChildren, szChildIndent, pDoc);
            Ptx_Release(pChildren);
        }
    }
}
void printOutlineItems(TPtxPdfNav_OutlineItemList* pOutlineItems, const TCHAR* szIndentation, TPtxPdf_Document* pDoc)
{
    int iCount = PtxPdfNav_OutlineItemList_GetCount(pOutlineItems);
    int i;
    for (i = 0; i < iCount; i++)
    {
        TPtxPdfNav_OutlineItem* pItem = PtxPdfNav_OutlineItemList_Get(pOutlineItems, i);
        if (pItem != NULL)
        {
            printOutlineItem(pItem, szIndentation, pDoc);
            Ptx_Release(pItem);
        }
    }
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                       pInStream = NULL;
    TPtxSys_StreamDescriptor    descriptor;
    TPtxPdf_Document*           pInDoc   = NULL;
    TPtxPdfNav_OutlineItemList* pOutline = NULL;
    TCHAR*                      szInPath;

    setlocale(LC_CTYPE, "");


    // Check command line parameters
    if (argc < 2 || argc > 2)
    {
        return Usage();
    }

    // Initialize library
    Ptx_Initialize();

    // Set and check license key
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(Ptx_Sdk_Initialize(_T("<-- insert license key -->"), NULL),
                                      _T("Failed to set license key. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    szInPath = argv[1];

    // Open input document
    pInStream = _tfopen(szInPath, _T("rb"));
    GOTO_CLEANUP_IF_NULL(pInStream, _T("Failed to open input file \"%s\".\n"), szInPath);
    PtxSysCreateFILEStreamDescriptor(&descriptor, pInStream, 0);
    pInDoc = PtxPdf_Document_Open(&descriptor, _T(""));
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInDoc, _T("Input file \"%s\" cannot be opened. %s (ErrorCode: 0x%08x).\n"),
                                     szInPath, szErrorBuff, Ptx_GetLastError());

    // Get the document outline
    pOutline = PtxPdf_Document_GetOutline(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutline, _T("Failed to get document outline. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Print the outline items
    printOutlineItems(pOutline, _T(""), pInDoc);

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pOutline != NULL)
        Ptx_Release(pOutline);
    if (pInDoc != NULL)
        PtxPdf_Document_Close(pInDoc);
    if (pInStream != NULL)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 