/****************************************************************************
 *
 * File:            toolboxaddformfields.c
 *
 * Usage:           toolboxaddformfields <inputPath> <outputPath>
 *                  Example: Form2NoneNoTP.pdf out.pdf
 *                  
 * Title:           Add form field
 *                  
 * Description:     Add form fields to a PDF.
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
    printf("Usage: toolboxaddformfields <inputPath> <outputPath>.\n");
    printf("       Example: Form2NoneNoTP.pdf out.pdf\n");

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
int addCheckBox(TPtxPdf_Document* pOutDoc, const TCHAR* szId, BOOL bChecked, TPtxPdf_Page* pPage,
                TPtxGeomReal_Rectangle* pRect)
{
    TPtxPdfForms_CheckBox*     pCheckBox = NULL;
    TPtxPdfForms_Widget*       pWidget   = NULL;
    TPtxPdfForms_FieldNodeMap* pFormFields;
    TPtxPdfForms_WidgetList*   pWidgets;

    // Create a check box
    pCheckBox = PtxPdfForms_CheckBox_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pCheckBox, _T("Failed to create check box. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Set the check box's state (must be set before adding widget)
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_CheckBox_SetChecked(pCheckBox, bChecked),
                                      _T("Failed to set check box state. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Create a widget and add it to the page's widgets
    pWidget = PtxPdfForms_Field_AddNewWidget((TPtxPdfForms_Field*)pCheckBox, pRect);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidget, _T("Failed to create check box widget. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pWidgets = PtxPdf_Page_GetWidgets(pPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidgets, _T("Failed to get page widgets. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_WidgetList_Add(pWidgets, pWidget),
                                      _T("Failed to add widget to page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Add the check box to the document
    pFormFields = PtxPdf_Document_GetFormFields(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFormFields, _T("Failed to get form fields. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfForms_FieldNodeMap_Set(pFormFields, szId, (TPtxPdfForms_FieldNode*)pCheckBox),
        _T("Failed to add check box to form fields. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

cleanup:
    return iReturnValue;
}
int addComboBox(TPtxPdf_Document* pOutDoc, const TCHAR* szId, const TCHAR** pItemNames, int nItems,
                const TCHAR* szValue, TPtxPdf_Page* pPage, TPtxGeomReal_Rectangle* pRect)
{
    TPtxPdfForms_ComboBox*     pComboBox   = NULL;
    TPtxPdfForms_ChoiceItem*   pChosenItem = NULL;
    TPtxPdfForms_Widget*       pWidget     = NULL;
    TPtxPdfForms_FieldNodeMap* pFormFields;
    TPtxPdfForms_WidgetList*   pWidgets;
    BOOL                       bItemChosen = FALSE;

    // Create a combo box
    pComboBox = PtxPdfForms_ComboBox_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pComboBox, _T("Failed to create combo box. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Loop over all given item names (must be done before adding widget)
    for (int i = 0; i < nItems; i++)
    {
        // Create a new choice item
        TPtxPdfForms_ChoiceItem* pItem =
            PtxPdfForms_ChoiceField_AddNewItem((TPtxPdfForms_ChoiceField*)pComboBox, pItemNames[i]);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pItem, _T("Failed to add combo box item. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Check whether this is the chosen item name
        if (_tcscmp(szValue, pItemNames[i]) == 0)
        {
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_ComboBox_SetChosenItem(pComboBox, pItem),
                                              _T("Failed to set chosen item. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                              Ptx_GetLastError());
            bItemChosen = TRUE;
        }
    }

    if (!bItemChosen && szValue != NULL && _tcslen(szValue) > 0)
    {
        // If no item has been chosen then assume we want to set the editable item
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_ComboBox_SetCanEdit(pComboBox, TRUE),
                                          _T("Failed to set can edit. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_ComboBox_SetEditableItemName(pComboBox, szValue),
                                          _T("Failed to set editable item name. %s (ErrorCode: 0x%08x).\n"),
                                          szErrorBuff, Ptx_GetLastError());
    }

    // Create a widget and add it to the page's widgets
    pWidget = PtxPdfForms_Field_AddNewWidget((TPtxPdfForms_Field*)pComboBox, pRect);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidget, _T("Failed to create combo box widget. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pWidgets = PtxPdf_Page_GetWidgets(pPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidgets, _T("Failed to get page widgets. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_WidgetList_Add(pWidgets, pWidget),
                                      _T("Failed to add widget to page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Add the combo box to the document
    pFormFields = PtxPdf_Document_GetFormFields(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFormFields, _T("Failed to get form fields. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfForms_FieldNodeMap_Set(pFormFields, szId, (TPtxPdfForms_FieldNode*)pComboBox),
        _T("Failed to add combo box to form fields. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

cleanup:
    return iReturnValue;
}
int addListBox(TPtxPdf_Document* pOutDoc, const TCHAR* szId, const TCHAR** pItemNames, int nItems,
               const TCHAR** pChosenNames, int nChosen, TPtxPdf_Page* pPage, TPtxGeomReal_Rectangle* pRect)
{
    TPtxPdfForms_ListBox*        pListBox     = NULL;
    TPtxPdfForms_Widget*         pWidget      = NULL;
    TPtxPdfForms_ChoiceItemList* pChosenItems = NULL;
    TPtxPdfForms_FieldNodeMap*   pFormFields;
    TPtxPdfForms_WidgetList*     pWidgets;

    // Create a list box
    pListBox = PtxPdfForms_ListBox_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pListBox, _T("Failed to create list box. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Allow multiple selections
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_ListBox_SetAllowMultiSelect(pListBox, TRUE),
                                      _T("Failed to set multi-select. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());
    pChosenItems = PtxPdfForms_ListBox_GetChosenItems(pListBox);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pChosenItems, _T("Failed to get chosen items. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Loop over all given item names (must be done before adding widget)
    for (int i = 0; i < nItems; i++)
    {
        // Create a new choice item
        TPtxPdfForms_ChoiceItem* pItem =
            PtxPdfForms_ChoiceField_AddNewItem((TPtxPdfForms_ChoiceField*)pListBox, pItemNames[i]);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pItem, _T("Failed to add list box item. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Check whether to add to the chosen items
        for (int j = 0; j < nChosen; j++)
        {
            if (_tcscmp(pChosenNames[j], pItemNames[i]) == 0)
            {
                GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_ChoiceItemList_Add(pChosenItems, pItem),
                                                  _T("Failed to add chosen item. %s (ErrorCode: 0x%08x).\n"),
                                                  szErrorBuff, Ptx_GetLastError());
                break;
            }
        }
    }

    // Create a widget and add it to the page's widgets
    pWidget = PtxPdfForms_Field_AddNewWidget((TPtxPdfForms_Field*)pListBox, pRect);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidget, _T("Failed to create list box widget. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pWidgets = PtxPdf_Page_GetWidgets(pPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidgets, _T("Failed to get page widgets. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_WidgetList_Add(pWidgets, pWidget),
                                      _T("Failed to add widget to page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Add the list box to the document
    pFormFields = PtxPdf_Document_GetFormFields(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFormFields, _T("Failed to get form fields. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdfForms_FieldNodeMap_Set(pFormFields, szId, (TPtxPdfForms_FieldNode*)pListBox),
        _T("Failed to add list box to form fields. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());

cleanup:
    return iReturnValue;
}
int addRadioButtonGroup(TPtxPdf_Document* pOutDoc, const TCHAR* szId, const TCHAR** pButtonNames, int nButtons,
                        int iChosen, TPtxPdf_Page* pPage, TPtxGeomReal_Rectangle* pRect)
{
    TPtxPdfForms_RadioButtonGroup* pGroup = NULL;
    TPtxPdfForms_FieldNodeMap*     pFormFields;
    TPtxPdfForms_WidgetList*       pWidgets;
    double                         dButtonWidth;

    // Create a radio button group
    pGroup = PtxPdfForms_RadioButtonGroup_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pGroup, _T("Failed to create radio button group. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());

    // Get the page's widgets
    pWidgets = PtxPdf_Page_GetWidgets(pPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidgets, _T("Failed to get page widgets. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Compute the width of the sub-rectangles
    dButtonWidth = (pRect->dRight - pRect->dLeft) / nButtons;

    // Loop over all button names
    for (int i = 0; i < nButtons; i++)
    {
        // Compute the sub-rectangle for this button
        TPtxGeomReal_Rectangle buttonRect;
        buttonRect.dLeft   = pRect->dLeft + i * dButtonWidth;
        buttonRect.dBottom = pRect->dBottom;
        buttonRect.dRight  = pRect->dLeft + (i + 1) * dButtonWidth;
        buttonRect.dTop    = pRect->dTop;

        // Create the button and an associated widget
        TPtxPdfForms_RadioButton* pButton = PtxPdfForms_RadioButtonGroup_AddNewButton(pGroup, pButtonNames[i]);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pButton, _T("Failed to create radio button. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());
        TPtxPdfForms_Widget* pWidget = PtxPdfForms_RadioButton_AddNewWidget(pButton, &buttonRect);
        GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidget, _T("Failed to create radio button widget. %s (ErrorCode: 0x%08x).\n"),
                                         szErrorBuff, Ptx_GetLastError());

        // Check if this is the chosen button
        if (i == iChosen)
        {
            GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_RadioButtonGroup_SetChosenButton(pGroup, pButton),
                                              _T("Failed to set chosen button. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                              Ptx_GetLastError());
        }

        // Add the widget to the page's widgets
        GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_WidgetList_Add(pWidgets, pWidget),
                                          _T("Failed to add widget to page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                          Ptx_GetLastError());
    }

    // Add the radio button group to the document
    pFormFields = PtxPdf_Document_GetFormFields(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFormFields, _T("Failed to get form fields. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_FieldNodeMap_Set(pFormFields, szId, (TPtxPdfForms_FieldNode*)pGroup),
                                      _T("Failed to add radio button group to form fields. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

cleanup:
    return iReturnValue;
}
int addGeneralTextField(TPtxPdf_Document* pOutDoc, const TCHAR* szId, const TCHAR* szValue, TPtxPdf_Page* pPage,
                        TPtxGeomReal_Rectangle* pRect)
{
    TPtxPdfForms_GeneralTextField* pField  = NULL;
    TPtxPdfForms_Widget*           pWidget = NULL;
    TPtxPdfForms_FieldNodeMap*     pFormFields;
    TPtxPdfForms_WidgetList*       pWidgets;

    // Create a general text field
    pField = PtxPdfForms_GeneralTextField_Create(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pField, _T("Failed to create text field. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Set the text value (must be set before adding widget)
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_TextField_SetText((TPtxPdfForms_TextField*)pField, szValue),
                                      _T("Failed to set text value. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Create a widget and add it to the page's widgets
    pWidget = PtxPdfForms_Field_AddNewWidget((TPtxPdfForms_Field*)pField, pRect);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidget, _T("Failed to create text field widget. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    pWidgets = PtxPdf_Page_GetWidgets(pPage);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pWidgets, _T("Failed to get page widgets. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_WidgetList_Add(pWidgets, pWidget),
                                      _T("Failed to add widget to page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                      Ptx_GetLastError());

    // Add the field to the document
    pFormFields = PtxPdf_Document_GetFormFields(pOutDoc);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pFormFields, _T("Failed to get form fields. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdfForms_FieldNodeMap_Set(pFormFields, szId, (TPtxPdfForms_FieldNode*)pField),
                                      _T("Failed to add text field to form fields. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

cleanup:
    return iReturnValue;
}
int _tmain(int argc, TCHAR* argv[])
{
    FILE*                      pInStream = NULL;
    TPtxSys_StreamDescriptor   descriptor;
    TPtxPdf_Document*          pInDoc     = NULL;
    FILE*                      pOutStream = NULL;
    TPtxSys_StreamDescriptor   outDescriptor;
    TPtxPdf_Document*          pOutDoc       = NULL;
    TPtxPdf_PageList*          pInPageList   = NULL;
    TPtxPdf_PageList*          pOutPageList  = NULL;
    TPtxPdf_PageList*          pInPageRange  = NULL;
    TPtxPdf_PageList*          pOutPageRange = NULL;
    TPtxPdf_Page*              pInPage       = NULL;
    TPtxPdf_Page*              pOutPage      = NULL;
    TPtxPdf_PageCopyOptions*   pCopyOptions  = NULL;
    TPtxPdf_Conformance        iConformance;
    TPtxPdfForms_FieldNodeMap* pInFormFields  = NULL;
    TPtxPdfForms_FieldNodeMap* pOutFormFields = NULL;
    TCHAR*                     szInPath;
    TCHAR*                     szOutPath;

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

    // Copy all form fields from input to output
    pInFormFields  = PtxPdf_Document_GetFormFields(pInDoc);
    pOutFormFields = PtxPdf_Document_GetFormFields(pOutDoc);
    if (pInFormFields != NULL && pOutFormFields != NULL)
    {
        int iEnd = PtxPdfForms_FieldNodeMap_GetEnd(pInFormFields);
        for (int it = PtxPdfForms_FieldNodeMap_GetBegin(pInFormFields); it != iEnd;
             it     = PtxPdfForms_FieldNodeMap_GetNext(pInFormFields, it))
        {
            TCHAR  szKey[256];
            size_t nKeySize = PtxPdfForms_FieldNodeMap_GetKey(pInFormFields, it, szKey, ARRAY_SIZE(szKey));
            if (nKeySize > 0)
            {
                TPtxPdfForms_FieldNode* pInNode = PtxPdfForms_FieldNodeMap_GetValue(pInFormFields, it);
                if (pInNode != NULL)
                {
                    TPtxPdfForms_FieldNode* pOutNode = PtxPdfForms_FieldNode_Copy(pOutDoc, pInNode);
                    if (pOutNode != NULL)
                        PtxPdfForms_FieldNodeMap_Set(pOutFormFields, szKey, pOutNode);
                }
            }
        }
    }

    // Define page copy options
    pCopyOptions = PtxPdf_PageCopyOptions_New();
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pCopyOptions, _T("Failed to create copy options. %s (ErrorCode: 0x%08x).\n"),
                                     szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdf_PageCopyOptions_SetFormFields(pCopyOptions, ePtxPdfForms_FormFieldCopyStrategy_CopyAndUpdateWidgets),
        _T("Failed to set form field copy strategy. %s (ErrorCode: 0x%08x).\n"), szErrorBuff, Ptx_GetLastError());
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(
        PtxPdf_PageCopyOptions_SetUnsignedSignatures(pCopyOptions, ePtxPdf_CopyStrategy_Remove),
        _T("Failed to set unsigned signatures copy strategy. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
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

    // Copy first page
    pInPage = PtxPdf_PageList_Get(pInPageList, 0);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pInPage, _T("Failed to get first page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());
    pOutPage = PtxPdf_Page_Copy(pOutDoc, pInPage, pCopyOptions);
    GOTO_CLEANUP_IF_NULL_PRINT_ERROR(pOutPage, _T("Failed to copy first page. %s (ErrorCode: 0x%08x).\n"), szErrorBuff,
                                     Ptx_GetLastError());

    // Add different types of form fields to the output page
    {
        TPtxGeomReal_Rectangle rect;

        // Add CheckBox
        rect.dLeft   = 50;
        rect.dBottom = 300;
        rect.dRight  = 70;
        rect.dTop    = 320;
        if (addCheckBox(pOutDoc, _T("Check Box ID"), TRUE, pOutPage, &rect) != 0)
            goto cleanup;

        // Add ComboBox
        {
            const TCHAR* comboItems[] = {_T("item 1"), _T("item 2")};
            rect.dLeft                = 50;
            rect.dBottom              = 260;
            rect.dRight               = 210;
            rect.dTop                 = 280;
            if (addComboBox(pOutDoc, _T("Combo Box ID"), comboItems, 2, _T("item 1"), pOutPage, &rect) != 0)
                goto cleanup;
        }

        // Add ListBox
        {
            const TCHAR* listItems[]   = {_T("item 1"), _T("item 2"), _T("item 3")};
            const TCHAR* chosenItems[] = {_T("item 1"), _T("item 3")};
            rect.dLeft                 = 50;
            rect.dBottom               = 160;
            rect.dRight                = 210;
            rect.dTop                  = 240;
            if (addListBox(pOutDoc, _T("List Box ID"), listItems, 3, chosenItems, 2, pOutPage, &rect) != 0)
                goto cleanup;
        }

        // Add RadioButtonGroup
        {
            const TCHAR* radioItems[] = {_T("A"), _T("B"), _T("C")};
            rect.dLeft                = 50;
            rect.dBottom              = 120;
            rect.dRight               = 210;
            rect.dTop                 = 140;
            if (addRadioButtonGroup(pOutDoc, _T("Radio Button ID"), radioItems, 3, 0, pOutPage, &rect) != 0)
                goto cleanup;
        }

        // Add GeneralTextField
        rect.dLeft   = 50;
        rect.dBottom = 80;
        rect.dRight  = 210;
        rect.dTop    = 100;
        if (addGeneralTextField(pOutDoc, _T("Text ID"), _T("Text"), pOutPage, &rect) != 0)
            goto cleanup;
    }

    // Add page to output document
    GOTO_CLEANUP_IF_FALSE_PRINT_ERROR(PtxPdf_PageList_Add(pOutPageList, pOutPage),
                                      _T("Failed to add page to output document. %s (ErrorCode: 0x%08x).\n"),
                                      szErrorBuff, Ptx_GetLastError());

    // Copy remaining pages and append to output document
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