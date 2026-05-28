/****************************************************************************
 *
 * File:            toolboxaddlinenumbers.c
 *
 * Usage:           toolboxaddlinenumbers <inputPath> <outputPath>
 *                  Example: in.pdf out.pdf
 *                  
 * Title:           Add line numbers to PDF
 *                  
 * Description:     Add a line number in front of each line that contains
 *                  text.
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
    printf("Usage: toolboxaddlinenumbers <inputPath> <outputPath>.\n");
    printf("       Example: in.pdf out.pdf\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

static double       dDistance   = 10.0;
static double       dFontSize   = 8.0;
static unsigned int iLineNumber = 0;

// Maximum number of unique Y positions (lines) per page
#define MAX_LINE_POSITIONS 4096

/* Try creating a font from a list of fallback names (Linux systems may not have Arial) */
TPtxPdfContent_Font* createFontWithFallbacks(TPtxPdf_Document* pDoc, const TCHAR* szStyle, BOOL bEmbed)
{
    const TCHAR*         fontNames[] = {_T("Arial"), _T("Liberation Sans"), _T("DejaVu Sans"), _T("Helvetica"),
                                        _T("sans-serif")};
    TPtxPdfContent_Font* pFont       = NULL;
    for (size_t i = 0; i < sizeof(fontNames) / sizeof(fontNames[0]); i++)
    {
        pFont = PtxPdfContent_Font_CreateFromSystem(pDoc, fontNames[i], szStyle, bEmbed);
        if (pFont != NULL)
            return pFont;
    }
    return NULL;
}
int copyDocumentData(TPtxPdf_Document* pInDoc, TPtxPdf_Document* pOutDoc)
{
    TPtxPdf_FileReferenceList* pInFileRefList;
    TPtxPdf_FileReferenceList* pOutFileRefList;

    // Output intent
    if (PtxPdf_Document_GetOutputIntent(pInDoc) != NULL)
        if (PtxPdf_Document_SetOutputIntent(pOutDoc, PtxPdfContent_IccBasedColorSpace_Copy(
                                                         pOutDoc, PtxPdf_Document_GetOutputIntent(pInDoc))) == FALSE)
            return FALSE;

    // Metadata
    if (PtxPdf_Document_SetMetadata(pOutDoc, PtxPdf_Metadata_Copy(pOutDoc, PtxPdf_Document_GetMetadata(pInDoc))) ==
        FALSE)
        return FALSE;

    // Viewer settings
    if (PtxPdf_Document_SetViewerSettings(
            pOutDoc, PtxPdfNav_ViewerSettings_Copy(pOutDoc, PtxPdf_Document_GetViewerSettings(pInDoc))) == FALSE)
        return FALSE;

    // Associated files (for PDF/A-3 and PDF 2.0 only)
    pInFileRefList  = PtxPdf_Document_GetAssociatedFiles(pInDoc);
    pOutFileRefList = PtxPdf_Document_GetAssociatedFiles(pOutDoc);
    if (pInFileRefList == NULL || pOutFileRefList == NULL)
        return FALSE;
    for (int iFileRef = 0; iFileRef < PtxPdf_FileReferenceList_GetCount(pInFileRefList); iFileRef++)
        if (PtxPdf_FileReferenceList_Add(
                pOutFileRefList,
                PtxPdf_FileReference_Copy(pOutDoc, PtxPdf_FileReferenceList_Get(pInFileRefList, iFileRef))) == FALSE)
            return FALSE;

    // Plain embedded files
    pInFileRefList  = PtxPdf_Document_GetPlainEmbeddedFiles(pInDoc);
    pOutFileRefList = PtxPdf_Document_GetPlainEmbeddedFiles(pOutDoc);
    if (pInFileRefList == NULL || pOutFileRefList == NULL)
        return FALSE;
    for (int iFileRef = 0; iFileRef < PtxPdf_FileReferenceList_GetCount(pInFileRefList); iFileRef++)
        if (PtxPdf_FileReferenceList_Add(
                pOutFileRefList,
                PtxPdf_FileReference_Copy(pOutDoc, PtxPdf_FileReferenceList_Get(pInFileRefList, iFileRef))) == FALSE)
            return FALSE;

    return TRUE;
}
// Comparison function for sorting doubles in descending order (top to bottom on page)
int compareDoubleDesc(const void* a, const void* b)
{
    double da   = *(const double*)a;
    double db   = *(const double*)b;
    double diff = db - da;
    if (fabs(diff) < dFontSize)
        return 0;
    return (diff > 0) ? 1 : -1;
}
int addLineNumbers(TPtxPdf_Document* pOutDoc, TPtxPdfContent_Font* pFont, TPtxPdf_Page* pInPage, TPtxPdf_Page* pOutPage)
{
    TPtxPdfContent_ContentExtractor*         pExtractor  = NULL;
    TPtxPdfContent_ContentExtractorIterator* pIterator   = NULL;
    TPtxPdfContent_ContentElement*           pElement    = NULL;
    TPtxPdfContent_Content*                  pInContent  = NULL;
    TPtxPdfContent_Content*                  pOutContent = NULL;
    TPtxPdfContent_ContentGenerator*         pContentGen = NULL;
    TPtxPdfContent_Text*                     pText       = NULL;
    TPtxPdfContent_TextGenerator*            pTextGen    = NULL;
    double                                   lineYPositions[MAX_LINE_POSITIONS];
    int                                      nLineCount = 0;
    double                                   dLeftX;
    TPtxGeomReal_Size                        pageSize;

    // Get page size
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pInPage, &pageSize),
                                      _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
    dLeftX = pageSize.dWidth;

    // Get input page content
    pInContent = PtxPdf_Page_GetContent(pInPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInContent, _T("Failed to get input page content. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Create content extractor with ungrouping
    pExtractor = PtxPdfContent_ContentExtractor_New(pInContent);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pExtractor, _T("Failed to create content extractor. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_ContentExtractor_SetUngrouping(pExtractor, ePtxPdfContent_UngroupingSelection_All),
        _T("Failed to set ungrouping. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

    // Get iterator
    pIterator = PtxPdfContent_ContentExtractor_GetIterator(pExtractor);
    GOTO_CLEANUP_IF_NULL(pIterator, _T("Failed to get iterator.\n"));
    PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);

    // Iterate over all content elements to collect line positions
    while ((pElement = PtxPdfContent_ContentExtractorIterator_GetValue(pIterator)) != NULL)
    {
        TPtxPdfContent_ContentElementType iType = PtxPdfContent_ContentElement_GetType(pElement);

        if (iType == ePtxPdfContent_ContentElementType_TextElement)
        {
            TPtxPdfContent_Text* pElemText = PtxPdfContent_TextElement_GetText((TPtxPdfContent_TextElement*)pElement);
            if (pElemText != NULL)
            {
                int nFragCount = PtxPdfContent_Text_GetCount(pElemText);
                for (int iFrag = 0; iFrag < nFragCount; iFrag++)
                {
                    TPtxPdfContent_TextFragment* pFragment = PtxPdfContent_Text_Get(pElemText, iFrag);
                    if (pFragment != NULL)
                    {
                        // Get the fragment's transform
                        TPtxGeomReal_AffineTransform fragTransform;
                        PtxPdfContent_TextFragment_GetTransform(pFragment, &fragTransform);

                        // Get the fragment's bounding box
                        TPtxGeomReal_Rectangle bbox;
                        PtxPdfContent_TextFragment_GetBoundingBox(pFragment, &bbox);

                        // Transform the base line starting point: (bbox.dLeft, 0)
                        double dPointX = fragTransform.dA * bbox.dLeft + fragTransform.dC * 0.0 + fragTransform.dE;
                        double dPointY = fragTransform.dB * bbox.dLeft + fragTransform.dD * 0.0 + fragTransform.dF;

                        // Update the left-most position
                        if (dPointX < dLeftX)
                            dLeftX = dPointX;

                        // Add the vertical position (check for duplicates within font size tolerance)
                        BOOL bFound = FALSE;
                        for (int i = 0; i < nLineCount; i++)
                        {
                            if (fabs(lineYPositions[i] - dPointY) < dFontSize)
                            {
                                bFound = TRUE;
                                break;
                            }
                        }
                        if (!bFound && nLineCount < MAX_LINE_POSITIONS)
                        {
                            lineYPositions[nLineCount] = dPointY;
                            nLineCount++;
                        }

                        Ptx_Release(pFragment);
                    }
                }
                Ptx_Release(pElemText);
            }
        }

        Ptx_Release(pElement);
        pElement = NULL;
        PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);
    }

    // If at least one text fragment was found: add line numbers
    if (nLineCount > 0)
    {
        // Sort Y positions in descending order (top of page to bottom)
        qsort(lineYPositions, nLineCount, sizeof(double), compareDoubleDesc);

        // Remove duplicates that are within font size tolerance after sorting
        double sortedUnique[MAX_LINE_POSITIONS];
        int    nUniqueCount = 0;
        for (int i = 0; i < nLineCount; i++)
        {
            if (nUniqueCount == 0 || fabs(sortedUnique[nUniqueCount - 1] - lineYPositions[i]) >= dFontSize)
            {
                sortedUnique[nUniqueCount] = lineYPositions[i];
                nUniqueCount++;
            }
        }

        // Get output page content
        pOutContent = PtxPdf_Page_GetContent(pOutPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutContent,
                                         _T("Failed to get output page content. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Create text object
        pText = PtxPdfContent_Text_Create(pOutDoc);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pText, _T("Failed to create text object. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Create text generator
        pTextGen = PtxPdfContent_TextGenerator_New(pText, pFont, dFontSize, NULL);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTextGen, _T("Failed to create text generator. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Iterate over all vertical positions
        for (int i = 0; i < nUniqueCount; i++)
        {
            TCHAR szLineNum[32];
            iLineNumber++;
            _sntprintf(szLineNum, ARRAY_SIZE(szLineNum), _T("%u"), iLineNumber);

            // Get the width of the line number string
            double dWidth = PtxPdfContent_TextGenerator_GetWidth(pTextGen, szLineNum);

            // Position line numbers right-aligned with a given distance
            TPtxGeomReal_Point position;
            position.dX = dLeftX - dWidth - dDistance;
            position.dY = sortedUnique[i];

            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_MoveTo(pTextGen, &position),
                                              _T("Failed to move to position. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                              Ptx_GetLastError());

            // Show the line number string
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_Show(pTextGen, szLineNum),
                                              _T("Failed to show line number. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                              Ptx_GetLastError());
        }

        // Close text generator
        PtxPdfContent_TextGenerator_Close(pTextGen);
        pTextGen = NULL;

        // Create content generator and paint text
        pContentGen = PtxPdfContent_ContentGenerator_New(pOutContent, FALSE);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContentGen,
                                         _T("Failed to create content generator. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_PaintText(pContentGen, pText),
                                          _T("Failed to paint text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
    }

cleanup:
    if (pTextGen != NULL)
        PtxPdfContent_TextGenerator_Close(pTextGen);
    if (pContentGen != NULL)
        PtxPdfContent_ContentGenerator_Close(pContentGen);
    if (pOutContent != NULL)
        Ptx_Release(pOutContent);
    if (pText != NULL)
        Ptx_Release(pText);
    if (pElement != NULL)
        Ptx_Release(pElement);
    if (pIterator != NULL)
        Ptx_Release(pIterator);
    if (pExtractor != NULL)
        Ptx_Release(pExtractor);
    if (pInContent != NULL)
        Ptx_Release(pInContent);

    return iReturnValue;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                    pInStream = NULL;
    TPtxSys_StreamDescriptor inDescriptor;
    TPtxPdf_Document*        pInDoc     = NULL;
    FILE*                    pOutStream = NULL;
    TPtxSys_StreamDescriptor outDescriptor;
    TPtxPdf_Document*        pOutDoc      = NULL;
    TPtxPdfContent_Font*     pFont        = NULL;
    TPtxPdf_PageList*        pInPageList  = NULL;
    TPtxPdf_PageList*        pOutPageList = NULL;
    TPtxPdf_PageList*        pCopiedPages = NULL;
    TPtxPdf_Page*            pInPage      = NULL;
    TPtxPdf_Page*            pOutPage     = NULL;
    TPtxPdf_PageCopyOptions* pCopyOptions = NULL;
    TPtxPdf_Conformance      iConformance;
    TCHAR*                   szInPath;
    TCHAR*                   szOutPath;

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

    szInPath  = argv[1];
    szOutPath = argv[2];

    // Open input document
    pInStream = _tfopen(szInPath, _T("rb"));
    GOTO_CLEANUP_IF_NULL(pInStream, _T("Failed to open input file \"%s\".\n"), szInPath);
    PtxSysCreateFILEStreamDescriptor(&inDescriptor, pInStream, 0);
    pInDoc = PtxPdf_Document_Open(&inDescriptor, _T(""));
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInDoc, _T("Input file \"%s\" cannot be opened. %s (ErrorCode: 0x%08x).\n"),
                                     szInPath, szErrorBuff, Ptx_GetLastError());

    // Create output document
    pOutStream = _tfopen(szOutPath, _T("wb+"));
    GOTO_CLEANUP_IF_NULL(pOutStream, _T("Failed to open output file \"%s\".\n"), szOutPath);
    PtxSysCreateFILEStreamDescriptor(&outDescriptor, pOutStream, 0);
    iConformance = PtxPdf_Document_GetConformance(pInDoc);
    pOutDoc      = PtxPdf_Document_Create(&outDescriptor, &iConformance, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutDoc, _T("Output file \"%s\" cannot be created. %s (ErrorCode: 0x%08x).\n"),
                                     szOutPath, szErrorBuff, Ptx_GetLastError());

    // Copy document-wide data
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(copyDocumentData(pInDoc, pOutDoc),
                                      _T("Failed to copy document-wide data. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Create a font for the line numbers
    pFont = createFontWithFallbacks(pOutDoc, NULL, TRUE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFont, _T("Failed to create font. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Define page copy options
    pCopyOptions = PtxPdf_PageCopyOptions_New();
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pCopyOptions, _T("Failed to create page copy options. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Get input page list
    pInPageList = PtxPdf_Document_GetPages(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList,
                                     _T("Failed to get the pages of the input document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Copy all pages from input to output document
    pCopiedPages = PtxPdf_PageList_Copy(pOutDoc, pInPageList, pCopyOptions);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pCopiedPages, _T("Failed to copy pages. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Iterate over all input-output page pairs and add line numbers
    int nPageCount = PtxPdf_PageList_GetCount(pInPageList);
    for (int iPage = 0; iPage < nPageCount; iPage++)
    {
        pInPage = PtxPdf_PageList_Get(pInPageList, iPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPage, _T("Failed to get input page %d. %s (ErrorCode: 0x%08x).\n"),
                                         iPage + 1, szErrorBuff, Ptx_GetLastError());
        pOutPage = PtxPdf_PageList_Get(pCopiedPages, iPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPage, _T("Failed to get output page %d. %s (ErrorCode: 0x%08x).\n"),
                                         iPage + 1, szErrorBuff, Ptx_GetLastError());

        if (addLineNumbers(pOutDoc, pFont, pInPage, pOutPage) != 0)
            goto cleanup;

        Ptx_Release(pInPage);
        pInPage = NULL;
        Ptx_Release(pOutPage);
        pOutPage = NULL;
    }

    // Add the finished pages to the output document's page list
    pOutPageList = PtxPdf_Document_GetPages(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageList,
                                     _T("Failed to get the pages of the output document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_AddRange(pOutPageList, pCopiedPages),
                                      _T("Failed to add pages to output document. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pOutPage != NULL)
        Ptx_Release(pOutPage);
    if (pInPage != NULL)
        Ptx_Release(pInPage);
    if (pCopiedPages != NULL)
        Ptx_Release(pCopiedPages);
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