/****************************************************************************
 *
 * File:            toolboxtraversedocumentstructure.c
 *
 * Usage:           toolboxtraversedocumentstructure <inputPath>
 *                  Example: in.pdf
 *                  
 * Title:           Traverse the document structure
 *                  
 * Description:     Traverse the logical structure of a
 *                  tagged PDF file.
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
    printf("Usage: toolboxtraversedocumentstructure <inputPath>.\n");
    printf("       Example: in.pdf\n");

    printf("return codes:\n");
    printf("    0: Successful completion\n");
    printf("    1: Execution failed\n");

    return 1;
}

size_t nBufSize;
TCHAR  szErrorBuff[1024];
int    iReturnValue = 0;

void printProperty(int iLevel, const TCHAR* szName, const TCHAR* szValue)
{
    for (int i = 0; i < iLevel * 2; i++)
        _tprintf(_T(" "));
    _tprintf(_T("%s: '%s'\n"), szName, szValue ? szValue : _T(""));
}
TCHAR* getStringProperty(size_t (*getter)(void*, TCHAR*, size_t), void* pObj)
{
    size_t nLen = getter(pObj, NULL, 0);
    if (nLen == 0)
        return NULL;
    TCHAR* szBuf = (TCHAR*)malloc(nLen * sizeof(TCHAR));
    if (szBuf != NULL)
        getter(pObj, szBuf, nLen);
    return szBuf;
}
void printNodeRecursively(TPtxPdfStructure_Node* pNode, int iLevel)
{
    TCHAR*                     szTag     = NULL;
    TCHAR*                     szAltText = NULL;
    TCHAR*                     szActual  = NULL;
    TCHAR*                     szAbbrev  = NULL;
    TCHAR*                     szLang    = NULL;
    TPtxPdfStructure_NodeList* pChildren = NULL;

    // Get tag
    szTag = getStringProperty((size_t(*)(void*, TCHAR*, size_t))PtxPdfStructure_Node_GetTag, pNode);
    printProperty(iLevel, _T("Tag"), szTag);

    // Get alternate text
    szAltText = getStringProperty((size_t(*)(void*, TCHAR*, size_t))PtxPdfStructure_Node_GetAlternateText, pNode);
    printProperty(iLevel, _T("Alternative text"), szAltText);

    // Get actual text
    szActual = getStringProperty((size_t(*)(void*, TCHAR*, size_t))PtxPdfStructure_Node_GetActualText, pNode);
    printProperty(iLevel, _T("Actual text"), szActual);

    // Get abbreviation
    szAbbrev = getStringProperty((size_t(*)(void*, TCHAR*, size_t))PtxPdfStructure_Node_GetAbbreviation, pNode);
    printProperty(iLevel, _T("Abbreviation"), szAbbrev);

    // Get language
    szLang = getStringProperty((size_t(*)(void*, TCHAR*, size_t))PtxPdfStructure_Node_GetLanguage, pNode);
    printProperty(iLevel, _T("Language"), szLang);

    // Traverse children
    pChildren = PtxPdfStructure_Node_GetChildren(pNode);
    if (pChildren != NULL)
    {
        int nChildCount = PtxPdfStructure_NodeList_GetCount(pChildren);
        for (int i = 0; i < nChildCount; i++)
        {
            TPtxPdfStructure_Node* pChild = PtxPdfStructure_NodeList_Get(pChildren, i);
            if (pChild != NULL)
            {
                printNodeRecursively(pChild, iLevel + 1);
                Ptx_Release(pChild);
            }
        }
        Ptx_Release(pChildren);
    }

    if (szTag != NULL)
        free(szTag);
    if (szAltText != NULL)
        free(szAltText);
    if (szActual != NULL)
        free(szActual);
    if (szAbbrev != NULL)
        free(szAbbrev);
    if (szLang != NULL)
        free(szLang);
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                      pInStream = NULL;
    TPtxSys_StreamDescriptor   descriptor;
    TPtxPdf_Document*          pInDoc    = NULL;
    TPtxPdfStructure_Tree*     pTree     = NULL;
    TPtxPdfStructure_NodeList* pChildren = NULL;
    TCHAR*                     szInPath;

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

    // Get the structure tree
    pTree = PtxPdfStructure_Tree_New(pInDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pTree, _T("Failed to get structure tree. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Get children of tree
    pChildren = PtxPdfStructure_Tree_GetChildren(pTree);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pChildren, _T("Failed to get tree children. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Traverse all children
    {
        int nChildCount = PtxPdfStructure_NodeList_GetCount(pChildren);
        for (int i = 0; i < nChildCount; i++)
        {
            TPtxPdfStructure_Node* pChild = PtxPdfStructure_NodeList_Get(pChildren, i);
            if (pChild != NULL)
            {
                printNodeRecursively(pChild, 0);
                Ptx_Release(pChild);
            }
        }
    }

    _tprintf(_T("Execution successful.\n"));

cleanup:
    if (pChildren != NULL)
        Ptx_Release(pChildren);
    if (pTree != NULL)
        Ptx_Release(pTree);
    if (pInDoc != NULL)
        PtxPdf_Document_Close(pInDoc);
    if (pInStream)
        fclose(pInStream);
    Ptx_Uninitialize();

    return iReturnValue;
} 