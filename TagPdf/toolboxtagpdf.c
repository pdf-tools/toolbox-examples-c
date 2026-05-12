/****************************************************************************
 *
 * File:            toolboxtagpdf.c
 *
 * Usage:           toolboxtagpdf <inPath> <outPath>
 *                  Example: in.pdf out.pdf
 *                  
 * Title:           Tag existing PDF content
 *                  
 * Description:     Copy content from an existing PDF, then apply logical
 *                  structure (tags) to selected elements.
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

int Usage()
{
    printf("Usage: toolboxtagpdf <inPath> <outPath>.\n");
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

    /* Output intent */
    if (PtxPdf_Document_GetOutputIntent(pInDoc) != NULL)
        if (PtxPdf_Document_SetOutputIntent(pOutDoc, PtxPdfContent_IccBasedColorSpace_Copy(
                                                         pOutDoc, PtxPdf_Document_GetOutputIntent(pInDoc))) == FALSE)
            return FALSE;

    /* Metadata */
    if (PtxPdf_Document_SetMetadata(pOutDoc, PtxPdf_Metadata_Copy(pOutDoc, PtxPdf_Document_GetMetadata(pInDoc))) ==
        FALSE)
        return FALSE;

    /* Viewer settings */
    if (PtxPdf_Document_SetViewerSettings(
            pOutDoc, PtxPdfNav_ViewerSettings_Copy(pOutDoc, PtxPdf_Document_GetViewerSettings(pInDoc))) == FALSE)
        return FALSE;

    /* Associated files (for PDF/A-3 and PDF 2.0 only) */
    pInFileRefList  = PtxPdf_Document_GetAssociatedFiles(pInDoc);
    pOutFileRefList = PtxPdf_Document_GetAssociatedFiles(pOutDoc);
    if (pInFileRefList == NULL || pOutFileRefList == NULL)
        return FALSE;
    for (int iFileRef = 0; iFileRef < PtxPdf_FileReferenceList_GetCount(pInFileRefList); iFileRef++)
        if (PtxPdf_FileReferenceList_Add(
                pOutFileRefList,
                PtxPdf_FileReference_Copy(pOutDoc, PtxPdf_FileReferenceList_Get(pInFileRefList, iFileRef))) == FALSE)
            return FALSE;

    /* Plain embedded files */
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
int copyAndTagTextElement(TPtxPdfContent_TextElement* pTextElement, TPtxPdfStructure_Node* pSection,
                          TPtxPdfContent_ContentGenerator* pGenerator, TPtxPdf_Page* pOutPage,
                          TPtxPdf_Document* pOutDoc, const TCHAR* szTag, TPtxPdfStructure_Node** ppOutNode)
{
    TPtxPdfStructure_Node*       pElement  = NULL;
    TPtxPdfStructure_NodeList*   pChildren = NULL;
    TPtxPdfContent_Text*         pTextList = NULL;
    TPtxPdfContent_TextFragment* pFragment = NULL;
    size_t                       nTextLen;
    TCHAR*                       szText = NULL;

    /* Create structure node */
    pElement = PtxPdfStructure_Node_New(szTag, pOutDoc, pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pElement, _T("Failed to create node. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    /* Get the text content from the first text fragment */
    pTextList = PtxPdfContent_TextElement_GetText(pTextElement);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTextList, _T("Failed to get text list. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    pFragment = PtxPdfContent_Text_Get(pTextList, 0);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFragment, _T("Failed to get text fragment. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    nTextLen = PtxPdfContent_TextFragment_GetText(pFragment, NULL, 0);
    if (nTextLen > 0)
    {
        szText = (TCHAR*)malloc(nTextLen * sizeof(TCHAR));
        GOTO_CLEANUP_IF_NULL(szText, _T("Failed to allocate memory for text.\n"));
        PtxPdfContent_TextFragment_GetText(pFragment, szText, nTextLen);

        /* Set actual text */
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetActualText(pElement, szText),
                                          _T("Failed to set actual text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
    }

    /* Set language */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetLanguage(pElement, _T("en")),
                                      _T("Failed to set language. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Add node to section children */
    pChildren = PtxPdfStructure_Node_GetChildren(pSection);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pChildren, _T("Failed to get children. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_NodeList_Add(pChildren, pElement),
                                      _T("Failed to add node. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Tag as this node */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_TagAs(pGenerator, pElement, NULL),
                                      _T("Failed to tag as node. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Append content element */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_ContentGenerator_AppendContentElement(pGenerator, (TPtxPdfContent_ContentElement*)pTextElement),
        _T("Failed to append content element. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

    /* Stop tagging */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_StopTagging(pGenerator),
                                      _T("Failed to stop tagging. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    if (ppOutNode != NULL)
        *ppOutNode = pElement;
    pElement = NULL; /* Owned by tree */

cleanup:
    if (szText != NULL)
        free(szText);
    if (pFragment != NULL)
        Ptx_Release(pFragment);
    if (pTextList != NULL)
        Ptx_Release(pTextList);

    return iReturnValue;
}
int copyAndTagImageElement(TPtxPdfContent_ImageElement* pImageElement, TPtxPdfContent_ContentGenerator* pGenerator,
                           TPtxPdf_Page* pOutPage, TPtxPdf_Document* pOutDoc, const TCHAR* szAlternateText,
                           TPtxPdfStructure_Node* pParentNode)
{
    TPtxPdfStructure_Node*       pImgNode  = NULL;
    TPtxPdfStructure_NodeList*   pChildren = NULL;
    TPtxGeomReal_Rectangle       boundingBox;
    TPtxGeomReal_AffineTransform transform;
    TPtxGeomReal_Quadrilateral   quad;
    TPtxGeomReal_Rectangle       rect;

    /* Create figure node */
    pImgNode = PtxPdfStructure_Node_New(_T("Figure"), pOutDoc, pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pImgNode, _T("Failed to create figure node. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Set alternate text and language */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetAlternateText(pImgNode, szAlternateText),
                                      _T("Failed to set alternate text. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetLanguage(pImgNode, _T("en")),
                                      _T("Failed to set language. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Get bounding box via transform */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_ContentElement_GetBoundingBox((TPtxPdfContent_ContentElement*)pImageElement, &boundingBox),
        _T("Failed to get bounding box. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_ContentElement_GetTransform((TPtxPdfContent_ContentElement*)pImageElement, &transform),
        _T("Failed to get transform. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxGeomReal_AffineTransform_TransformRectangle(&transform, &boundingBox, &quad),
                                      _T("Failed to transform rectangle. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    rect.dLeft   = quad.BottomLeft.dX;
    rect.dBottom = quad.BottomLeft.dY;
    rect.dRight  = quad.TopRight.dX;
    rect.dTop    = quad.TopRight.dY;

    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetBoundingBox(pImgNode, &rect),
                                      _T("Failed to set bounding box. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Set string attribute for layout */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_Node_SetStringAttribute(pImgNode, _T("O"), _T("Layout")),
                                      _T("Failed to set string attribute. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Add to parent children */
    pChildren = PtxPdfStructure_Node_GetChildren(pParentNode);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pChildren, _T("Failed to get children. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_NodeList_Add(pChildren, pImgNode),
                                      _T("Failed to add figure node. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Tag as figure node */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_TagAs(pGenerator, pImgNode, NULL),
                                      _T("Failed to tag as figure. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Append content element */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_ContentGenerator_AppendContentElement(pGenerator, (TPtxPdfContent_ContentElement*)pImageElement),
        _T("Failed to append image element. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

    /* Stop tagging */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_StopTagging(pGenerator),
                                      _T("Failed to stop tagging. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    pImgNode = NULL; /* Owned by tree */

cleanup:
    return iReturnValue;
}
int copyAndTagContent(TPtxPdf_Page* pInPage, TPtxPdf_Page* pOutPage, TPtxPdf_Document* pOutDoc)
{
    TPtxPdfContent_Content*                  pInContent     = NULL;
    TPtxPdfContent_Content*                  pOutContent    = NULL;
    TPtxPdfContent_ContentExtractor*         pExtractor     = NULL;
    TPtxPdfContent_ContentGenerator*         pGenerator     = NULL;
    TPtxPdfContent_ContentExtractorIterator* pIterator      = NULL;
    TPtxPdfContent_ContentElement*           pInElement     = NULL;
    TPtxPdfContent_ContentElement*           pOutElement    = NULL;
    TPtxPdfStructure_Tree*                   pStructTree    = NULL;
    TPtxPdfStructure_Node*                   pDocNode       = NULL;
    TPtxPdfStructure_Node*                   pSection       = NULL;
    TPtxPdfStructure_NodeList*               pDocChildren   = NULL;
    TPtxPdfStructure_Node*                   pParagraphNode = NULL;

    /* Create structure tree */
    pStructTree = PtxPdfStructure_Tree_New(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pStructTree, _T("Failed to create structure tree. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    pDocNode = PtxPdfStructure_Tree_GetDocumentNode(pStructTree);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pDocNode, _T("Failed to get document node. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Create section node */
    pSection = PtxPdfStructure_Node_New(_T("Sect"), pOutDoc, pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pSection, _T("Failed to create section node. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pDocChildren = PtxPdfStructure_Node_GetChildren(pDocNode);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pDocChildren, _T("Failed to get document children. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfStructure_NodeList_Add(pDocChildren, pSection),
                                      _T("Failed to add section to document. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Get input content */
    pInContent = PtxPdf_Page_GetContent(pInPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInContent, _T("Failed to get input content. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Create content extractor */
    pExtractor = PtxPdfContent_ContentExtractor_New(pInContent);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pExtractor, _T("Failed to create content extractor. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Get output content and create generator */
    pOutContent = PtxPdf_Page_GetContent(pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutContent, _T("Failed to get output content. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pGenerator = PtxPdfContent_ContentGenerator_New(pOutContent, FALSE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pGenerator, _T("Failed to create content generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Iterate over all content elements */
    pIterator = PtxPdfContent_ContentExtractor_GetIterator(pExtractor);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pIterator, _T("Failed to get iterator. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);

    while ((pInElement = PtxPdfContent_ContentExtractorIterator_GetValue(pIterator)) != NULL)
    {
        TPtxPdfContent_ContentElementType iType = PtxPdfContent_ContentElement_GetType(pInElement);

        if (iType == ePtxPdfContent_ContentElementType_TextElement)
        {
            /* Copy text element to output document */
            pOutElement = PtxPdfContent_ContentElement_Copy(pOutDoc, pInElement);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutElement,
                                             _T("Failed to copy content element. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());

            /* Get text content to determine which tag to use */
            {
                TPtxPdfContent_TextElement*  pTextElem = (TPtxPdfContent_TextElement*)pOutElement;
                TPtxPdfContent_Text*         pTextList = PtxPdfContent_TextElement_GetText(pTextElem);
                TPtxPdfContent_TextFragment* pFrag     = NULL;
                size_t                       nLen;
                TCHAR*                       szFragText = NULL;

                if (pTextList != NULL)
                {
                    pFrag = PtxPdfContent_Text_Get(pTextList, 0);
                    if (pFrag != NULL)
                    {
                        nLen = PtxPdfContent_TextFragment_GetText(pFrag, NULL, 0);
                        if (nLen > 0)
                        {
                            szFragText = (TCHAR*)malloc(nLen * sizeof(TCHAR));
                            if (szFragText != NULL)
                            {
                                PtxPdfContent_TextFragment_GetText(pFrag, szFragText, nLen);

                                if (_tcscmp(szFragText, _T("This is a properly tagged heading")) == 0)
                                {
                                    if (copyAndTagTextElement(pTextElem, pSection, pGenerator, pOutPage, pOutDoc,
                                                              _T("H1"), NULL) != 0)
                                    {
                                        free(szFragText);
                                        Ptx_Release(pFrag);
                                        Ptx_Release(pTextList);
                                        goto cleanup;
                                    }
                                }
                                else if (_tcscmp(szFragText, _T("This is a properly tagged paragraph. Both heading ")
                                                             _T("and paragraph belong to a section.")) == 0)
                                {
                                    if (copyAndTagTextElement(pTextElem, pSection, pGenerator, pOutPage, pOutDoc,
                                                              _T("P"), &pParagraphNode) != 0)
                                    {
                                        free(szFragText);
                                        Ptx_Release(pFrag);
                                        Ptx_Release(pTextList);
                                        goto cleanup;
                                    }
                                }

                                free(szFragText);
                            }
                        }
                        Ptx_Release(pFrag);
                    }
                    Ptx_Release(pTextList);
                }
            }
        }
        else if (iType == ePtxPdfContent_ContentElementType_ImageElement)
        {
            /* Copy image element to output document */
            pOutElement = PtxPdfContent_ContentElement_Copy(pOutDoc, pInElement);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutElement, _T("Failed to copy image element. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());

            {
                TPtxPdfContent_ImageElement* pImgElem = (TPtxPdfContent_ImageElement*)pOutElement;
                TPtxGeomReal_Rectangle       boundingBox;
                TPtxGeomReal_AffineTransform transform;
                TPtxGeomReal_Quadrilateral   quad;

                GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
                    PtxPdfContent_ContentElement_GetBoundingBox(pOutElement, &boundingBox),
                    _T("Failed to get bounding box. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
                GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentElement_GetTransform(pOutElement, &transform),
                                                  _T("Failed to get transform. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                                  Ptx_GetLastError());
                GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
                    PtxGeomReal_AffineTransform_TransformRectangle(&transform, &boundingBox, &quad),
                    _T("Failed to transform rectangle. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

                /* Check if this is the logo image by approximate position */
                if (fabs(quad.BottomLeft.dX - 70.86) < 0.5 && fabs(quad.BottomLeft.dY - 632.65) < 0.5 &&
                    fabs(quad.TopRight.dX - 127.559) < 0.5 && fabs(quad.TopRight.dY - 689.34) < 0.5)
                {
                    TPtxPdfStructure_Node* pImgParent = (pParagraphNode != NULL) ? pParagraphNode : pSection;
                    if (copyAndTagImageElement(pImgElem, pGenerator, pOutPage, pOutDoc, _T("PdfTools AG Logo"),
                                               pImgParent) != 0)
                        goto cleanup;
                }
            }
        }
        else if (iType == ePtxPdfContent_ContentElementType_GroupElement)
        {
            /* Copy group element without content (recursive handling simplified) */
            pOutElement = (TPtxPdfContent_ContentElement*)PtxPdfContent_GroupElement_CopyWithoutContent(
                pOutDoc, (TPtxPdfContent_GroupElement*)pInElement);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutElement, _T("Failed to copy group element. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());
        }

        if (pInElement != NULL)
        {
            Ptx_Release(pInElement);
            pInElement = NULL;
        }
        if (pOutElement != NULL)
        {
            Ptx_Release(pOutElement);
            pOutElement = NULL;
        }
        PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);
    }

cleanup:
    if (pInElement != NULL)
        Ptx_Release(pInElement);
    if (pOutElement != NULL)
        Ptx_Release(pOutElement);
    if (pIterator != NULL)
        Ptx_Release(pIterator);
    if (pGenerator != NULL)
        PtxPdfContent_ContentGenerator_Close(pGenerator);
    if (pOutContent != NULL)
        Ptx_Release(pOutContent);
    if (pExtractor != NULL)
        Ptx_Release(pExtractor);
    if (pInContent != NULL)
        Ptx_Release(pInContent);

    return iReturnValue;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                      pInStream = NULL;
    TPtxSys_StreamDescriptor   inDescriptor;
    TPtxPdf_Document*          pInDoc     = NULL;
    FILE*                      pOutStream = NULL;
    TPtxSys_StreamDescriptor   outDescriptor;
    TPtxPdf_Document*          pOutDoc      = NULL;
    TPtxPdf_PageList*          pInPageList  = NULL;
    TPtxPdf_PageList*          pOutPageList = NULL;
    TPtxPdf_Page*              pInPage      = NULL;
    TPtxPdf_Page*              pOutPage     = NULL;
    TPtxPdf_Conformance        iConformance;
    TPtxGeomReal_Size          pageSize;
    TPtxPdf_Metadata*          pMetadata       = NULL;
    TPtxPdfNav_ViewerSettings* pViewerSettings = NULL;
    TCHAR*                     szInPath;
    TCHAR*                     szOutPath;

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

    szInPath  = argv[1];
    szOutPath = argv[2];

    /* Open input document */
    pInStream = _tfopen(szInPath, _T("rb"));
    GOTO_CLEANUP_IF_NULL(pInStream, _T("Failed to open input file \"%s\".\n"), szInPath);
    PtxSysCreateFILEStreamDescriptor(&inDescriptor, pInStream, 0);
    pInDoc = PtxPdf_Document_Open(&inDescriptor, _T(""));
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInDoc, _T("Input file \"%s\" cannot be opened. %s (ErrorCode: 0x%08x).\n"),
                                     szInPath, szErrorBuff, Ptx_GetLastError());

    /* Create output document */
    pOutStream = _tfopen(szOutPath, _T("wb+"));
    GOTO_CLEANUP_IF_NULL(pOutStream, _T("Failed to open output file \"%s\".\n"), szOutPath);
    PtxSysCreateFILEStreamDescriptor(&outDescriptor, pOutStream, 0);
    iConformance = PtxPdf_Document_GetConformance(pInDoc);
    pOutDoc      = PtxPdf_Document_Create(&outDescriptor, &iConformance, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutDoc, _T("Output file \"%s\" cannot be created. %s (ErrorCode: 0x%08x).\n"),
                                     szOutPath, szErrorBuff, Ptx_GetLastError());

    /* Copy document-wide data */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(copyDocumentData(pInDoc, pOutDoc),
                                      _T("Failed to copy document-wide data. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    /* Set document language and PDF/UA */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Document_SetLanguage(pOutDoc, _T("en")),
                                      _T("Failed to set language. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
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

    /* Set viewer settings */
    pViewerSettings = PtxPdf_Document_GetViewerSettings(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pViewerSettings, _T("Failed to get viewer settings. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfNav_ViewerSettings_SetDisplayDocumentTitle(pViewerSettings, TRUE),
                                      _T("Failed to set display document title. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    /* Get first input page */
    pInPageList = PtxPdf_Document_GetPages(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList, _T("Failed to get input pages. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pInPage = PtxPdf_PageList_Get(pInPageList, 0);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPage, _T("Failed to get first page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    /* Create empty output page with same size */
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pInPage, &pageSize),
                                      _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
    pOutPage = PtxPdf_Page_Create(pOutDoc, &pageSize);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPage, _T("Failed to create output page. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    /* Copy and tag content */
    if (copyAndTagContent(pInPage, pOutPage, pOutDoc) != 0)
        goto cleanup;

    /* Add page to output document */
    pOutPageList = PtxPdf_Document_GetPages(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageList, _T("Failed to get output pages. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_Add(pOutPageList, pOutPage),
                                      _T("Failed to add page to output. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pOutPage != NULL)
        Ptx_Release(pOutPage);
    if (pInPage != NULL)
        Ptx_Release(pInPage);
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
    if (pInStream != NULL)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 