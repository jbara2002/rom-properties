/***************************************************************************
 * ROM Properties Page shell extension. (librpsecure/win32)                *
 * integrity_level.c: Integrity level manipulation for process tokens.     *
 *                                                                         *
 * Copyright (c) 2020 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

// NOTE: Using LocalAlloc()/LocalFree() here to prevent issues
// mixing and matching static and dynamic CRT versions.

#include "integrity_level.h"

// C includes.
#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

// Windows includes.
#include <sddl.h>
#include <tchar.h>

// stdboolx
#include "stdboolx.h"

/**
 * Check if we're running Windows Vista or later.
 * @return True if running Vista; false if not.
 */
static bool isRunningVista(void)
{
	// Are we running Windows Vista or later?
	// NOTE: Technically not thread-safe, but the worst that will
	// happen is two threads set isVista to the same value.
	static bool isVista = false;
	static bool hasCheckedVista = false;
	OSVERSIONINFO osvi;

	if (hasCheckedVista) {
		return isVista;
	}

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4996)
#endif /* _MSC_VER */
	// TODO: Use versionhelpers.h.
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	isVista = (GetVersionEx(&osvi) && osvi.dwMajorVersion >= 6);
#ifdef _MSC_VER
# pragma warning(pop)
#endif /* _MSC_VER */

	return isVista;
}

/**
 * Adjust a token's integrity level.
 * @param hToken Token.
 * @param level Integrity level. (SECURITY_MANDATORY_*_RID)
 * @return 0 on success; GetLastError() on error.
 */
static DWORD adjustTokenIntegrityLevel(HANDLE hToken, int level)
{
	PSID pIntegritySid = NULL;
	TOKEN_MANDATORY_LABEL tml;
	DWORD dwLastError;
	TCHAR szIntegritySid[20];

	// TODO: Verify the integrity level value?
	if (level < 0) {
		return ERROR_INVALID_PARAMETER;
	}
	_sntprintf(szIntegritySid, _countof(szIntegritySid), _T("S-1-16-%d"), level);

	// Based on Chromium's SetTokenIntegrityLevel().
	if (!ConvertStringSidToSid(szIntegritySid, &pIntegritySid)) {
		// Failed to convert the SID.
		goto out;
	}

	tml.Label.Attributes = SE_GROUP_INTEGRITY;
	tml.Label.Sid = pIntegritySid;

	// Set the process integrity level.
	SetLastError(ERROR_INVALID_PARAMETER);
	if (!SetTokenInformation(
		hToken,				// TokenHandle
		TokenIntegrityLevel,		// TokenInformationClass
		&tml,				// TokenInformation
		sizeof(TOKEN_MANDATORY_LABEL) +
		GetLengthSid(pIntegritySid)))	// TokenInformationLength
	{
		// Failed to set the process integrity level.
		dwLastError = GetLastError();
		goto out;
	}

	// Success!
	dwLastError = 0;

out:
	LocalFree(pIntegritySid);
	return dwLastError;
}

/**
 * Create a token with the specified integrity level.
 * This requires Windows Vista or later.
 *
 * Caller must call CloseHandle() on the token when done using it.
 *
 * @param level Integrity level. (SECURITY_MANDATORY_*_RID)
 * @return New token, or NULL on error.
 */
HANDLE CreateIntegrityLevelToken(int level)
{
	HANDLE hToken = NULL;
	HANDLE hNewToken = NULL;
	DWORD dwRet;

	// Are we running Windows Vista or later?
	if (!isRunningVista()) {
		// Not running Windows Vista or later.
		// Can't create a low-integrity token.
		return NULL;
	}

	// Get the current process's token.
	if (!OpenProcessToken(
		GetCurrentProcess(),	// ProcessHandle
		TOKEN_DUPLICATE |
		TOKEN_ADJUST_DEFAULT |
		TOKEN_QUERY |
		TOKEN_ASSIGN_PRIMARY,	// DesiredAccess
		&hToken))		// TokenHandle
	{
		// Unable to open the process token.
		goto out;
	}

	// Duplicate the token.
	if (!DuplicateTokenEx(
		hToken,			// hExistingToken
		0,			// dwDesiredAccess
		NULL,			// lpTokenAttributes
		SecurityImpersonation,	// ImpersonationLevel
		TokenPrimary,		// TokenType
		&hNewToken))		// phNewToken
	{
		// Unable to duplicate the token.
		goto out;
	}

	// Adjust the token's integrity level.
	dwRet = adjustTokenIntegrityLevel(hNewToken, level);
	if (dwRet != 0) {
		// Adjusting the token's integrity level failed.
		CloseHandle(hNewToken);
		hNewToken = NULL;
		goto out;
	}

out:
	if (hToken) {
		CloseHandle(hToken);
	}

	return hNewToken;
}

