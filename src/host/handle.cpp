/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/

#include "precomp.h"

#include "handle.h"

#include "misc.h"
#include "output.h"
#include "srvinit.h"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

void LockConsole()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole();
}

void UnlockConsole()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.GetCSRecursionCount() == 1)
    {
        ProcessCtrlEvents();
    }
    else
    {
        gci.UnlockConsole();
    }
}

// Routine Description:
// - This routine allocates and initialized a console and its associated
//   data - input buffer and screen buffer.
// - NOTE: Will read global ServiceLocator::LocateGlobals().getConsoleInformation expecting Settings to already be filled.
// Arguments:
// - title - Window Title to display
// Return Value:
// - STATUS_SUCCESS if successful.
[[nodiscard]]
NTSTATUS CONSOLE_INFORMATION::AllocateConsole(const std::wstring_view title)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // Synchronize flags
    WI_SetFlagIf(gci.Flags, CONSOLE_AUTO_POSITION, !!gci.GetAutoPosition());
    WI_SetFlagIf(gci.Flags, CONSOLE_QUICK_EDIT_MODE, !!gci.GetQuickEdit());
    WI_SetFlagIf(gci.Flags, CONSOLE_HISTORY_NODUP, !!gci.GetHistoryNoDup());

    Selection* const pSelection = &Selection::Instance();
    pSelection->SetLineSelection(!!gci.GetLineSelection());

    SetConsoleCPInfo(TRUE);
    SetConsoleCPInfo(FALSE);

    // Initialize input buffer.
    try
    {
        gci.pInputBuffer = new InputBuffer();
    }
    catch(...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    try
    {
        gci.SetTitle(title);
        gci.SetOriginalTitle(std::wstring(TranslateConsoleTitle(gci.GetTitle().c_str(), TRUE, FALSE)));
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    NTSTATUS Status = DoCreateScreenBuffer();
    if (!NT_SUCCESS(Status))
    {
        goto ErrorExit2;
    }

    gci.pCurrentScreenBuffer = gci.ScreenBuffers;

    gci.GetActiveOutputBuffer().ScrollScale = gci.GetScrollScale();

    gci.ConsoleIme.RefreshAreaAttributes();

    if (NT_SUCCESS(Status))
    {
        return STATUS_SUCCESS;
    }

    RIPMSG1(RIP_WARNING, "Console init failed with status 0x%x", Status);

    delete gci.ScreenBuffers;
    gci.ScreenBuffers = nullptr;

ErrorExit2:
    delete gci.pInputBuffer;

    return Status;
}
