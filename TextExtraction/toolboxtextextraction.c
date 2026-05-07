/****************************************************************************
 *
 * File:            toolboxtextextraction.c
 *
 * Usage:           toolboxtextextraction <inputPath>
 *                  Example: in.pdf
 *                  
 * Title:           Extract all text from PDF
 *                  
 * Description:     Write text from PDF page by page to console. Determine
 *                  heuristically if two text fragments belong to the same
 *                  word.
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
    printf("Usage: toolboxtextextraction <inputPath>.\n");
    printf("       Example: in.pdf\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

int writeText(TPtxPdfContent_Text* pText)
{
    int    iFragCount;
    TCHAR* szOutput    = NULL;
    size_t nOutputSize = 0;
    size_t nOutputCap  = 4096;

    szOutput = (TCHAR*)malloc(nOutputCap * sizeof(TCHAR));
    if (szOutput == NULL)
        return 1;
    szOutput[0] = _T('\0');

    iFragCount = PtxPdfContent_Text_GetCount(pText);
    if (iFragCount < 0)
    {
        free(szOutput);
        return 1;
    }

    for (int iFragment = 0; iFragment < iFragCount; iFragment++)
    {
        TPtxPdfContent_TextFragment* pCurrFrag = PtxPdfContent_Text_Get(pText, iFragment);
        if (pCurrFrag == NULL)
            continue;

        // Get the text of this fragment
        size_t nTextLen = PtxPdfContent_TextFragment_GetText(pCurrFrag, NULL, 0);
        if (nTextLen == 0)
        {
            Ptx_Release(pCurrFrag);
            continue;
        }
        TCHAR* szFragText = (TCHAR*)malloc(nTextLen * sizeof(TCHAR));
        if (szFragText == NULL)
        {
            Ptx_Release(pCurrFrag);
            continue;
        }
        PtxPdfContent_TextFragment_GetText(pCurrFrag, szFragText, nTextLen);

        BOOL bAddSpace = FALSE;

        if (iFragment > 0)
        {
            TPtxPdfContent_TextFragment* pLastFrag = PtxPdfContent_Text_Get(pText, iFragment - 1);
            if (pLastFrag != NULL)
            {
                // Compare formatting properties to decide if a space is needed
                double dCurrCharSpacing = PtxPdfContent_TextFragment_GetCharacterSpacing(pCurrFrag);
                double dLastCharSpacing = PtxPdfContent_TextFragment_GetCharacterSpacing(pLastFrag);
                double dCurrFontSize    = PtxPdfContent_TextFragment_GetFontSize(pCurrFrag);
                double dLastFontSize    = PtxPdfContent_TextFragment_GetFontSize(pLastFrag);
                double dCurrHorizScale  = PtxPdfContent_TextFragment_GetHorizontalScaling(pCurrFrag);
                double dLastHorizScale  = PtxPdfContent_TextFragment_GetHorizontalScaling(pLastFrag);
                double dCurrRise        = PtxPdfContent_TextFragment_GetRise(pCurrFrag);
                double dLastRise        = PtxPdfContent_TextFragment_GetRise(pLastFrag);
                double dCurrWordSpacing = PtxPdfContent_TextFragment_GetWordSpacing(pCurrFrag);
                double dLastWordSpacing = PtxPdfContent_TextFragment_GetWordSpacing(pLastFrag);

                if (dCurrCharSpacing != dLastCharSpacing || dCurrFontSize != dLastFontSize ||
                    dCurrHorizScale != dLastHorizScale || dCurrRise != dLastRise ||
                    dCurrWordSpacing != dLastWordSpacing)
                {
                    bAddSpace = TRUE;
                }
                else
                {
                    // Get bounding boxes and transforms to compare positions
                    TPtxGeomReal_Rectangle       currBBox, lastBBox;
                    TPtxGeomReal_AffineTransform currTransform, lastTransform;

                    if (PtxPdfContent_TextFragment_GetBoundingBox(pCurrFrag, &currBBox) &&
                        PtxPdfContent_TextFragment_GetBoundingBox(pLastFrag, &lastBBox) &&
                        PtxPdfContent_TextFragment_GetTransform(pCurrFrag, &currTransform) &&
                        PtxPdfContent_TextFragment_GetTransform(pLastFrag, &lastTransform))
                    {
                        // Transform bottom-left of current and bottom-right of last
                        // Current bottom-left: (currBBox.dLeft, currBBox.dBottom)
                        double dCurrBotLeftX =
                            currTransform.dA * currBBox.dLeft + currTransform.dC * currBBox.dBottom + currTransform.dE;
                        double dCurrBotLeftY =
                            currTransform.dB * currBBox.dLeft + currTransform.dD * currBBox.dBottom + currTransform.dF;

                        // Last bottom-right: (lastBBox.dRight, lastBBox.dBottom)
                        double dLastBotRightX =
                            lastTransform.dA * lastBBox.dRight + lastTransform.dC * lastBBox.dBottom + lastTransform.dE;
                        double dLastBotRightY =
                            lastTransform.dB * lastBBox.dRight + lastTransform.dD * lastBBox.dBottom + lastTransform.dF;

                        if (dLastBotRightX < dCurrBotLeftX - 0.7 * dCurrFontSize ||
                            dLastBotRightY < dCurrBotLeftY - 0.1 * dCurrFontSize ||
                            dCurrBotLeftY < dLastBotRightY - 0.1 * dCurrFontSize)
                        {
                            bAddSpace = TRUE;
                        }
                    }
                }
                Ptx_Release(pLastFrag);
            }
        }

        // Append text to output buffer
        size_t nFragLen  = _tcslen(szFragText);
        size_t nSpaceLen = bAddSpace ? 1 : 0;
        size_t nNeeded   = nOutputSize + nSpaceLen + nFragLen + 1;
        if (nNeeded > nOutputCap)
        {
            nOutputCap  = nNeeded * 2;
            TCHAR* pNew = (TCHAR*)realloc(szOutput, nOutputCap * sizeof(TCHAR));
            if (pNew == NULL)
            {
                free(szFragText);
                Ptx_Release(pCurrFrag);
                free(szOutput);
                return 1;
            }
            szOutput = pNew;
        }
        if (bAddSpace)
        {
            szOutput[nOutputSize] = _T(' ');
            nOutputSize++;
        }
        _tcscpy(szOutput + nOutputSize, szFragText);
        nOutputSize += nFragLen;

        free(szFragText);
        Ptx_Release(pCurrFrag);
    }

    _tprintf(_T("%s\n"), szOutput);
    free(szOutput);

    return 0;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                            pInStream = NULL;
    TPtxSys_StreamDescriptor         descriptor;
    TPtxPdf_Document*                pInDoc      = NULL;
    TPtxPdf_PageList*                pInPageList = NULL;
    TPtxPdf_Page*                    pPage       = NULL;
    TPtxPdfContent_Content*          pContent    = NULL;
    TPtxPdfContent_ContentExtractor* pExtractor  = NULL;
    TCHAR*                           szInPath;

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

    // Get page list
    pInPageList = PtxPdf_Document_GetPages(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList,
                                     _T("Failed to get the pages of the input document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Process each page
    for (int iPageNo = 0; iPageNo < PtxPdf_PageList_GetCount(pInPageList); iPageNo++)
    {
        _tprintf(_T("==========\n"));
        _tprintf(_T("Page: %d\n"), iPageNo + 1);
        _tprintf(_T("==========\n"));

        pPage = PtxPdf_PageList_Get(pInPageList, iPageNo);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pPage, _T("Failed to get page %d. %s (ErrorCode: 0x%08x).\n"), iPageNo + 1,
                                         szErrorBuff, Ptx_GetLastError());

        pContent = PtxPdf_Page_GetContent(pPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContent, _T("Failed to get content from page %d. %s (ErrorCode: 0x%08x).\n"),
                                         iPageNo + 1, szErrorBuff, Ptx_GetLastError());

        pExtractor = PtxPdfContent_ContentExtractor_New(pContent);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pExtractor,
                                         _T("Failed to create content extractor. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Set ungrouping to all
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
            PtxPdfContent_ContentExtractor_SetUngrouping(pExtractor, ePtxPdfContent_UngroupingSelection_All),
            _T("Failed to set ungrouping. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

        // Iterate over all content elements
        {
            TPtxPdfContent_ContentExtractorIterator* pIterator = NULL;
            TPtxPdfContent_ContentElement*           pElement  = NULL;

            pIterator = PtxPdfContent_ContentExtractor_GetIterator(pExtractor);
            GOTO_CLEANUP_IF_NULL(pIterator, _T("Failed to get iterator.\n"));
            PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);
            while ((pElement = PtxPdfContent_ContentExtractorIterator_GetValue(pIterator)) != NULL)
            {
                TPtxPdfContent_ContentElementType iType = PtxPdfContent_ContentElement_GetType(pElement);
                if (iType == ePtxPdfContent_ContentElementType_TextElement)
                {
                    TPtxPdfContent_Text* pText =
                        PtxPdfContent_TextElement_GetText((TPtxPdfContent_TextElement*)pElement);
                    if (pText != NULL)
                    {
                        if (writeText(pText) != 0)
                        {
                            Ptx_Release(pText);
                            Ptx_Release(pElement);
                            Ptx_Release(pIterator);
                            goto cleanup;
                        }
                        Ptx_Release(pText);
                    }
                }
                Ptx_Release(pElement);
                pElement = NULL;
                PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);
            }
            Ptx_Release(pIterator);
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
    if (pInStream)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 