/**
 * Get the current process's integrity level.
 * @return Integrity level (SECURITY_MANDATORY_*_RID), or -1 on error.
 */
int GetProcessIntegrityLevel(void)
{
	// Reference: https://kb.digital-detective.net/display/BF/Understanding+and+Working+in+Protected+Mode+Internet+Explorer
	int ret = -1;
	BOOL bRet;

	HANDLE hToken = NULL;
	PTOKEN_MANDATORY_LABEL pTML = NULL;
	PUCHAR pucSidSubAuthorityCount;
	PDWORD pdwIntegrityLevel;
	DWORD dwLengthNeeded;

	// Are we running Windows Vista or later?
	if (!isRunningVista()) {
		// Not running Windows Vista or later.
		// Can't get the integrity level.
		return ret;
	}

	if (!OpenProcessToken(
		GetCurrentProcess(),	// ProcessHandle
		TOKEN_QUERY |
		TOKEN_QUERY_SOURCE,	// DesiredAccess
		&hToken))		// TokenHandle
	{
		// Failed to open the process token.
		// Assume low integrity is not supported.
		// TODO: Check GetLastError()?
		return ret;
	}

	// Get the integrity level.
	bRet = GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &dwLengthNeeded);
	assert(bRet == 0);	// should fail here
	assert(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
	if (bRet || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		// We didn't fail, or the error was incorrect.
		goto out;
	}

	pTML = LocalAlloc(LMEM_FIXED, dwLengthNeeded);
	assert(pTML != NULL);
	if (!pTML) {
		// malloc() failed.
		goto out;
	}

	if (!GetTokenInformation(hToken, TokenIntegrityLevel, pTML, dwLengthNeeded, &dwLengthNeeded)) {
		// GetTokenInformation() failed.
		goto out;
	}

	// Get the SID sub-authority value.
	// This is equivalent to the integrity level.
	pucSidSubAuthorityCount = GetSidSubAuthorityCount(pTML->Label.Sid);
	if (!pucSidSubAuthorityCount) {
		// SID is invalid.
		goto out;
	}
	pdwIntegrityLevel = GetSidSubAuthority(pTML->Label.Sid, *pucSidSubAuthorityCount - 1);
	if (!pdwIntegrityLevel) {
		// SID is invalid.
		goto out;
	}

	// Return the integrity level directly.
	ret = (int)*pdwIntegrityLevel;

out:
	LocalFree(pTML);
	if (hToken) {
		CloseHandle(hToken);
	}
	return ret;
}

/**
 * Adjust the current process's integrity level.
 *
 * References:
 * - https://github.com/chromium/chromium/blob/4e88a3c4fa53bf4d3622d07fd13f3812d835e40f/sandbox/win/src/restricted_token_utils.cc
 * - https://github.com/chromium/chromium/blob/master/sandbox/win/src/restricted_token_utils.cc
 *
 * @param level Integrity level. (SECURITY_MANDATORY_*_RID)
 * @return 0 on success; GetLastError() on error.
 */
DWORD SetProcessIntegrityLevel(int level)
{
	HANDLE hToken;
	DWORD dwRet;

	// Are we running Windows Vista or later?
	if (!isRunningVista()) {
		// Not running Windows Vista or later.
		// Can't set the process integrity level.
		// We'll pretend everything "just works" anyway.
		return ERROR_SUCCESS;
	}

	if (!OpenProcessToken(
		GetCurrentProcess(),	// ProcessHandle
		TOKEN_ADJUST_DEFAULT,	// DesiredAccess
		&hToken))		// TokenHandle
	{
		// OpenProcessToken() failed.
		return GetLastError();
	}

	// Adjust the token's integrity level.
	dwRet = adjustTokenIntegrityLevel(hToken, level);
	CloseHandle(hToken);
	return dwRet;
}
