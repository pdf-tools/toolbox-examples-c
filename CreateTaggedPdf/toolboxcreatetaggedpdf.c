/****************************************************************************
 *
 * File:            toolboxcreatetaggedpdf.c
 *
 * Usage:           toolboxcreatetaggedpdf <imagePath> <outPath>
 *                  Example: PdfToolsLogo.png out.pdf
 *                  
 * Title:           Create tagged PDF
 *                  
 * Description:     Create a new PDF document, add content and apply logical
 *                  structure (tags) during content creation.
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
    printf("Usage: toolboxcreatetaggedpdf <imagePath> <outPath>.\n");
    printf("       Example: PdfToolsLogo.png out.pdf\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

/* Convert centimeters to points (1 cm = 28.346456693 points) */
double cmToPoints(double value) { return value * 28.346456693; }
/* Try creating a font from a list of fallback names */
TPtxPdfContent_Font* createFontWithFallbacks(TPtxPdf_Document* pDoc)
{
    const TCHAR*         fontNames[] = {_T("Arial"), _T("Liberation Sans"), _T("DejaVu Sans"), _T("Helvetica"),
                                        _T("sans-serif")};
    int                  i;
    TPtxPdfContent_Font* pFont = NULL;

    for (i = 0; i < (int)(sizeof(fontNames) / sizeof(fontNames[0])); i++)
    {
        pFont = PtxPdfContent_Font_CreateFromSystem(pDoc, fontNames[i], _T(""), TRUE);
        if (pFont != NULL)
            return pFont;
    }
    return NULL;
}
int createAndTagText(TPtxPdf_Document* pOutDoc, TPtxPdf_Page* pOutPage, TPtxPdfContent_ContentGenerator* pGen,
                     TPtxPdfStructure_Node* pSectionNode, TPtxPdfContent_Font* pFont, double dTopY,
                     const TCHAR* szTagName, const TCHAR* szTextContent, double dFontSize, double* pBottomY,
                     TPtxPdfStructure_Node** ppCreatedNode)
{
    TPtxPdfStructure_Node*        pTextNode = NULL;
    TPtxPdfContent_Text*          pText     = NULL;
    TPtxPdfContent_TextGenerator* pTextGen  = NULL;
    TPtxPdfStructure_NodeList*    pChildren = NULL;
    double                        dBaselineY;
    double                        dFontAscent;
    double                        dFontDescent;
    TPtxGeomReal_Point            position;

    /* Create structure node for this text element */
    pTextNode = PtxPdfStructure_Node_New(szTagName, pOutDoc, pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTextNode, _T("Failed to create text node. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Set actual text and language */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetActualText(pTextNode, szTextContent),
                                      _T("Failed to set actual text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetLanguage(pTextNode, _T("en")),
                                      _T("Failed to set language. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Tag content generator as this node */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_TagAs(pGen, pTextNode, NULL),
                                      _T("Failed to tag as node. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Create text object */
    pText = PtxPdfContent_Text_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pText, _T("Failed to create text object. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    /* Add text node to section children */
    pChildren = PtxPdfStructure_Node_GetChildren(pSectionNode);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pChildren, _T("Failed to get children list. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_NodeList_Add(pChildren, pTextNode),
                                      _T("Failed to add text node to section. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Calculate text baseline position */
    dFontAscent = PtxPdfContent_Font_GetAscent(pFont);
    GOTO_CLEANUP_IF_ZERO_PRINT_ERROR(dFontAscent, _T("Failed to get font ascent. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    dBaselineY = dTopY - dFontSize * dFontAscent;

    /* Create text generator */
    pTextGen = PtxPdfContent_TextGenerator_New(pText, pFont, dFontSize, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTextGen, _T("Failed to create text generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Move to position and show text */
    position.dX = cmToPoints(2.5);
    position.dY = dBaselineY;
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_MoveTo(pTextGen, &position),
                                      _T("Failed to move to position. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_ShowLine(pTextGen, szTextContent),
                                      _T("Failed to show text line. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Close text generator */
    PtxPdfContent_TextGenerator_Close(pTextGen);
    pTextGen = NULL;

    /* Paint text */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_PaintText(pGen, pText),
                                      _T("Failed to paint text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Stop tagging */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_StopTagging(pGen),
                                      _T("Failed to stop tagging. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Return bottom coordinate (baseline - descent * fontSize) */
    dFontDescent = PtxPdfContent_Font_GetDescent(pFont);
    *pBottomY    = dBaselineY - dFontSize * dFontDescent;
    if (ppCreatedNode != NULL)
        *ppCreatedNode = pTextNode;

    /* Don't release pTextNode here - it's owned by the structure tree */
    pTextNode = NULL;

cleanup:
    if (pTextGen != NULL)
        PtxPdfContent_TextGenerator_Close(pTextGen);
    if (pText != NULL)
        Ptx_Release(pText);
    /* pTextNode is owned by tree, don't release */

    return iReturnValue;
}
int createAndTagImage(TPtxPdf_Document* pOutDoc, TPtxPdf_Page* pOutPage, TPtxPdfContent_ContentGenerator* pGen,
                      const TCHAR* szImagePath, double dTopY, TPtxPdfStructure_Node* pParentNode)
{
    TPtxPdfStructure_Node*     pFigureNode = NULL;
    TPtxPdfStructure_NodeList* pChildren   = NULL;
    FILE*                      pImgStream  = NULL;
    TPtxSys_StreamDescriptor   imageStreamDesc;
    TPtxPdfContent_Image*      pImage = NULL;
    TPtxGeomInt_Size           imgSize;
    double                     dX, dWidth, dHeight;
    TPtxGeomReal_Rectangle     rect;

    /* Create figure node */
    pFigureNode = PtxPdfStructure_Node_New(_T("Figure"), pOutDoc, pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFigureNode, _T("Failed to create figure node. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Set alternate text and language */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetAlternateText(pFigureNode, _T("PdfTools AG Logo")),
                                      _T("Failed to set alternate text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetLanguage(pFigureNode, _T("en")),
                                      _T("Failed to set language. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Set string attribute for layout */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetStringAttribute(pFigureNode, _T("O"), _T("Layout")),
                                      _T("Failed to set string attribute. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Add figure node to parent children */
    pChildren = PtxPdfStructure_Node_GetChildren(pParentNode);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pChildren, _T("Failed to get children list. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_NodeList_Add(pChildren, pFigureNode),
                                      _T("Failed to add figure node to parent. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Tag content generator as figure node */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_TagAs(pGen, pFigureNode, NULL),
                                      _T("Failed to tag as figure node. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Open image file and create image */
    pImgStream = _tfopen(szImagePath, _T("rb"));
    GOTO_CLEANUP_IF_NULL(pImgStream, _T("Failed to open image file \"%s\".\n"), szImagePath);
    PtxSysCreateFILEStreamDescriptor(&imageStreamDesc, pImgStream, 0);
    pImage = PtxPdfContent_Image_Create(pOutDoc, &imageStreamDesc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pImage, _T("Failed to create image. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    /* Calculate image rectangle preserving aspect ratio */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_Image_GetSize(pImage, &imgSize),
                                      _T("Failed to get image size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    dX      = cmToPoints(2.5);
    dWidth  = cmToPoints(2.0);
    dHeight = dWidth * (double)imgSize.iHeight / (double)imgSize.iWidth; /* preserve aspect ratio */

    rect.dLeft   = dX;
    rect.dBottom = dTopY - dHeight;
    rect.dRight  = dX + dWidth;
    rect.dTop    = dTopY;

    /* Set bounding box on figure node */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetBoundingBox(pFigureNode, &rect),
                                      _T("Failed to set bounding box. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Paint image */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_PaintImage(pGen, pImage, &rect),
                                      _T("Failed to paint image. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Stop tagging */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_StopTagging(pGen),
                                      _T("Failed to stop tagging. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Don't release pFigureNode - owned by tree */
    pFigureNode = NULL;

cleanup:
    if (pImage != NULL)
        Ptx_Release(pImage);
    if (pImgStream != NULL)
        fclose(pImgStream);

    return iReturnValue;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                            pOutStream = NULL;
    TPtxSys_StreamDescriptor         outDescriptor;
    TPtxPdf_Document*                pOutDoc         = NULL;
    TPtxPdfContent_Font*             pFont           = NULL;
    TPtxPdf_Page*                    pOutPage        = NULL;
    TPtxPdf_PageList*                pOutPageList    = NULL;
    TPtxPdfContent_Content*          pContent        = NULL;
    TPtxPdfContent_ContentGenerator* pGenerator      = NULL;
    TPtxPdf_Metadata*                pMetadata       = NULL;
    TPtxPdfNav_ViewerSettings*       pViewerSettings = NULL;
    TPtxPdfStructure_Tree*           pStructTree     = NULL;
    TPtxPdfStructure_Node*           pDocNode        = NULL;
    TPtxPdfStructure_Node*           pSectionNode    = NULL;
    TPtxPdfStructure_NodeList*       pDocChildren    = NULL;
    TPtxPdfStructure_Node*           pLastTextNode   = NULL;
    TPtxPdf_Conformance              iConformance;
    TPtxGeomReal_Size                pageSize;
    double                           dCurrentY;
    double                           dPadding;
    TCHAR*                           szImagePath;
    TCHAR*                           szOutPath;

    setlocale(LC_CTYPE, "");


    // Check command line parameters
    if (argc < 3 || argc > 3)
    {
        return Usage();
    }

    /* Initialize library */
    Ptx_Initialize();

    /* Set and check license key */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(Ptx_Sdk_Initialize(_T("<-- insert license key -->"), NULL),
                                      _T("Failed to set license key. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    szImagePath = argv[1];
    szOutPath   = argv[2];

    /* Create output document */
    pOutStream = _tfopen(szOutPath, _T("wb+"));
    GOTO_CLEANUP_IF_NULL(pOutStream, _T("Failed to open output file \"%s\".\n"), szOutPath);
    PtxSysCreateFILEStreamDescriptor(&outDescriptor, pOutStream, 0);
    iConformance = ePtxPdf_Conformance_Pdf17;
    pOutDoc      = PtxPdf_Document_Create(&outDescriptor, &iConformance, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutDoc, _T("Output file \"%s\" cannot be created. %s (ErrorCode: 0x%08x).\n"),
                                     szOutPath, szErrorBuff, Ptx_GetLastError());

    /* Create font with fallbacks */
    pFont = createFontWithFallbacks(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFont, _T("Failed to create font. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    /* Set document language */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Document_SetLanguage(pOutDoc, _T("en")),
                                      _T("Failed to set document language. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Set PDF/UA conformant */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Document_SetPdfUaConformant(pOutDoc),
                                      _T("Failed to set PDF/UA conformant. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Set metadata title */
    pMetadata = PtxPdf_Document_GetMetadata(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pMetadata, _T("Failed to get metadata. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Metadata_SetTitle(pMetadata, _T("TaggedPDF")),
                                      _T("Failed to set title. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Set viewer settings to display document title */
    pViewerSettings = PtxPdf_Document_GetViewerSettings(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pViewerSettings, _T("Failed to get viewer settings. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfNav_ViewerSettings_SetDisplayDocumentTitle(pViewerSettings, TRUE),
                                      _T("Failed to set display document title. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    /* Create a page (DIN A4: 21cm x 29.7cm) */
    pageSize.dWidth  = cmToPoints(21.0);
    pageSize.dHeight = cmToPoints(29.7);
    pOutPage         = PtxPdf_Page_Create(pOutDoc, &pageSize);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPage, _T("Failed to create page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    /* Get page content and create content generator */
    pContent = PtxPdf_Page_GetContent(pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContent, _T("Failed to get page content. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    pGenerator = PtxPdfContent_ContentGenerator_New(pContent, FALSE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pGenerator, _T("Failed to create content generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Create structure tree */
    pStructTree = PtxPdfStructure_Tree_New(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pStructTree, _T("Failed to create structure tree. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    pDocNode = PtxPdfStructure_Tree_GetDocumentNode(pStructTree);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pDocNode, _T("Failed to get document node. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Create section node */
    pSectionNode = PtxPdfStructure_Node_New(_T("Sect"), pOutDoc, pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pSectionNode, _T("Failed to create section node. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pDocChildren = PtxPdfStructure_Node_GetChildren(pDocNode);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pDocChildren, _T("Failed to get document children. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_NodeList_Add(pDocChildren, pSectionNode),
                                      _T("Failed to add section node. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Start from the top of the page with margin */
    dCurrentY = pageSize.dHeight - cmToPoints(2.5);
    dPadding  = cmToPoints(1.0);

    /* Create header (H1) */
    if (createAndTagText(pOutDoc, pOutPage, pGenerator, pSectionNode, pFont, dCurrentY, _T("H1"),
                         _T("This is a properly tagged heading"), 24.0, &dCurrentY, &pLastTextNode) != 0)
        goto cleanup;

    /* Add padding and create paragraph (P) */
    dCurrentY -= dPadding;
    if (createAndTagText(pOutDoc, pOutPage, pGenerator, pSectionNode, pFont, dCurrentY, _T("P"),
                         _T("This is a properly tagged paragraph. Both heading and paragraph belong to a section."),
                         12.0, &dCurrentY, &pLastTextNode) != 0)
        goto cleanup;

    /* Add padding and create image */
    dCurrentY -= dPadding;
    if (createAndTagImage(pOutDoc, pOutPage, pGenerator, szImagePath, dCurrentY, pLastTextNode) != 0)
        goto cleanup;

    /* Close content generator */
    PtxPdfContent_ContentGenerator_Close(pGenerator);
    pGenerator = NULL;

    /* Add page to output document */
    pOutPageList = PtxPdf_Document_GetPages(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageList, _T("Failed to get output page list. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_Add(pOutPageList, pOutPage),
                                      _T("Failed to add page to output document. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pGenerator != NULL)
        PtxPdfContent_ContentGenerator_Close(pGenerator);
    if (pContent != NULL)
        Ptx_Release(pContent);
    if (pOutPage != NULL)
        Ptx_Release(pOutPage);
    if (pOutPageList != NULL)
        Ptx_Release(pOutPageList);
    if (pFont != NULL)
        Ptx_Release(pFont);
    if (pOutDoc != NULL)
        PtxPdf_Document_Close(pOutDoc);
    if (pOutStream != NULL)
        fclose(pOutStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 