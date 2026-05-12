/****************************************************************************
 *
 * File:            toolboxmergeandcreatetableofcontents.c
 *
 * Usage:           toolboxmergeandcreatetableofcontents <inputPath> [<inputPath2> ...] <outputPath>
 *                  Example: in1.pdf in2.pdf out.pdf
 *                  
 * Title:           Merge multiple PDFs and create a table of contents page
 *                  
 * Description:     Merge several PDF documents to one and create a table of
 *                  contents page.
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
#include <math.h>
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

#define GOTO_CLEANUP_IF_ZERO_PRINT_ERROR(outDouble, ...)                                  \
    do                                                                                    \
    {                                                                                     \
        if ((outDouble) == 0.0)                                                           \
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
    printf("Usage: toolboxmergeandcreatetableofcontents <inputPath> [<inputPath2> ...] <outputPath>.\n");
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
 * Get the file name without extension from a path string.
 */
void getFileNameWithoutExtension(const TCHAR* szPath, TCHAR* szOut, size_t nOutSize)
{
    const TCHAR* szSlash     = _tcsrchr(szPath, _T('/'));
    const TCHAR* szBackslash = _tcsrchr(szPath, _T('\\'));
    const TCHAR* szStart     = szPath;
    const TCHAR* szDot;

    if (szSlash != NULL && szBackslash != NULL)
        szStart = (szSlash > szBackslash) ? szSlash + 1 : szBackslash + 1;
    else if (szSlash != NULL)
        szStart = szSlash + 1;
    else if (szBackslash != NULL)
        szStart = szBackslash + 1;

    szDot = _tcsrchr(szStart, _T('.'));
    if (szDot != NULL)
    {
        size_t nLen = (size_t)(szDot - szStart);
        if (nLen >= nOutSize)
            nLen = nOutSize - 1;
        _tcsncpy(szOut, szStart, nLen);
        szOut[nLen] = _T('\0');
    }
    else
    {
        _tcsncpy(szOut, szStart, nOutSize - 1);
        szOut[nOutSize - 1] = _T('\0');
    }
}
/* Structure to hold per-input-document data */
typedef struct
{
    TCHAR             szTitle[256];
    TPtxPdf_PageList* pCopiedPages;
} TDocEntry;
int addPageNumber(TPtxPdf_Document* pOutDoc, TPtxPdf_Page* pPage, TPtxPdfContent_Font* pFont, int iPageNumber)
{
    TPtxPdfContent_Content*          pContent       = NULL;
    TPtxPdfContent_ContentGenerator* pGenerator     = NULL;
    TPtxPdfContent_Text*             pText          = NULL;
    TPtxPdfContent_TextGenerator*    pTextGenerator = NULL;
    TPtxGeomReal_Size                size;
    TPtxGeomReal_Point               position;
    double                           dTextWidth;
    TCHAR                            szStampText[64];

    pContent = PtxPdf_Page_GetContent(pPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContent, _T("Failed to get page content. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    pGenerator = PtxPdfContent_ContentGenerator_New(pContent, FALSE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pGenerator, _T("Failed to create content generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    pText = PtxPdfContent_Text_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pText, _T("Failed to create text object. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    pTextGenerator = PtxPdfContent_TextGenerator_New(pText, pFont, 8, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTextGenerator, _T("Failed to create text generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pPage, &size),
                                      _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    _stprintf(szStampText, _T("Page %d"), iPageNumber);

    dTextWidth = PtxPdfContent_TextGenerator_GetWidth(pTextGenerator, szStampText);

    position.dX = (size.dWidth / 2.0) - (dTextWidth / 2.0);
    position.dY = 10.0;

    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_MoveTo(pTextGenerator, &position),
                                      _T("Failed to move to position. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_Show(pTextGenerator, szStampText),
                                      _T("Failed to show text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    PtxPdfContent_TextGenerator_Close(pTextGenerator);
    pTextGenerator = NULL;

    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_PaintText(pGenerator, pText),
                                      _T("Failed to paint text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

cleanup:
    if (pTextGenerator != NULL)
        PtxPdfContent_TextGenerator_Close(pTextGenerator);
    if (pText != NULL)
        Ptx_Release(pText);
    if (pGenerator != NULL)
        PtxPdfContent_ContentGenerator_Close(pGenerator);
    if (pContent != NULL)
        Ptx_Release(pContent);

    return iReturnValue;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                            pInStream = NULL;
    TPtxSys_StreamDescriptor         inDescriptor;
    TPtxPdf_Document*                pInDoc     = NULL;
    FILE*                            pOutStream = NULL;
    TPtxSys_StreamDescriptor         outDescriptor;
    TPtxPdf_Document*                pOutDoc      = NULL;
    TPtxPdf_PageList*                pInPageList  = NULL;
    TPtxPdf_PageList*                pOutPageList = NULL;
    TPtxPdf_PageCopyOptions*         pCopyOptions = NULL;
    TPtxPdfContent_Font*             pFont        = NULL;
    TPtxPdfContent_Font*             pTocFont     = NULL;
    TPtxPdf_Page*                    pTocPage     = NULL;
    TPtxPdfContent_Content*          pTocContent  = NULL;
    TPtxPdfContent_ContentGenerator* pTocCG       = NULL;
    TPtxPdfContent_Text*             pTocText     = NULL;
    TPtxPdfContent_TextGenerator*    pTocTG       = NULL;
    TCHAR*                           szOutPath;
    TDocEntry*                       pEntries = NULL;
    int                              nInputCount;
    int                              iPageNumber;

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

    szOutPath   = argv[argc - 1];
    nInputCount = argc - 2;

    // Allocate entry array
    pEntries = (TDocEntry*)calloc(nInputCount, sizeof(TDocEntry));
    GOTO_CLEANUP_IF_NULL(pEntries, _T("Failed to allocate memory.\n"));

    // Create output document
    pOutStream = _tfopen(szOutPath, _T("wb+"));
    GOTO_CLEANUP_IF_NULL(pOutStream, _T("Failed to open output file \"%s\".\n"), szOutPath);
    PtxSysCreateFILEStreamDescriptor(&outDescriptor, pOutStream, 0);
    pOutDoc = PtxPdf_Document_Create(&outDescriptor, NULL, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutDoc, _T("Output file \"%s\" cannot be created. %s (ErrorCode: 0x%08x).\n"),
                                     szOutPath, szErrorBuff, Ptx_GetLastError());

    // Create embedded font in output document
    pFont = PtxPdfContent_Font_CreateFromSystem(pOutDoc, _T("Arial"), _T(""), TRUE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFont, _T("Failed to create font. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Define page copy options
    pCopyOptions = PtxPdf_PageCopyOptions_New();
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pCopyOptions, _T("Failed to create page copy options. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Page number counter (TOC is page 1, content starts at page 2)
    iPageNumber = 2;

    // Copy all input documents' pages
    for (int i = 0; i < nInputCount; i++)
    {
        TCHAR*            szInPath  = argv[i + 1];
        TPtxPdf_Metadata* pMetadata = NULL;
        TCHAR*            szTitle   = NULL;

        // Open input document
        pInStream = _tfopen(szInPath, _T("rb"));
        GOTO_CLEANUP_IF_NULL(pInStream, _T("Failed to open input file \"%s\".\n"), szInPath);
        PtxSysCreateFILEStreamDescriptor(&inDescriptor, pInStream, 0);
        pInDoc = PtxPdf_Document_Open(&inDescriptor, _T(""));
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInDoc, _T("Input file \"%s\" cannot be opened. %s (ErrorCode: 0x%08x).\n"),
                                         szInPath, szErrorBuff, Ptx_GetLastError());

        // Copy all pages
        pInPageList = PtxPdf_Document_GetPages(pInDoc);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList,
                                         _T("Failed to get pages of the input document. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        pEntries[i].pCopiedPages = PtxPdf_PageList_Copy(pOutDoc, pInPageList, pCopyOptions);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pEntries[i].pCopiedPages,
                                         _T("Failed to copy pages. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                         Ptx_GetLastError());

        // Add page numbers to copied pages
        {
            int nCopiedCount = PtxPdf_PageList_GetCount(pEntries[i].pCopiedPages);
            for (int j = 0; j < nCopiedCount; j++)
            {
                TPtxPdf_Page* pCopiedPage = PtxPdf_PageList_Get(pEntries[i].pCopiedPages, j);
                if (pCopiedPage != NULL)
                {
                    if (addPageNumber(pOutDoc, pCopiedPage, pFont, iPageNumber++) != 0)
                    {
                        Ptx_Release(pCopiedPage);
                        goto cleanup;
                    }
                    Ptx_Release(pCopiedPage);
                }
            }
        }

        // Get title from metadata, fall back to file name without extension
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
            TCHAR szName[256];
            getFileNameWithoutExtension(szInPath, szName, 256);
            szTitle = (TCHAR*)malloc((_tcslen(szName) + 1) * sizeof(TCHAR));
            if (szTitle != NULL)
                _tcscpy(szTitle, szName);
        }
        _tcsncpy(pEntries[i].szTitle, szTitle ? szTitle : _T(""), 255);
        pEntries[i].szTitle[255] = _T('\0');

        if (szTitle != NULL)
            free(szTitle);
        Ptx_Release(pInPageList);
        pInPageList = NULL;
        PtxPdf_Document_Close(pInDoc);
        pInDoc = NULL;
        fclose(pInStream);
        pInStream = NULL;
    }

    // Create table of contents page
    {
        TPtxGeomReal_Size  firstPageSize;
        TPtxGeomReal_Size  tocSize;
        double             dBorder = 30.0;
        double             dTextWidth;
        double             dChapterSize = 24.0;
        double             dTitleSize   = 12.0;
        TPtxGeomReal_Point location;
        int                iTocPageNumber = 2;

        // Get size of first copied page
        TPtxPdf_Page* pFirstPage = PtxPdf_PageList_Get(pEntries[0].pCopiedPages, 0);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFirstPage, _T("Failed to get first page. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pFirstPage, &firstPageSize),
                                          _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
        Ptx_Release(pFirstPage);

        tocSize.dWidth  = firstPageSize.dWidth;
        tocSize.dHeight = firstPageSize.dHeight;

        // Create TOC page
        pTocPage = PtxPdf_Page_Create(pOutDoc, &tocSize);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTocPage, _T("Failed to create TOC page. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Create font for TOC
        pTocFont = PtxPdfContent_Font_CreateFromSystem(pOutDoc, _T("Arial"), NULL, TRUE);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTocFont, _T("Failed to create TOC font. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        dTextWidth = tocSize.dWidth - 2 * dBorder;

        location.dX = dBorder;
        location.dY = tocSize.dHeight - dBorder - dChapterSize;

        pTocContent = PtxPdf_Page_GetContent(pTocPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTocContent, _T("Failed to get TOC content. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        pTocCG = PtxPdfContent_ContentGenerator_New(pTocContent, FALSE);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTocCG,
                                         _T("Failed to create TOC content generator. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        pTocText = PtxPdfContent_Text_Create(pOutDoc);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTocText, _T("Failed to create TOC text. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Create text generator with chapter title font size
        pTocTG = PtxPdfContent_TextGenerator_New(pTocText, pTocFont, dChapterSize, &location);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTocTG, _T("Failed to create TOC text generator. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Show chapter title
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_ShowLine(pTocTG, _T("Table of Contents")),
                                          _T("Failed to show TOC title. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        // Advance vertical position
        location.dY -= 1.7 * dChapterSize;

        // Set font size for entries
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_SetFontSize(pTocTG, dTitleSize),
                                          _T("Failed to set font size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        // Iterate over all copied page ranges
        for (int i = 0; i < nInputCount; i++)
        {
            TCHAR                    szPageNum[32];
            double                   dPageNumWidth;
            double                   dTitleWidth;
            double                   dDotWidth;
            int                      nDots;
            TCHAR                    szEntryLine[1024];
            TPtxGeomReal_Point       pageNumPos;
            TPtxGeomReal_Rectangle   linkRect;
            TPtxPdf_Page*            pTargetPage = NULL;
            TPtxGeomReal_Size        targetSize;
            TPtxPdfNav_Destination*  pDest  = NULL;
            TPtxPdfNav_InternalLink* pLink  = NULL;
            TPtxPdfNav_LinkList*     pLinks = NULL;
            double                   dFontDescent;
            double                   dFontAscent;

            _stprintf(szPageNum, _T("%d"), iTocPageNumber);

            dPageNumWidth = PtxPdfContent_TextGenerator_GetWidth(pTocTG, szPageNum);
            dTitleWidth   = PtxPdfContent_TextGenerator_GetWidth(pTocTG, pEntries[i].szTitle);
            dDotWidth     = PtxPdfContent_TextGenerator_GetWidth(pTocTG, _T("."));
            nDots         = (int)floor((dTextWidth - dTitleWidth - dPageNumWidth) / dDotWidth);
            if (nDots < 0)
                nDots = 0;

            // Build entry line: title + dots
            _tcscpy(szEntryLine, pEntries[i].szTitle);
            for (int d = 0; d < nDots && _tcslen(szEntryLine) < 1020; d++)
                _tcscat(szEntryLine, _T("."));

            // Move to current location and show entry
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_MoveTo(pTocTG, &location),
                                              _T("Failed to move to position. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                              Ptx_GetLastError());
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_Show(pTocTG, szEntryLine),
                                              _T("Failed to show entry. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                              Ptx_GetLastError());

            // Show page number
            pageNumPos.dX = tocSize.dWidth - dBorder - dPageNumWidth;
            pageNumPos.dY = location.dY;
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_MoveTo(pTocTG, &pageNumPos),
                                              _T("Failed to move to page number position. %s (ErrorCode: 0x%08x).\n"),
                                              szErrorBuff, Ptx_GetLastError());
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_Show(pTocTG, szPageNum),
                                              _T("Failed to show page number. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                              Ptx_GetLastError());

            // Create link rectangle
            dFontDescent = PtxPdfContent_Font_GetDescent(pTocFont);
            dFontAscent  = PtxPdfContent_Font_GetAscent(pTocFont);

            linkRect.dLeft   = dBorder;
            linkRect.dBottom = location.dY + dFontDescent * dTitleSize;
            linkRect.dRight  = dBorder + dTextWidth;
            linkRect.dTop    = location.dY + dFontAscent * dTitleSize;

            // Create destination to the first page of this range
            pTargetPage = PtxPdf_PageList_Get(pEntries[i].pCopiedPages, 0);
            if (pTargetPage != NULL)
            {
                PtxPdf_Page_GetSize(pTargetPage, &targetSize);
                {
                    double dLeft = 0;
                    double dTop  = targetSize.dHeight;
                    pDest = (TPtxPdfNav_Destination*)PtxPdfNav_LocationZoomDestination_Create(pOutDoc, pTargetPage,
                                                                                              &dLeft, &dTop, NULL);
                }
                if (pDest != NULL)
                {
                    pLink = PtxPdfNav_InternalLink_Create(pOutDoc, &linkRect, pDest);
                    if (pLink != NULL)
                    {
                        pLinks = PtxPdf_Page_GetLinks(pTocPage);
                        if (pLinks != NULL)
                        {
                            PtxPdfNav_LinkList_Add(pLinks, (TPtxPdfNav_Link*)pLink);
                            Ptx_Release(pLinks);
                        }
                        Ptx_Release(pLink);
                    }
                    Ptx_Release(pDest);
                }
                Ptx_Release(pTargetPage);
            }

            // Advance location
            location.dY -= 1.8 * dTitleSize;
            iTocPageNumber += PtxPdf_PageList_GetCount(pEntries[i].pCopiedPages);
        }

        // Close text generator
        PtxPdfContent_TextGenerator_Close(pTocTG);
        pTocTG = NULL;

        // Paint the generated text
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_PaintText(pTocCG, pTocText),
                                          _T("Failed to paint TOC text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        PtxPdfContent_ContentGenerator_Close(pTocCG);
        pTocCG = NULL;

        // Add page number to TOC page
        if (addPageNumber(pOutDoc, pTocPage, pFont, 1) != 0)
            goto cleanup;
    }

    // Add pages to output document
    pOutPageList = PtxPdf_Document_GetPages(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageList, _T("Failed to get output pages. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Add TOC page first
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_Add(pOutPageList, pTocPage),
                                      _T("Failed to add TOC page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Add all copied pages
    for (int i = 0; i < nInputCount; i++)
    {
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_AddRange(pOutPageList, pEntries[i].pCopiedPages),
                                          _T("Failed to add pages. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pTocTG != NULL)
        PtxPdfContent_TextGenerator_Close(pTocTG);
    if (pTocText != NULL)
        Ptx_Release(pTocText);
    if (pTocCG != NULL)
        PtxPdfContent_ContentGenerator_Close(pTocCG);
    if (pTocContent != NULL)
        Ptx_Release(pTocContent);
    if (pTocPage != NULL)
        Ptx_Release(pTocPage);
    if (pTocFont != NULL)
        Ptx_Release(pTocFont);
    if (pEntries != NULL)
    {
        for (int i = 0; i < nInputCount; i++)
        {
            if (pEntries[i].pCopiedPages != NULL)
                Ptx_Release(pEntries[i].pCopiedPages);
        }
        free(pEntries);
    }
    if (pOutPageList != NULL)
        Ptx_Release(pOutPageList);
    if (pInPageList != NULL)
        Ptx_Release(pInPageList);
    if (pCopyOptions != NULL)
        Ptx_Release(pCopyOptions);
    if (pFont != NULL)
        Ptx_Release(pFont);
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