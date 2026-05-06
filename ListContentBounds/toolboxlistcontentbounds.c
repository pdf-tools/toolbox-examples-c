/****************************************************************************
 *
 * File:            toolboxlistcontentbounds.c
 *
 * Usage:           toolboxlistcontentbounds <inputPath>
 *                  
 * Title:           List bounds of page content
 *                  
 * Description:     For each page, list the page size and the rectangular
 *                  bounding box of all content on the page in PDF points
 *                  (1/72 inch).
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
#include <float.h>
#include "PdfTools_Toolbox.h"


#include <locale.h>
#if !defined(WIN32)
#define TCHAR char
#define _tcslen strlen
#define _tcscat strcat
#define _tcscpy strcpy
#define _tcsrchr strrchr
#define _tcstok strtok
#define _tcslen strlen
#define _tcscmp strcmp
#define _tcsftime strftime
#define _tcsncpy strncpy
#define _tmain main
#define _tfopen fopen
#define _ftprintf fprintf
#define _stprintf sprintf
#define _tstof atof
#define _tremove remove
#define _tprintf printf
#define _T(str) str
#endif


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
    printf("Usage: toolboxlistcontentbounds <inputPath>.\n");
    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

void enlargeBox(TPtxGeomReal_Rectangle* pBox, double dX, double dY)
{
    if (dX < pBox->dLeft)
        pBox->dLeft = dX;
    else if (dX > pBox->dRight)
        pBox->dRight = dX;
    if (dY < pBox->dBottom)
        pBox->dBottom = dY;
    else if (dY > pBox->dTop)
        pBox->dTop = dY;
}
void transformPoint(const TPtxGeomReal_AffineTransform* pTr, double dInX, double dInY, double* pdOutX, double* pdOutY)
{
    *pdOutX = pTr->dA * dInX + pTr->dC * dInY + pTr->dE;
    *pdOutY = pTr->dB * dInX + pTr->dD * dInY + pTr->dF;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                                pInStream       = NULL;
    TPtxSys_StreamDescriptor             descriptor;
    TPtxPdf_Document*                    pInDoc          = NULL;
    TPtxPdf_PageList*                    pInPageList     = NULL;
    TPtxPdf_Page*                        pPage           = NULL;
    TPtxPdfContent_Content*              pContent        = NULL;
    TPtxPdfContent_ContentExtractor*     pExtractor      = NULL;
    TPtxPdfContent_ContentExtractorIterator* pIterator   = NULL;
    TPtxPdfContent_ContentElement*       pContentElement = NULL;
    TCHAR*                               szInPath;


    setlocale(LC_CTYPE, "");


    // Check command line parameters
    if (argc < 2 || argc > 2)
    {
        return Usage();
    }

    // Initialize library
    Ptx_Initialize();

    // Set and check license key
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(Ptx_Sdk_Initialize(_T("insert-license-key-here"), NULL),
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

    // Get pages
    pInPageList = PtxPdf_Document_GetPages(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList,
                                     _T("Failed to get pages of the input document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Iterate over all pages
    for (int iPageNo = 0; iPageNo < PtxPdf_PageList_GetCount(pInPageList); iPageNo++)
    {
        TPtxGeomReal_Size      pageSize;
        TPtxGeomReal_Rectangle contentBox;

        pPage = PtxPdf_PageList_Get(pInPageList, iPageNo);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pPage,
                                         _T("Failed to get page %d. %s (ErrorCode: 0x%08x).\n"),
                                         iPageNo + 1, szErrorBuff, Ptx_GetLastError());

        // Print page size
        _tprintf(_T("Page %d\n"), iPageNo + 1);
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pPage, &pageSize),
                                          _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
        _tprintf(_T("  Size:\n"));
        _tprintf(_T("    Width: %g\n"), pageSize.dWidth);
        _tprintf(_T("    Height: %g\n"), pageSize.dHeight);

        // Initialize content bounding box
        contentBox.dLeft   = DBL_MAX;
        contentBox.dBottom = DBL_MAX;
        contentBox.dRight  = -DBL_MAX;
        contentBox.dTop    = -DBL_MAX;

        // Get page content
        pContent = PtxPdf_Page_GetContent(pPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContent,
                                         _T("Failed to get content from page %d. %s (ErrorCode: 0x%08x).\n"),
                                         iPageNo + 1, szErrorBuff, Ptx_GetLastError());

        pExtractor = PtxPdfContent_ContentExtractor_New(pContent);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pExtractor,
                                         _T("Failed to create content extractor. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        pIterator = PtxPdfContent_ContentExtractor_GetIterator(pExtractor);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pIterator,
                                         _T("Failed to get iterator. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);
        while ((pContentElement = PtxPdfContent_ContentExtractorIterator_GetValue(pIterator)) != NULL)
        {
            TPtxGeomReal_AffineTransform tr;
            TPtxGeomReal_Rectangle       box;
            double                       dOutX, dOutY;

            // Get element transform and bounding box
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentElement_GetTransform(pContentElement, &tr),
                                              _T("Failed to get element transform. %s (ErrorCode: 0x%08x).\n"),
                                              szErrorBuff, Ptx_GetLastError());
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentElement_GetBoundingBox(pContentElement, &box),
                                              _T("Failed to get element bounding box. %s (ErrorCode: 0x%08x).\n"),
                                              szErrorBuff, Ptx_GetLastError());

            // The location on the page is given by the transformed points
            // Transform all four corners and enlarge content box
            transformPoint(&tr, box.dLeft, box.dBottom, &dOutX, &dOutY);
            enlargeBox(&contentBox, dOutX, dOutY);

            transformPoint(&tr, box.dRight, box.dBottom, &dOutX, &dOutY);
            enlargeBox(&contentBox, dOutX, dOutY);

            transformPoint(&tr, box.dRight, box.dTop, &dOutX, &dOutY);
            enlargeBox(&contentBox, dOutX, dOutY);

            transformPoint(&tr, box.dLeft, box.dTop, &dOutX, &dOutY);
            enlargeBox(&contentBox, dOutX, dOutY);

            Ptx_Release(pContentElement);
            pContentElement = NULL;
            PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);
        }

        _tprintf(_T("  Content bounding box:\n"));
        _tprintf(_T("    Left: %g\n"), contentBox.dLeft);
        _tprintf(_T("    Bottom: %g\n"), contentBox.dBottom);
        _tprintf(_T("    Right: %g\n"), contentBox.dRight);
        _tprintf(_T("    Top: %g\n"), contentBox.dTop);

        if (pIterator != NULL)
        {
            Ptx_Release(pIterator);
            pIterator = NULL;
        }
        if (pExtractor != NULL)
        {
            Ptx_Release(pExtractor);
            pExtractor = NULL;
        }
        if (pContent != NULL)
        {
            Ptx_Release(pContent);
            pContent = NULL;
        }
        if (pPage != NULL)
        {
            Ptx_Release(pPage);
            pPage = NULL;
        }
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pContentElement != NULL)
        Ptx_Release(pContentElement);
    if (pIterator != NULL)
        Ptx_Release(pIterator);
    if (pExtractor != NULL)
        Ptx_Release(pExtractor);
    if (pContent != NULL)
        Ptx_Release(pContent);
    if (pPage != NULL)
        Ptx_Release(pPage);
    if (pInPageList != NULL)
        Ptx_Release(pInPageList);
    if (pInDoc != NULL)
        PtxPdf_Document_Close(pInDoc);
    if (pInStream != NULL)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 