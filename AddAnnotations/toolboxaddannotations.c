/****************************************************************************
 *
 * File:            toolboxaddannotations.c
 *
 * Usage:           toolboxaddannotations <inputPath> <outputPath>
 *                  Example: in.pdf out.pdf
 *                  
 * Title:           Add annotations to PDF
 *                  
 * Description:     Generate and add various types of annotations at
 *                  specified positions on the first page of a PDF document.
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
    printf("Usage: toolboxaddannotations <inputPath> <outputPath>.\n");
    printf("       Example: in.pdf out.pdf\n");

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
int copyAndAddAnnotations(TPtxPdf_Document* pOutDoc, TPtxPdf_Page* pInPage, TPtxPdf_PageCopyOptions* pCopyOptions,
                          TPtxPdf_Page** ppOutPage)
{
    TPtxPdf_Page*                    pOutPage       = NULL;
    TPtxPdfContent_ColorSpace*       pRgb           = NULL;
    TPtxPdfContent_Paint*            pGreen         = NULL;
    TPtxPdfContent_Paint*            pBlue          = NULL;
    TPtxPdfContent_Paint*            pYellow        = NULL;
    TPtxPdfContent_Paint*            pYellowTransp  = NULL;
    TPtxPdfContent_Paint*            pRed           = NULL;
    TPtxPdfContent_Transparency*     pTransparency  = NULL;
    TPtxPdfAnnots_StickyNote*        pStickyNote    = NULL;
    TPtxPdfAnnots_EllipseAnnotation* pEllipse       = NULL;
    TPtxPdfAnnots_FreeText*          pFreeText      = NULL;
    TPtxPdfAnnots_Highlight*         pHighlight     = NULL;
    TPtxPdfNav_WebLink*              pWebLink       = NULL;
    TPtxPdfAnnots_AnnotationList*    pAnnotations   = NULL;
    TPtxPdfNav_LinkList*             pLinks         = NULL;
    TPtxPdfContent_Stroke*           pStroke        = NULL;
    TPtxPdfContent_Stroke*           pWebLinkStroke = NULL;
    TPtxPdfContent_ContentExtractor* pExtractor     = NULL;
    TPtxPdfContent_Content*          pInContent     = NULL;
    TPtxGeomReal_Size                pageSize;
    TPtxGeomReal_Point               stickyNoteTopLeft;
    TPtxGeomReal_Rectangle           ellipseBox;
    TPtxGeomReal_Rectangle           freeTextBox;
    double                           aGreen[3];
    double                           aBlue[3];
    double                           aYellow[3];
    double                           aRed[3];
    BOOL                             bHighlightCreated = FALSE;
    BOOL                             bWebLinkCreated   = FALSE;

    // Copy page to output document
    pOutPage = PtxPdf_Page_Copy(pOutDoc, pInPage, pCopyOptions);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPage, _T("Failed to copy page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Make a RGB color space
    pRgb = PtxPdfContent_ColorSpace_CreateProcessColorSpace(pOutDoc, ePtxPdfContent_ProcessColorSpaceType_Rgb);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pRgb, _T("Failed to create RGB color space. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Get the page size for positioning annotations
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pOutPage, &pageSize),
                                      _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Get the output page's list of annotations for adding annotations
    pAnnotations = PtxPdf_Page_GetAnnotations(pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pAnnotations, _T("Failed to get annotations. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Create a sticky note and add to output page's annotations
    aGreen[0] = 0.0;
    aGreen[1] = 1.0;
    aGreen[2] = 0.0;
    pGreen    = PtxPdfContent_Paint_Create(pOutDoc, pRgb, aGreen, 3, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pGreen, _T("Failed to create green paint. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    stickyNoteTopLeft.dX = 10.0;
    stickyNoteTopLeft.dY = pageSize.dHeight - 10.0;
    pStickyNote          = PtxPdfAnnots_StickyNote_Create(pOutDoc, &stickyNoteTopLeft, _T("Hello world!"), pGreen);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pStickyNote, _T("Failed to create sticky note. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfAnnots_AnnotationList_Add(pAnnotations, (TPtxPdfAnnots_Annotation*)pStickyNote),
        _T("Failed to add sticky note. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

    // Create an ellipse and add to output page's annotations
    aBlue[0] = 0.0;
    aBlue[1] = 0.0;
    aBlue[2] = 1.0;
    pBlue    = PtxPdfContent_Paint_Create(pOutDoc, pRgb, aBlue, 3, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pBlue, _T("Failed to create blue paint. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    aYellow[0] = 1.0;
    aYellow[1] = 1.0;
    aYellow[2] = 0.0;
    pYellow    = PtxPdfContent_Paint_Create(pOutDoc, pRgb, aYellow, 3, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pYellow, _T("Failed to create yellow paint. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    ellipseBox.dLeft   = 10.0;
    ellipseBox.dBottom = pageSize.dHeight - 60.0;
    ellipseBox.dRight  = 70.0;
    ellipseBox.dTop    = pageSize.dHeight - 20.0;
    pStroke            = PtxPdfContent_Stroke_New(pBlue, 1.5);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pStroke, _T("Failed to create stroke. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    pEllipse = PtxPdfAnnots_EllipseAnnotation_Create(pOutDoc, &ellipseBox, pStroke, pYellow);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pEllipse, _T("Failed to create ellipse. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfAnnots_AnnotationList_Add(pAnnotations, (TPtxPdfAnnots_Annotation*)pEllipse),
        _T("Failed to add ellipse. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

    // Create a free text and add to output page's annotations
    pTransparency = PtxPdfContent_Transparency_New(0.5);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTransparency, _T("Failed to create transparency. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pYellowTransp = PtxPdfContent_Paint_Create(pOutDoc, pRgb, aYellow, 3, pTransparency);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pYellowTransp,
                                     _T("Failed to create yellow transparent paint. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    freeTextBox.dLeft   = 10.0;
    freeTextBox.dBottom = pageSize.dHeight - 170.0;
    freeTextBox.dRight  = 120.0;
    freeTextBox.dTop    = pageSize.dHeight - 70.0;
    pFreeText           = PtxPdfAnnots_FreeText_Create(
        pOutDoc, &freeTextBox, _T("Lorem ipsum dolor sit amet, consectetur adipiscing elit."), pYellowTransp, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFreeText, _T("Failed to create free text. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfAnnots_AnnotationList_Add(pAnnotations, (TPtxPdfAnnots_Annotation*)pFreeText),
        _T("Failed to add free text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

    // Extract content elements from the input page for highlight and web-link
    pInContent = PtxPdf_Page_GetContent(pInPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInContent, _T("Failed to get input page content. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pExtractor = PtxPdfContent_ContentExtractor_New(pInContent);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pExtractor, _T("Failed to create content extractor. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Iterate over content elements
    {
        TPtxPdfContent_ContentExtractorIterator* pIter = PtxPdfContent_ContentExtractor_GetIterator(pExtractor);
        while (PtxPdfContent_ContentExtractorIterator_MoveNext(pIter))
        {
            TPtxPdfContent_ContentElement* pElement = PtxPdfContent_ContentExtractorIterator_GetValue(pIter);
            if (pElement == NULL)
                continue;

            TPtxPdfContent_ContentElementType eType = PtxPdfContent_ContentElement_GetType(pElement);

            // Take the first text element for highlight
            if (!bHighlightCreated && eType == ePtxPdfContent_ContentElementType_TextElement)
            {
                TPtxPdfContent_TextElement* pTextElement = (TPtxPdfContent_TextElement*)pElement;
                TPtxPdfContent_Text*        pTextIter    = PtxPdfContent_TextElement_GetText(pTextElement);
                if (pTextIter != NULL)
                {
                    TPtxGeomReal_QuadrilateralList* pQuads = PtxGeomReal_QuadrilateralList_New();
                    if (pQuads != NULL)
                    {
                        // Iterate over text fragments and collect quadrilaterals
                        int nFragments = PtxPdfContent_Text_GetCount(pTextIter);
                        for (int iFrag = 0; iFrag < nFragments; iFrag++)
                        {
                            TPtxPdfContent_TextFragment* pFragment = PtxPdfContent_Text_Get(pTextIter, iFrag);
                            if (pFragment != NULL)
                            {
                                TPtxGeomReal_AffineTransform transform;
                                TPtxGeomReal_Rectangle       bbox;
                                if (PtxPdfContent_TextFragment_GetTransform(pFragment, &transform) == TRUE &&
                                    PtxPdfContent_TextFragment_GetBoundingBox(pFragment, &bbox) == TRUE)
                                {
                                    TPtxGeomReal_Quadrilateral quad;
                                    PtxGeomReal_AffineTransform_TransformRectangle(&transform, &bbox, &quad);
                                    PtxGeomReal_QuadrilateralList_Add(pQuads, &quad);
                                }
                            }
                        }

                        // Create a highlight
                        pHighlight = PtxPdfAnnots_Highlight_CreateFromQuadrilaterals(pOutDoc, pQuads, pYellow);
                        if (pHighlight != NULL)
                        {
                            PtxPdfAnnots_AnnotationList_Add(pAnnotations, (TPtxPdfAnnots_Annotation*)pHighlight);
                            bHighlightCreated = TRUE;
                        }
                    }
                }
            }

            // Take the first image element for web-link
            if (!bWebLinkCreated && eType == ePtxPdfContent_ContentElementType_ImageElement)
            {
                TPtxGeomReal_QuadrilateralList* pQuads = PtxGeomReal_QuadrilateralList_New();
                if (pQuads != NULL)
                {
                    TPtxGeomReal_AffineTransform transform;
                    TPtxGeomReal_Rectangle       bbox;
                    if (PtxPdfContent_ContentElement_GetTransform(pElement, &transform) == TRUE &&
                        PtxPdfContent_ContentElement_GetBoundingBox(pElement, &bbox) == TRUE)
                    {
                        TPtxGeomReal_Quadrilateral quad;
                        PtxGeomReal_AffineTransform_TransformRectangle(&transform, &bbox, &quad);
                        PtxGeomReal_QuadrilateralList_Add(pQuads, &quad);

                        // Create a web-link
                        pWebLink = PtxPdfNav_WebLink_CreateFromQuadrilaterals(pOutDoc, pQuads,
                                                                              _T("https://www.pdf-tools.com"));
                        if (pWebLink != NULL)
                        {
                            // Set border style
                            aRed[0] = 1.0;
                            aRed[1] = 0.0;
                            aRed[2] = 0.0;
                            pRed    = PtxPdfContent_Paint_Create(pOutDoc, pRgb, aRed, 3, NULL);
                            if (pRed != NULL)
                            {
                                pWebLinkStroke = PtxPdfContent_Stroke_New(pRed, 1.5);
                                if (pWebLinkStroke != NULL)
                                    PtxPdfNav_Link_SetBorderStyle((TPtxPdfNav_Link*)pWebLink, pWebLinkStroke);
                            }

                            // Add web-link to the page's links
                            pLinks = PtxPdf_Page_GetLinks(pOutPage);
                            if (pLinks != NULL)
                                PtxPdfNav_LinkList_Add(pLinks, (TPtxPdfNav_Link*)pWebLink);
                            bWebLinkCreated = TRUE;
                        }
                    }
                }
            }

            // Exit loop if both have been created
            if (bHighlightCreated && bWebLinkCreated)
                break;
        }
    }

    *ppOutPage = pOutPage;
    pOutPage   = NULL; // prevent release in cleanup

cleanup:
    if (pExtractor != NULL)
        Ptx_Release(pExtractor);
    if (pInContent != NULL)
        Ptx_Release(pInContent);
    if (pOutPage != NULL)
        Ptx_Release(pOutPage);

    return iReturnValue;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                    pInStream = NULL;
    TPtxSys_StreamDescriptor descriptor;
    TPtxPdf_Document*        pInDoc     = NULL;
    FILE*                    pOutStream = NULL;
    TPtxSys_StreamDescriptor outDescriptor;
    TPtxPdf_Document*        pOutDoc       = NULL;
    TPtxPdf_PageList*        pInPageList   = NULL;
    TPtxPdf_PageList*        pOutPageList  = NULL;
    TPtxPdf_PageList*        pInPageRange  = NULL;
    TPtxPdf_PageList*        pOutPageRange = NULL;
    TPtxPdf_Page*            pInPage       = NULL;
    TPtxPdf_Page*            pOutPage      = NULL;
    TPtxPdf_PageCopyOptions* pCopyOptions  = NULL;
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
    PtxSysCreateFILEStreamDescriptor(&descriptor, pInStream, 0);
    pInDoc = PtxPdf_Document_Open(&descriptor, _T(""));
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

    // Define page copy options
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

    // Copy first page and add annotations
    pInPage = PtxPdf_PageList_Get(pInPageList, 0);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPage, _T("Failed to get the first page. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    if (copyAndAddAnnotations(pOutDoc, pInPage, pCopyOptions, &pOutPage) != 0)
        goto cleanup;

    // Add the page to the output document's page list
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_Add(pOutPageList, pOutPage),
                                      _T("Failed to add page to output document. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    // Copy the remaining pages and add to the output document's page list
    if (PtxPdf_PageList_GetCount(pInPageList) > 1)
    {
        pInPageRange = PtxPdf_PageList_GetRange(pInPageList, 1, PtxPdf_PageList_GetCount(pInPageList) - 1);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageRange, _T("Failed to get page range. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        pOutPageRange = PtxPdf_PageList_Copy(pOutDoc, pInPageRange, pCopyOptions);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageRange, _T("Failed to copy page range. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_AddRange(pOutPageList, pOutPageRange),
                                          _T("Failed to add page range. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pOutPage != NULL)
        Ptx_Release(pOutPage);
    if (pInPage != NULL)
        Ptx_Release(pInPage);
    if (pOutPageRange != NULL)
        Ptx_Release(pOutPageRange);
    if (pInPageRange != NULL)
        Ptx_Release(pInPageRange);
    if (pOutPageList != NULL)
        Ptx_Release(pOutPageList);
    if (pInPageList != NULL)
        Ptx_Release(pInPageList);
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