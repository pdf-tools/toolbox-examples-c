/****************************************************************************
 *
 * File:            toolboxoverlaycolor.c
 *
 * Usage:           toolboxoverlaycolor [<options>] <inputPath> <outputPath>
 *                  Example: -k 0.5 1.0 in.pdf out.pdf
 *                  Options:
 *                  -k (k) (a)             specifiy grayscale and alpha color
 *                  -c (c) (m) (y) (k) (a)      specifiy CMKY and alpha color
 *                  -r (r) (g) (b) (a)          specifiy RGB and alpha color
 *                  color values between 0 and 1
 *                  default: -k 0.9 1.0
 *                  
 * Title:           Overlay color of PDF
 *                  
 * Description:     Overlay all pages of a PDF document with a configurable
 *                  color.
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
    printf("Usage: toolboxoverlaycolor [<options>] <inputPath> <outputPath>.\n");
    printf("       Example: -k 0.5 1.0 in.pdf out.pdf\n");
    printf("       Options:\n");
    printf("       -k (k) (a)             specifiy grayscale and alpha color\n");
    printf("       -c (c) (m) (y) (k) (a)      specifiy CMKY and alpha color\n");
    printf("       -r (r) (g) (b) (a)          specifiy RGB and alpha color\n");
    printf("       color values between 0 and 1\n");
    printf("       default: -k 0.9 1.0\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

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
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                                pInStream = NULL;
    TPtxSys_StreamDescriptor             inDescriptor;
    TPtxPdf_Document*                    pInDoc     = NULL;
    FILE*                                pOutStream = NULL;
    TPtxSys_StreamDescriptor             outDescriptor;
    TPtxPdf_Document*                    pOutDoc      = NULL;
    TPtxPdf_PageList*                    pInPageList  = NULL;
    TPtxPdf_PageList*                    pOutPageList = NULL;
    TPtxPdf_Page*                        pInPage      = NULL;
    TPtxPdf_Page*                        pOutPage     = NULL;
    TPtxPdf_PageCopyOptions*             pCopyOptions = NULL;
    TPtxPdf_Conformance                  iConformance;
    TPtxPdfContent_Transparency*         pTransparency = NULL;
    TPtxPdfContent_ColorSpace*           pColorSpace   = NULL;
    TPtxPdfContent_Paint*                pPaint        = NULL;
    TPtxPdfContent_Fill*                 pFill         = NULL;
    TPtxPdfContent_Path*                 pPath         = NULL;
    TPtxPdfContent_PathGenerator*        pPathGen      = NULL;
    TPtxPdfContent_Content*              pContent      = NULL;
    TPtxPdfContent_ContentGenerator*     pGenerator    = NULL;
    TCHAR*                               szInPath;
    TCHAR*                               szOutPath;
    TPtxPdfContent_ProcessColorSpaceType eColorType  = ePtxPdfContent_ProcessColorSpaceType_Gray;
    double                               color[4]    = {0.9, 0.0, 0.0, 0.0};
    size_t                               nColor      = 1;
    double                               dColorAlpha = 1.0;
    int                                  iArgIdx;

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

    // Parse optional color arguments
    iArgIdx = 1;
    while (iArgIdx < argc && argv[iArgIdx][0] == _T('-'))
    {
        TCHAR cOption = argv[iArgIdx][1];
        iArgIdx++;

        switch (cOption)
        {
        case _T('c'):
            eColorType = ePtxPdfContent_ProcessColorSpaceType_Cmyk;
            if (argc - iArgIdx < 7)
                return Usage();
            color[0]    = _tstof(argv[iArgIdx++]);
            color[1]    = _tstof(argv[iArgIdx++]);
            color[2]    = _tstof(argv[iArgIdx++]);
            color[3]    = _tstof(argv[iArgIdx++]);
            nColor      = 4;
            dColorAlpha = _tstof(argv[iArgIdx++]);
            break;
        case _T('k'):
            eColorType = ePtxPdfContent_ProcessColorSpaceType_Gray;
            if (argc - iArgIdx < 4)
                return Usage();
            color[0]    = _tstof(argv[iArgIdx++]);
            nColor      = 1;
            dColorAlpha = _tstof(argv[iArgIdx++]);
            break;
        case _T('r'):
            eColorType = ePtxPdfContent_ProcessColorSpaceType_Rgb;
            if (argc - iArgIdx < 6)
                return Usage();
            color[0]    = _tstof(argv[iArgIdx++]);
            color[1]    = _tstof(argv[iArgIdx++]);
            color[2]    = _tstof(argv[iArgIdx++]);
            nColor      = 3;
            dColorAlpha = _tstof(argv[iArgIdx++]);
            break;
        default:
            return Usage();
        }
    }

    if (argc - iArgIdx < 2)
    {
        return Usage();
    }

    szInPath  = argv[iArgIdx];
    szOutPath = argv[iArgIdx + 1];

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

    // Create transparency and set blend mode
    pTransparency = PtxPdfContent_Transparency_New(dColorAlpha);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTransparency, _T("Failed to create transparency. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_Transparency_SetBlendMode(pTransparency, ePtxPdfContent_BlendMode_Multiply),
        _T("Failed to set blend mode. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

    // Create color space
    pColorSpace = PtxPdfContent_ColorSpace_CreateProcessColorSpace(pOutDoc, eColorType);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pColorSpace, _T("Failed to create color space. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Create a transparent paint for the given color
    pPaint = PtxPdfContent_Paint_Create(pOutDoc, pColorSpace, color, nColor, pTransparency);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pPaint, _T("Failed to create paint. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Create fill
    pFill = PtxPdfContent_Fill_New(pPaint);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFill, _T("Failed to create fill. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Configure copy options
    pCopyOptions = PtxPdf_PageCopyOptions_New();

    // Get page lists
    pInPageList = PtxPdf_Document_GetPages(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList,
                                     _T("Failed to get the pages of the input document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pOutPageList = PtxPdf_Document_GetPages(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageList,
                                     _T("Failed to get the pages of the output document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Loop through all pages
    for (int i = 0; i < PtxPdf_PageList_GetCount(pInPageList); i++)
    {
        TPtxGeomReal_Size      pageSize;
        TPtxGeomReal_Rectangle pathRect;

        pInPage = PtxPdf_PageList_Get(pInPageList, i);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPage, _T("Failed to get input page. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Copy page from input to output
        pOutPage = PtxPdf_Page_Copy(pOutDoc, pInPage, pCopyOptions);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPage, _T("Failed to copy page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                         Ptx_GetLastError());

        // Get page size
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pInPage, &pageSize),
                                          _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        // Create content generator
        pContent = PtxPdf_Page_GetContent(pOutPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContent, _T("Failed to get page content. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        pGenerator = PtxPdfContent_ContentGenerator_New(pContent, FALSE);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pGenerator,
                                         _T("Failed to create content generator. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Create a rectangular path the same size as the page
        pPath = PtxPdfContent_Path_New();
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pPath, _T("Failed to create path. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                         Ptx_GetLastError());
        pPathGen = PtxPdfContent_PathGenerator_New(pPath);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pPathGen, _T("Failed to create path generator. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        pathRect.dLeft   = 0;
        pathRect.dBottom = 0;
        pathRect.dRight  = pageSize.dWidth;
        pathRect.dTop    = pageSize.dHeight;

        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_PathGenerator_AddRectangle(pPathGen, &pathRect),
                                          _T("Failed to add rectangle to path. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        // Close path generator
        PtxPdfContent_PathGenerator_Close(pPathGen);
        pPathGen = NULL;

        // Paint the path with the transparent paint
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_PaintPath(pGenerator, pPath, pFill, NULL),
                                          _T("Failed to paint path. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());

        // Close content generator
        PtxPdfContent_ContentGenerator_Close(pGenerator);
        pGenerator = NULL;

        // Add page to output document
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_Add(pOutPageList, pOutPage),
                                          _T("Failed to add page to output document. %s (ErrorCode: 0x%08x).\n"),
                                          szErrorBuff, Ptx_GetLastError());

        if (pPath != NULL)
        {
            Ptx_Release(pPath);
            pPath = NULL;
        }
        if (pContent != NULL)
        {
            Ptx_Release(pContent);
            pContent = NULL;
        }
        if (pOutPage != NULL)
        {
            Ptx_Release(pOutPage);
            pOutPage = NULL;
        }
        if (pInPage != NULL)
        {
            Ptx_Release(pInPage);
            pInPage = NULL;
        }
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pPathGen != NULL)
        PtxPdfContent_PathGenerator_Close(pPathGen);
    if (pPath != NULL)
        Ptx_Release(pPath);
    if (pGenerator != NULL)
        PtxPdfContent_ContentGenerator_Close(pGenerator);
    if (pContent != NULL)
        Ptx_Release(pContent);
    if (pOutPage != NULL)
        Ptx_Release(pOutPage);
    if (pInPage != NULL)
        Ptx_Release(pInPage);
    if (pOutPageList != NULL)
        Ptx_Release(pOutPageList);
    if (pInPageList != NULL)
        Ptx_Release(pInPageList);
    if (pFill != NULL)
        Ptx_Release(pFill);
    if (pPaint != NULL)
        Ptx_Release(pPaint);
    if (pColorSpace != NULL)
        Ptx_Release(pColorSpace);
    if (pTransparency != NULL)
        Ptx_Release(pTransparency);
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