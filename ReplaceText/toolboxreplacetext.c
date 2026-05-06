/****************************************************************************
 *
 * File:            toolboxreplacetext.c
 *
 * Usage:           toolboxreplacetext <inputPath> <outputPath>
 *                  Example: in.pdf out.pdf
 *                  
 * Title:           Replace text fragment in PDF
 *                  
 * Description:     For a given text, search through all text fragments on
 *                  all pages and replace the first matching fragment found.
 *                  Links, annotations, form fields, outlines, and logical
 *                  structure are discarded.
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
    printf("Usage: toolboxreplacetext <inputPath> <outputPath>.\n");
    printf("       Example: in.pdf out.pdf\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

// Global state: information about the found text fragment
BOOL                          bFragmentFound       = FALSE;
double                        dFoundFontSize       = 0.0;
double                        dFoundCharSpacing    = 0.0;
double                        dFoundWordSpacing    = 0.0;
double                        dFoundHorizScaling   = 0.0;
double                        dFoundRise           = 0.0;
TCHAR                         szFoundBaseFont[256] = {0};
TPtxGeomReal_AffineTransform  overallTransform;

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
int copyContent(TPtxPdfContent_Content* pInContent, TPtxPdfContent_Content* pOutContent,
                TPtxPdf_Document* pOutDoc, const TCHAR* szSearchString)
{
    TPtxPdfContent_ContentExtractor*         pExtractor       = NULL;
    TPtxPdfContent_ContentGenerator*         pGenerator       = NULL;
    TPtxPdfContent_ContentExtractorIterator* pIterator        = NULL;
    TPtxPdfContent_ContentElement*           pInElement       = NULL;
    TPtxPdfContent_ContentElement*           pOutElement      = NULL;
    TPtxPdfContent_GroupElement*             pOutGroupElem    = NULL;
    TPtxPdfContent_Group*                    pInGroup         = NULL;
    TPtxPdfContent_Group*                    pOutGroup        = NULL;
    TPtxPdfContent_Content*                  pInGroupContent  = NULL;
    TPtxPdfContent_Content*                  pOutGroupContent = NULL;

    // Create content extractor
    pExtractor = PtxPdfContent_ContentExtractor_New(pInContent);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pExtractor,
                                     _T("Failed to create content extractor. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Create content generator
    pGenerator = PtxPdfContent_ContentGenerator_New(pOutContent, FALSE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pGenerator,
                                     _T("Failed to create content generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Get iterator
    pIterator = PtxPdfContent_ContentExtractor_GetIterator(pExtractor);
    GOTO_CLEANUP_IF_NULL(pIterator, _T("Failed to get iterator.\n"));
    PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);

    // Iterate over all content elements
    while ((pInElement = PtxPdfContent_ContentExtractorIterator_GetValue(pIterator)) != NULL)
    {
        TPtxPdfContent_ContentElementType iType = PtxPdfContent_ContentElement_GetType(pInElement);
        BOOL bAppendElement = TRUE;

        if (iType == ePtxPdfContent_ContentElementType_GroupElement)
        {
            // Special treatment for group elements
            pOutGroupElem = PtxPdfContent_GroupElement_CopyWithoutContent(
                pOutDoc, (TPtxPdfContent_GroupElement*)pInElement);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutGroupElem,
                                             _T("Failed to copy group element. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());

            // Save transform for later restore
            TPtxGeomReal_AffineTransform savedTransform = overallTransform;

            // Update the transform with the group element's transform
            TPtxGeomReal_AffineTransform groupTransform;
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
                PtxPdfContent_ContentElement_GetTransform(pInElement, &groupTransform),
                _T("Failed to get group transform. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
            PtxGeomReal_AffineTransform_Concatenate(&overallTransform, &groupTransform);

            // Get input group content
            pInGroup = PtxPdfContent_GroupElement_GetGroup((TPtxPdfContent_GroupElement*)pInElement);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInGroup,
                                             _T("Failed to get input group. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());
            pInGroupContent = PtxPdfContent_Group_GetContent(pInGroup);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInGroupContent,
                                             _T("Failed to get input group content. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());

            // Get output group content
            pOutGroup = PtxPdfContent_GroupElement_GetGroup(pOutGroupElem);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutGroup,
                                             _T("Failed to get output group. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());
            pOutGroupContent = PtxPdfContent_Group_GetContent(pOutGroup);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutGroupContent,
                                             _T("Failed to get output group content. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());

            // Recursively copy content
            if (copyContent(pInGroupContent, pOutGroupContent, pOutDoc, szSearchString) != 0)
                goto cleanup;

            // Restore the transform
            overallTransform = savedTransform;

            // Append the group element
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
                PtxPdfContent_ContentGenerator_AppendContentElement(pGenerator,
                                                                     (TPtxPdfContent_ContentElement*)pOutGroupElem),
                _T("Failed to append group element. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

            Ptx_Release(pInGroup);
            pInGroup = NULL;
            Ptx_Release(pOutGroup);
            pOutGroup = NULL;
            Ptx_Release(pInGroupContent);
            pInGroupContent = NULL;
            Ptx_Release(pOutGroupContent);
            pOutGroupContent = NULL;
            Ptx_Release(pOutGroupElem);
            pOutGroupElem = NULL;
        }
        else
        {
            // Copy the content element to the output document
            pOutElement = PtxPdfContent_ContentElement_Copy(pOutDoc, pInElement);
            GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutElement,
                                             _T("Failed to copy content element. %s (ErrorCode: 0x%08x).\n"),
                                             szErrorBuff, Ptx_GetLastError());

            if (!bFragmentFound && iType == ePtxPdfContent_ContentElementType_TextElement)
            {
                // Special treatment for text elements: search for matching fragment
                TPtxPdfContent_Text* pText =
                    PtxPdfContent_TextElement_GetText((TPtxPdfContent_TextElement*)pOutElement);
                if (pText != NULL)
                {
                    int nFragCount = PtxPdfContent_Text_GetCount(pText);
                    for (int iFrag = nFragCount - 1; iFrag >= 0; iFrag--)
                    {
                        TPtxPdfContent_TextFragment* pFragment = PtxPdfContent_Text_Get(pText, iFrag);
                        if (pFragment != NULL)
                        {
                            // Get fragment text using two-call pattern
                            size_t nTextLen = PtxPdfContent_TextFragment_GetText(pFragment, NULL, 0);
                            if (nTextLen > 0)
                            {
                                TCHAR* szFragText = (TCHAR*)malloc(nTextLen * sizeof(TCHAR));
                                if (szFragText != NULL)
                                {
                                    PtxPdfContent_TextFragment_GetText(pFragment, szFragText, nTextLen);

                                    // Check if fragment text matches search string
                                    if (_tcscmp(szFragText, szSearchString) == 0)
                                    {
                                        // Found the fragment - save its properties
                                        bFragmentFound = TRUE;

                                        // Save font size
                                        dFoundFontSize = PtxPdfContent_TextFragment_GetFontSize(pFragment);

                                        // Save character spacing
                                        dFoundCharSpacing = PtxPdfContent_TextFragment_GetCharacterSpacing(pFragment);

                                        // Save word spacing
                                        dFoundWordSpacing = PtxPdfContent_TextFragment_GetWordSpacing(pFragment);

                                        // Save horizontal scaling
                                        dFoundHorizScaling = PtxPdfContent_TextFragment_GetHorizontalScaling(pFragment);

                                        // Save rise
                                        dFoundRise = PtxPdfContent_TextFragment_GetRise(pFragment);

                                        // Save base font name
                                        TPtxPdfContent_Font* pFragFont = PtxPdfContent_TextFragment_GetFont(pFragment);
                                        if (pFragFont != NULL)
                                        {
                                            size_t nFontNameLen =
                                                PtxPdfContent_Font_GetBaseFont(pFragFont, NULL, 0);
                                            if (nFontNameLen > 0 && nFontNameLen <= ARRAY_SIZE(szFoundBaseFont))
                                                PtxPdfContent_Font_GetBaseFont(pFragFont, szFoundBaseFont,
                                                                                ARRAY_SIZE(szFoundBaseFont));
                                            Ptx_Release(pFragFont);
                                        }

                                        // Update the overall transform with the fragment's transform
                                        TPtxGeomReal_AffineTransform fragTransform;
                                        PtxPdfContent_TextFragment_GetTransform(pFragment, &fragTransform);
                                        PtxGeomReal_AffineTransform_Concatenate(&overallTransform, &fragTransform);

                                        // Remove the found text fragment from the output
                                        PtxPdfContent_Text_Remove(pText, iFrag);

                                        free(szFragText);
                                        Ptx_Release(pFragment);
                                        break;
                                    }
                                    free(szFragText);
                                }
                            }
                            Ptx_Release(pFragment);
                        }
                    }
                    // Prevent appending an empty text element
                    if (PtxPdfContent_Text_GetCount(pText) == 0)
                        bAppendElement = FALSE;
                    Ptx_Release(pText);
                }
            }

            // Append the content element if not empty
            if (bAppendElement)
            {
                GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
                    PtxPdfContent_ContentGenerator_AppendContentElement(pGenerator, pOutElement),
                    _T("Failed to append content element. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                    Ptx_GetLastError());
            }

            Ptx_Release(pOutElement);
            pOutElement = NULL;
        }

        Ptx_Release(pInElement);
        pInElement = NULL;
        PtxPdfContent_ContentExtractorIterator_MoveNext(pIterator);
    }

cleanup:
    if (pOutGroupContent != NULL)
        Ptx_Release(pOutGroupContent);
    if (pInGroupContent != NULL)
        Ptx_Release(pInGroupContent);
    if (pOutGroup != NULL)
        Ptx_Release(pOutGroup);
    if (pInGroup != NULL)
        Ptx_Release(pInGroup);
    if (pOutGroupElem != NULL)
        Ptx_Release(pOutGroupElem);
    if (pOutElement != NULL)
        Ptx_Release(pOutElement);
    if (pInElement != NULL)
        Ptx_Release(pInElement);
    if (pIterator != NULL)
        Ptx_Release(pIterator);
    if (pGenerator != NULL)
        PtxPdfContent_ContentGenerator_Close(pGenerator);
    if (pExtractor != NULL)
        Ptx_Release(pExtractor);

    return iReturnValue;
}
int addText(TPtxPdf_Document* pOutDoc, TPtxPdf_Page* pOutPage, const TCHAR* szReplString)
{
    TPtxPdfContent_Content*          pContent       = NULL;
    TPtxPdfContent_ContentGenerator* pContentGen    = NULL;
    TPtxPdfContent_Text*             pText          = NULL;
    TPtxPdfContent_TextGenerator*    pTextGenerator = NULL;
    TPtxPdfContent_Font*             pFont          = NULL;
    TCHAR                            szFamily[256]  = {0};
    TCHAR*                           szStyle        = NULL;

    // Heuristic to map the extracted font base name to a font name and font family
    _tcscpy(szFamily, szFoundBaseFont);
    TCHAR* pDash = _tcsrchr(szFamily, _T('-'));
    if (pDash != NULL)
    {
        *pDash  = _T('\0');
        szStyle = pDash + 1;
    }

    // Create a new font object
    pFont = PtxPdfContent_Font_CreateFromSystem(pOutDoc, szFamily, szStyle, TRUE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFont, _T("Failed to create font. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Create a new text object
    pText = PtxPdfContent_Text_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pText, _T("Failed to create text object. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Create a text generator and set the original fragment's properties
    pTextGenerator = PtxPdfContent_TextGenerator_New(pText, pFont, dFoundFontSize, NULL);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTextGenerator,
                                     _T("Failed to create text generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Set original fragment properties
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_TextGenerator_SetCharacterSpacing(pTextGenerator, dFoundCharSpacing),
        _T("Failed to set character spacing. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_TextGenerator_SetWordSpacing(pTextGenerator, dFoundWordSpacing),
        _T("Failed to set word spacing. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfContent_TextGenerator_SetHorizontalScaling(pTextGenerator, dFoundHorizScaling),
        _T("Failed to set horizontal scaling. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_SetRise(pTextGenerator, dFoundRise),
                                      _T("Failed to set rise. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Show the replacement string
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_TextGenerator_Show(pTextGenerator, szReplString),
                                      _T("Failed to show replacement text. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    // Close text generator
    PtxPdfContent_TextGenerator_Close(pTextGenerator);
    pTextGenerator = NULL;

    // Create content generator
    pContent = PtxPdf_Page_GetContent(pOutPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContent, _T("Failed to get page content. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pContentGen = PtxPdfContent_ContentGenerator_New(pContent, FALSE);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pContentGen,
                                     _T("Failed to create content generator. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Apply the computed transform
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_Transform(pContentGen, &overallTransform),
                                      _T("Failed to apply transform. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    // Paint the new text
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfContent_ContentGenerator_PaintText(pContentGen, pText),
                                      _T("Failed to paint text. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

cleanup:
    if (pTextGenerator != NULL)
        PtxPdfContent_TextGenerator_Close(pTextGenerator);
    if (pContentGen != NULL)
        PtxPdfContent_ContentGenerator_Close(pContentGen);
    if (pContent != NULL)
        Ptx_Release(pContent);
    if (pText != NULL)
        Ptx_Release(pText);
    if (pFont != NULL)
        Ptx_Release(pFont);

    return iReturnValue;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                    pInStream    = NULL;
    TPtxSys_StreamDescriptor inDescriptor;
    TPtxPdf_Document*        pInDoc       = NULL;
    FILE*                    pOutStream   = NULL;
    TPtxSys_StreamDescriptor outDescriptor;
    TPtxPdf_Document*        pOutDoc      = NULL;
    TPtxPdf_PageList*        pInPageList  = NULL;
    TPtxPdf_PageList*        pOutPageList = NULL;
    TPtxPdf_Page*            pInPage      = NULL;
    TPtxPdf_Page*            pOutPage     = NULL;
    TPtxPdfContent_Content*  pInContent   = NULL;
    TPtxPdfContent_Content*  pOutContent  = NULL;
    TPtxPdf_Conformance      iConformance;
    TCHAR*                   szInPath;
    TCHAR*                   szOutPath;
    const TCHAR*             szSearchString = _T("Muster Company AG");
    const TCHAR*             szReplString   = _T("Replacement String");


    setlocale(LC_CTYPE, "");


    // Check command line parameters
    if (argc < 3 || argc > 3)
    {
        return Usage();
    }

    // Initialize library
    Ptx_Initialize();

    // Set and check license key
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(Ptx_Sdk_Initialize(_T("insert-license-key-here"), NULL),
                                      _T("Failed to set license key. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    szInPath  = argv[1];
    szOutPath = argv[2];

    // Initialize the overall transform to identity
    PtxGeomReal_AffineTransform_GetIdentity(&overallTransform);

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

    // Get page lists
    pInPageList = PtxPdf_Document_GetPages(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPageList,
                                     _T("Failed to get the pages of the input document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pOutPageList = PtxPdf_Document_GetPages(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPageList,
                                     _T("Failed to get the pages of the output document. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Process each page
    for (int iPage = 0; iPage < PtxPdf_PageList_GetCount(pInPageList); iPage++)
    {
        TPtxGeomReal_Size pageSize;

        // Get input page
        pInPage = PtxPdf_PageList_Get(pInPageList, iPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPage, _T("Failed to get page %d. %s (ErrorCode: 0x%08x).\n"),
                                         iPage + 1, szErrorBuff, Ptx_GetLastError());

        // Get page size
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_Page_GetSize(pInPage, &pageSize),
                                          _T("Failed to get page size. %s (ErrorCode: 0x%08x).\n"),
                                          szErrorBuff, Ptx_GetLastError());

        // Create empty output page with same size
        pOutPage = PtxPdf_Page_Create(pOutDoc, &pageSize);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPage, _T("Failed to create output page. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Get input and output content
        pInContent = PtxPdf_Page_GetContent(pInPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInContent,
                                         _T("Failed to get input page content. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        pOutContent = PtxPdf_Page_GetContent(pOutPage);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutContent,
                                         _T("Failed to get output page content. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Copy content and search for string
        if (copyContent(pInContent, pOutContent, pOutDoc, szSearchString) != 0)
            goto cleanup;

        // If the text was found and deleted, add the replacement text
        if (bFragmentFound)
        {
            if (addText(pOutDoc, pOutPage, szReplString) != 0)
                goto cleanup;
        }

        // Add page to output document
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_Add(pOutPageList, pOutPage),
                                          _T("Failed to add page to output document. %s (ErrorCode: 0x%08x).\n"),
                                          szErrorBuff, Ptx_GetLastError());

        Ptx_Release(pInContent);
        pInContent = NULL;
        Ptx_Release(pOutContent);
        pOutContent = NULL;
        Ptx_Release(pOutPage);
        pOutPage = NULL;
        Ptx_Release(pInPage);
        pInPage = NULL;
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pOutContent != NULL)
        Ptx_Release(pOutContent);
    if (pInContent != NULL)
        Ptx_Release(pInContent);
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
    if (pInStream)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 