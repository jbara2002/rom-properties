/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * KeyManagerTab.hpp: Key Manager tab for rp-config.                       *
 *                                                                         *
 * Copyright (c) 2016-2021 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "KeyManagerTab.hpp"
#include "res/resource.h"
#include "../FontHandler.hpp"

// KeyStore
#include "KeyStoreWin32.hpp"

// IListView and other undocumented stuff.
#include "libwin32common/sdk/IListView.hpp"
#include "KeyStore_OwnerDataCallback.hpp"

// libwin32common
#include "libwin32common/SubclassWindow.h"
using LibWin32Common::WTSSessionNotification;

// librpbase, librpfile
#include "librpbase/crypto/KeyManager.hpp"
using namespace LibRpBase;
using namespace LibRpFile;

// libromdata
#include "libromdata/disc/WiiPartition.hpp"
#include "libromdata/crypto/CtrKeyScrambler.hpp"
#include "libromdata/crypto/N3DSVerifyKeys.hpp"
using namespace LibRomData;

// C++ STL classes.
using std::string;
using std::wstring;

class KeyManagerTabPrivate
{
	public:
		KeyManagerTabPrivate();
		~KeyManagerTabPrivate();

	private:
		RP_DISABLE_COPY(KeyManagerTabPrivate)

	public:
		/**
		 * Initialize the UI.
		 */
		void initUI(void);

	public:
		/**
		 * Reset the configuration.
		 */
		void reset(void);

		/**
		 * Save the configuration.
		 */
		void save(void);

	public:
		/**
		 * Dialog procedure.
		 * @param hDlg
		 * @param uMsg
		 * @param wParam
		 * @param lParam
		 */
		static INT_PTR CALLBACK dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

		/**
		 * Property sheet callback procedure.
		 * @param hWnd
		 * @param uMsg
		 * @param ppsp
		 */
		static UINT CALLBACK callbackProc(HWND hWnd, UINT uMsg, LPPROPSHEETPAGE ppsp);

		/**
		 * ListView subclass procedure.
		 * @param hWnd		Control handle.
		 * @param uMsg		Message.
		 * @param wParam	WPARAM
		 * @param lParam	LPARAM
		 * @param uIdSubclass	Subclass ID. (usually the control ID)
		 * @param dwRefData	KeyManagerTabPrivate*
		 * @return
		 */
		static LRESULT CALLBACK ListViewSubclassProc(
			HWND hWnd, UINT uMsg,
			WPARAM wParam, LPARAM lParam,
			UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

		/**
		 * ListView EDIT control subclass procedure.
		 * @param hWnd		Control handle.
		 * @param uMsg		Message.
		 * @param wParam	WPARAM
		 * @param lParam	LPARAM
		 * @param uIdSubclass	Subclass ID. (usually the control ID)
		 * @param dwRefData	KeyManagerTabPrivate*
		 * @return
		 */
		static LRESULT CALLBACK ListViewEditSubclassProc(
			HWND hWnd, UINT uMsg,
			WPARAM wParam, LPARAM lParam,
			UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

	public:
		// Property sheet.
		HPROPSHEETPAGE hPropSheetPage;
		HWND hWndPropSheet;

	public:
		// "Import" popup menu.
		HMENU hMenuImport;	// Must be deleted using DestroyMenu().

		// KeyStore.
		KeyStoreWin32 *keyStore;
		KeyStore_OwnerDataCallback *keyStore_ownerDataCallback;

		// Font Handler.
		FontHandler fontHandler;

		// EDIT box for ListView.
		HWND hEditBox;
		int iEditItem;		// Item being edited. (-1 for none)
		bool bCancelEdit;	// True if the edit is being cancelled.
		bool bAllowKanji;	// Allow kanji in the editor.

		// Is this COMCTL32.dll v6.10 or later?
		bool isComCtl32_v610;

		// wtsapi32.dll for Remote Desktop status. (WinXP and later)
		WTSSessionNotification wts;

		// Icons for the "Valid?" column.
		// NOTE: "?" and "X" are copies from User32.
		// Checkmark is a PNG image loaded from a resource.
		int iconSize;		// NOTE: Needs to be SIGNED to prevent issues with negative coordinates.
		HICON hIconUnknown;	// "?" (USER32.dll,-102)
		HICON hIconInvalid;	// "X" (USER32.dll,-103)
		HICON hIconGood;	// Checkmark

		// Alternate row color.
		COLORREF colorAltRow;
		HBRUSH hbrAltRow;

		/**
		 * ListView GetDispInfo function.
		 * @param plvdi	[in/out] NMLVDISPINFO
		 * @return True if handled; false if not.
		 */
		inline BOOL ListView_GetDispInfo(NMLVDISPINFO *plvdi);

		/**
		 * ListView CustomDraw function.
		 * @param plvcd	[in/out] NMLVCUSTOMDRAW
		 * @return Return value.
		 */
		inline int ListView_CustomDraw(NMLVCUSTOMDRAW *plvcd);

		/**
		 * Load images.
		 */
		void loadImages(void);

	public:
		/** "Import" menu actions. **/

		// Starting directory for importing keys.
		// TODO: Save this in the configuration file?
		tstring ts_keyFileDir;

		/**
		 * Update ts_keyFileDir.
		 * @param tfilename New filename.
		 */
		inline void updateKeyFileDir(const tstring &tfilename)
		{
			// Remove everything after the first backslash.
			// NOTE: If this is the root directory, the backslash is left intact.
			// Otherwise, the backslash is removed.
			ts_keyFileDir = tfilename;
			size_t bspos = ts_keyFileDir.rfind(_T('\\'));
			if (bspos != string::npos) {
				if (bspos > 2) {
					ts_keyFileDir.resize(bspos);
				} else if (bspos == 2) {
					ts_keyFileDir.resize(3);
				}
			}
		}

		/**
		 * Import keys from Wii keys.bin. (BootMii format)
		 */
		void importWiiKeysBin(void);

		/**
		 * Import keys from Wii U otp.bin.
		 */
		void importWiiUOtpBin(void);

		/**
		 * Import keys from 3DS boot9.bin.
		 */
		void import3DSboot9bin(void);

		/**
		 * Import keys from 3DS aeskeydb.bin.
		 */
		void import3DSaeskeydb(void);
};

/** KeyManagerTabPrivate **/

KeyManagerTabPrivate::KeyManagerTabPrivate()
	: hPropSheetPage(nullptr)
	, hWndPropSheet(nullptr)
	, hMenuImport(nullptr)
	, keyStore(new KeyStoreWin32(nullptr))
	, keyStore_ownerDataCallback(nullptr)
	, fontHandler(nullptr)
	, hEditBox(nullptr)
	, iEditItem(-1)
	, bCancelEdit(false)
	, bAllowKanji(false)
	, isComCtl32_v610(false)
	, iconSize(0)
	, hIconUnknown(nullptr)
	, hIconInvalid(nullptr)
	, hIconGood(nullptr)
	, colorAltRow(0)
	, hbrAltRow(nullptr)
{
	// Load images.
	loadImages();

	// Initialize the alternate row color.
	colorAltRow = LibWin32Common::getAltRowColor();
	hbrAltRow = CreateSolidBrush(colorAltRow);

	// Check the COMCTL32.DLL version.
	isComCtl32_v610 = LibWin32Common::isComCtl32_v610();
}

KeyManagerTabPrivate::~KeyManagerTabPrivate()
{
	if (hMenuImport) {
		DestroyMenu(hMenuImport);
	}

	delete keyStore;
	if (keyStore_ownerDataCallback) {
		keyStore_ownerDataCallback->Release();
	}

	// Icons.
	if (hIconUnknown) {
		DestroyIcon(hIconUnknown);
	}
	if (hIconInvalid) {
		DestroyIcon(hIconInvalid);
	}
	if (hIconGood) {
		DestroyIcon(hIconGood);
	}

	// Alternate row color.
	if (hbrAltRow) {
		DeleteBrush(hbrAltRow);
	}
}

/**
 * Initialize the UI.
 */
void KeyManagerTabPrivate::initUI(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	// Initialize the fonts.
	fontHandler.setWindow(hWndPropSheet);

	// Get the required controls.
	HWND hBtnImport = GetDlgItem(hWndPropSheet, IDC_KEYMANAGER_IMPORT);
	HWND hListView = GetDlgItem(hWndPropSheet, IDC_KEYMANAGER_LIST);
	assert(hBtnImport != nullptr);
	assert(hListView != nullptr);
	if (!hBtnImport || !hListView)
		return;

	if (isComCtl32_v610) {
		// COMCTL32 is v6.10 or later. Use BS_SPLITBUTTON.
		// (Windows Vista or later)
		LONG lStyle = GetWindowLong(hBtnImport, GWL_STYLE);
		lStyle |= BS_SPLITBUTTON;
		SetWindowLong(hBtnImport, GWL_STYLE, lStyle);
		BUTTON_SPLITINFO bsi;
		bsi.mask = BCSIF_STYLE;
		bsi.uSplitStyle = BCSS_NOSPLIT;
		Button_SetSplitInfo(hBtnImport, &bsi);
	} else {
		// COMCTL32 is older than v6.10. Use a regular button.
		// NOTE: The Unicode down arrow doesn't show on on Windows XP.
		// Maybe we *should* use ownerdraw...
		SetWindowText(hBtnImport, U82T_c(C_("KeyManagerTab", "I&mport...")));
	}

	// Initialize the ListView.
	// Set full row selection.
	DWORD dwExStyle;
	if (!GetSystemMetrics(SM_REMOTESESSION)) {
		// Not RDP (or is RemoteFX): Enable double buffering.
		dwExStyle = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
	} else {
		// RDP: Disable double buffering to reduce bandwidth usage.
		dwExStyle = LVS_EX_FULLROWSELECT;
	}
	ListView_SetExtendedListViewStyle(hListView, dwExStyle);

	// Set the virtual list item count.
	ListView_SetItemCountEx(hListView, keyStore->totalKeyCount(),
		LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

	// Column title.
	tstring tsColTitle;

	// tr: Column 0: Key Name.
	tsColTitle = U82T_c(C_("KeyManagerTab", "Key Name"));
	LVCOLUMN lvCol;
	memset(&lvCol, 0, sizeof(lvCol));
	lvCol.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
	lvCol.fmt = LVCFMT_LEFT;
	lvCol.pszText = const_cast<LPTSTR>(tsColTitle.c_str());
	ListView_InsertColumn(hListView, 0, &lvCol);

	// tr: Column 1: Value.
	tsColTitle = U82T_c(C_("KeyManagerTab", "Value"));
	lvCol.pszText = const_cast<LPTSTR>(tsColTitle.c_str());
	ListView_InsertColumn(hListView, 1, &lvCol);

	// tr: Column 2: Verification status.
	tsColTitle = U82T_c(C_("KeyManagerTab", "Valid?"));
	lvCol.pszText = const_cast<LPTSTR>(tsColTitle.c_str());
	ListView_InsertColumn(hListView, 2, &lvCol);

	if (isComCtl32_v610) {
		// Set the IOwnerDataCallback.
		bool hasIListView = false;

		// Check for Windows 7 IListView first.
		{
			IListView_Win7 *pListView = nullptr;
			ListView_QueryInterface(hListView, IID_IListView_Win7, &pListView);
			if (pListView) {
				// IListView obtained.
				keyStore_ownerDataCallback = new KeyStore_OwnerDataCallback(keyStore);
				pListView->SetOwnerDataCallback(keyStore_ownerDataCallback);
				pListView->Release();
				hasIListView = true;
			}
		}

		// If that failed, check for Windows Vista IListView.
		if (!hasIListView) {
			IListView_WinVista *pListView = nullptr;
			ListView_QueryInterface(hListView, IID_IListView_WinVista, &pListView);
			if (pListView) {
				// IListView obtained.
				keyStore_ownerDataCallback = new KeyStore_OwnerDataCallback(keyStore);
				pListView->SetOwnerDataCallback(keyStore_ownerDataCallback);
				pListView->Release();
				hasIListView = true;
			}
		}

		if (hasIListView) {
			// Create groups for each section.
			// NOTE: We have to use the Vista+ LVGROUP definition.
			// NOTE: LVGROUP always uses Unicode strings.
#if _WIN32_WINNT < 0x0600
# error Windows Vista SDK or later is required.
#endif
			LVGROUP lvGroup;
			lvGroup.cbSize = sizeof(lvGroup);
			lvGroup.mask = LVGF_ALIGN | LVGF_GROUPID | LVGF_HEADER | LVGF_ITEMS;
			lvGroup.uAlign = LVGA_HEADER_LEFT;
			for (int sectIdx = 0; sectIdx < keyStore->sectCount(); sectIdx++) {
				const wstring sectName = U82W_c(keyStore->sectName(sectIdx));
				lvGroup.iGroupId = sectIdx;
				lvGroup.pszHeader = const_cast<LPWSTR>(sectName.c_str());
				lvGroup.cItems = keyStore->keyCount(sectIdx);
				ListView_InsertGroup(hListView, sectIdx, &lvGroup);
			}
			ListView_EnableGroupView(hListView, true);
		}
	}

	// Determine the maximum width of columns 0 and 1.
	// This is needed because LVSCW_AUTOSIZE_USEHEADER doesn't
	// work with LVS_OWNERDATA.
	// Reference: https://stackoverflow.com/questions/9255540/how-auto-size-the-columns-width-of-a-list-view-in-virtual-mode
	// TODO: Determine the correct padding.
	// 8,12 seems to be right on both XP and 7...
	// TODO: If the user double-clicks the column splitter, it will
	// resize based on the displayed rows, not all rows.
	static const int column_padding[2] = {8, 12};
	int column_width[2] = {0, 0};

	// Make sure the "Value" column is at least 32 characters wide.
	// NOTE: ListView_GetStringWidth() doesn't adjust for the monospaced font.
	SIZE szValue;
	HFONT hFontMono = fontHandler.monospacedFont();
	int ret = LibWin32Common::measureTextSize(hListView, hFontMono, _T("0123456789ABCDEF0123456789ABCDEF"), &szValue);
	assert(ret == 0);
	if (ret == 0) {
		column_width[1] = szValue.cx + column_padding[1];
	}
	//column_width[1] = ListView_GetStringWidth(hListView, _T("0123456789ABCDEF0123456789ABCDEF")) + column_padding[1];

	for (int i = keyStore->totalKeyCount()-1; i >= 0; i--) {
		const KeyStoreWin32::Key *key = keyStore->getKey(i);
		assert(key != nullptr);
		if (!key)
			continue;

		int tmp_width[2];
		tmp_width[0] = ListView_GetStringWidth(hListView, U82T_s(key->name)) + column_padding[0];
		//tmp_width[1] = ListView_GetStringWidth(hListView, U82T_s(key->value)) + column_padding[1];

		column_width[0] = std::max(column_width[0], tmp_width[0]);
		//column_width[1] = std::max(column_width[1], tmp_width[1]);

		ret = LibWin32Common::measureTextSize(hListView, hFontMono, U82T_s(key->value), &szValue);
		assert(ret == 0);
		if (ret == 0) {
			column_width[1] = std::max(column_width[1], (int)szValue.cx + column_padding[1]);
		}
	}
	ListView_SetColumnWidth(hListView, 0, column_width[0]);
	ListView_SetColumnWidth(hListView, 1, column_width[1]);

	// Auto-size the "Valid?" column.
	ListView_SetColumnWidth(hListView, 2, LVSCW_AUTOSIZE_USEHEADER);

	// Subclass the ListView.
	// TODO: Error handling?
	SetWindowSubclass(hListView, ListViewSubclassProc,
		IDC_KEYMANAGER_LIST, reinterpret_cast<DWORD_PTR>(this));

	// Create the EDIT box.
	hEditBox = CreateWindowEx(WS_EX_LEFT,
		WC_EDIT, nullptr,
		WS_CHILDWINDOW | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_UPPERCASE | ES_WANTRETURN,
		0, 0, 0, 0,
		hListView, (HMENU)IDC_KEYMANAGER_EDIT, nullptr, nullptr);
	fontHandler.addMonoControl(hEditBox);
	SetWindowSubclass(hEditBox, ListViewEditSubclassProc,
		IDC_KEYMANAGER_EDIT, reinterpret_cast<DWORD_PTR>(this));

	// Set the KeyStore's window.
	keyStore->setHWnd(hWndPropSheet);

	// Register for WTS session notifications. (Remote Desktop)
	wts.registerSessionNotification(hWndPropSheet, NOTIFY_FOR_THIS_SESSION);
}

/**
 * Reset the configuration.
 */
void KeyManagerTabPrivate::reset(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	// Reset the keys.
	keyStore->reset();
}

/**
 * Save the configuration.
 */
void KeyManagerTabPrivate::save(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	if (!keyStore->hasChanged())
		return;

	// NOTE: This may re-check the configuration timestamp.
	const KeyManager *const keyManager = KeyManager::instance();
	const char *const filename = keyManager->filename();
	assert(filename != nullptr);
	if (!filename) {
		// No configuration filename...
		return;
	}

	// Make sure the configuration directory exists.
	// NOTE: The filename portion MUST be kept in config_path,
	// since the last component is ignored by rmkdir().
	int ret = FileSystem::rmkdir(filename);
	if (ret != 0) {
		// rmkdir() failed.
		return;
	}

	// Save the keys.
	const tstring tfilename = U82T_c(filename);
	const int totalKeyCount = keyStore->totalKeyCount();
	for (int i = 0; i < totalKeyCount; i++) {
		const KeyStoreWin32::Key *const pKey = keyStore->getKey(i);
		assert(pKey != nullptr);
		if (!pKey || !pKey->modified)
			continue;

		// Save this key.
		WritePrivateProfileString(_T("Keys"), U82T_s(pKey->name), U82T_s(pKey->value), tfilename.c_str());
	}

	// Clear the modified status.
	keyStore->allKeysSaved();
}

/**
 * Dialog procedure.
 * @param hDlg
 * @param uMsg
 * @param wParam
 * @param lParam
 */
INT_PTR CALLBACK KeyManagerTabPrivate::dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_INITDIALOG: {
			// Get the pointer to the property sheet page object. This is 
			// contained in the LPARAM of the PROPSHEETPAGE structure.
			LPPROPSHEETPAGE pPage = reinterpret_cast<LPPROPSHEETPAGE>(lParam);
			if (!pPage)
				return true;

			// Get the pointer to the KeyManagerTabPrivate object.
			KeyManagerTabPrivate *const d = reinterpret_cast<KeyManagerTabPrivate*>(pPage->lParam);
			if (!d)
				return true;

			assert(d->hWndPropSheet == nullptr);
			d->hWndPropSheet = hDlg;

			// Store the D object pointer with this particular page dialog.
			SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

			// Initialize the UI.
			d->initUI();

			// Reset the configuration.
			d->reset();
			return true;
		}

		case WM_NOTIFY: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}

			NMHDR *const pHdr = reinterpret_cast<NMHDR*>(lParam);
			switch (pHdr->code) {
				case PSN_APPLY:
					// Save settings.
					d->save();
					break;

				case LVN_GETDISPINFO: {
					// Get data for an LVS_OWNERDRAW ListView.
					if (!d->keyStore || pHdr->idFrom != IDC_KEYMANAGER_LIST)
						break;

					return d->ListView_GetDispInfo(reinterpret_cast<NMLVDISPINFO*>(lParam));
				}

				case NM_CUSTOMDRAW: {
					// Custom drawing notification.
					if (pHdr->idFrom != IDC_KEYMANAGER_LIST)
						break;

					// NOTE: Since this is a DlgProc, we can't simply return
					// the CDRF code. It has to be set as DWLP_MSGRESULT.
					// References:
					// - https://stackoverflow.com/questions/40549962/c-winapi-listview-nm-customdraw-not-getting-cdds-itemprepaint
					// - https://stackoverflow.com/a/40552426
					const int result = d->ListView_CustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(pHdr));
					SetWindowLongPtr(hDlg, DWLP_MSGRESULT, result);
					return true;
				}

				case PSN_SETACTIVE:
					// Disable the "Defaults" button.
					RpPropSheet_EnableDefaults(GetParent(hDlg), false);
					break;

				default:
					break;
			}
			break;
		}

		case WM_COMMAND: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}

			switch (LOWORD(wParam)) {
				case IDC_KEYMANAGER_IMPORT: {
					// Show the "Import" popup menu.
					if (!d->hMenuImport) {
						d->hMenuImport = ITab::LoadMenu_i18n(IDR_KEYMANAGER_IMPORT);
					}

					if (!d->hMenuImport) {
						// Unable to create the "Import" popup menu.
						return true;
					}

					HMENU hSubMenu = GetSubMenu(d->hMenuImport, 0);
					if (hSubMenu) {
						RECT btnRect;
						GetWindowRect(GetDlgItem(hDlg, IDC_KEYMANAGER_IMPORT), &btnRect);
						TrackPopupMenu(hSubMenu, TPM_LEFTALIGN|TPM_TOPALIGN,
							btnRect.left, btnRect.bottom, 0, hDlg, nullptr);
					}
					return true;
				}

				case IDM_KEYMANAGER_IMPORT_WII_KEYS_BIN:
					d->importWiiKeysBin();
					return true;
				case IDM_KEYMANAGER_IMPORT_WIIU_OTP_BIN:
					d->importWiiUOtpBin();
					return true;
				case IDM_KEYMANAGER_IMPORT_3DS_BOOT9_BIN:
					d->import3DSboot9bin();
					break;
				case IDM_KEYMANAGER_IMPORT_3DS_AESKEYDB:
					d->import3DSaeskeydb();
					break;

				default:
					break;
			}
			break;
		}

		case WM_RP_PROP_SHEET_RESET: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}

			// Reset the tab.
			d->reset();
			break;
		}

		case WM_SYSCOLORCHANGE:
		case WM_THEMECHANGED: {
			// Reinitialize the alternate row color.
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (d) {
				// Reinitialize the alternate row color.
				d->colorAltRow = LibWin32Common::getAltRowColor();
				if (d->hbrAltRow) {
					DeleteBrush(d->hbrAltRow);
				}
				d->hbrAltRow = CreateSolidBrush(d->colorAltRow);

				// Update the fonts.
				d->fontHandler.updateFonts(true);
			}
			break;
		}

		case WM_NCPAINT: {
			// Update the monospaced font.
			// NOTE: This should be WM_SETTINGCHANGE with
			// SPI_GETFONTSMOOTHING or SPI_GETFONTSMOOTHINGTYPE,
			// but that message isn't sent when previewing changes
			// for ClearType. (It's sent when applying the changes.)
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (d) {
				// Update the fonts.
				d->fontHandler.updateFonts();
			}
			break;
		}

		case WM_KEYSTORE_KEYCHANGED_IDX: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}

			// Update the row.
			HWND hListView = GetDlgItem(d->hWndPropSheet, IDC_KEYMANAGER_LIST);
			assert(hListView != nullptr);
			if (hListView) {
				ListView_RedrawItems(hListView, (int)lParam, (int)lParam);
			}
			return true;
		}

		case WM_KEYSTORE_ALLKEYSCHANGED: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}

			// Update all rows.
			HWND hListView = GetDlgItem(d->hWndPropSheet, IDC_KEYMANAGER_LIST);
			assert(hListView != nullptr);
			if (hListView) {
				ListView_RedrawItems(hListView, 0, d->keyStore->totalKeyCount()-1);
			}
			return true;
		}

		case WM_KEYSTORE_MODIFIED: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}

			// Key was modified.
			PropSheet_Changed(GetParent(hDlg), hDlg);
			return true;
		}

		case WM_WTSSESSION_CHANGE: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}
			HWND hListView = GetDlgItem(d->hWndPropSheet, IDC_KEYMANAGER_LIST);
			assert(hListView != nullptr);
			if (!hListView)
				break;
			DWORD dwExStyle = ListView_GetExtendedListViewStyle(hListView);

			// If RDP was connected, disable ListView double-buffering.
			// If console (or RemoteFX) was connected, enable ListView double-buffering.
			switch (wParam) {
				case WTS_CONSOLE_CONNECT:
					dwExStyle |= LVS_EX_DOUBLEBUFFER;
					ListView_SetExtendedListViewStyle(hListView, dwExStyle);
					break;
				case WTS_REMOTE_CONNECT:
					dwExStyle &= ~LVS_EX_DOUBLEBUFFER;
					ListView_SetExtendedListViewStyle(hListView, dwExStyle);
					break;
				default:
					break;
			}
			break;
		}

		case WM_DPICHANGED: {
			auto *const d = reinterpret_cast<KeyManagerTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No KeyManagerTabPrivate. Can't do anything...
				return false;
			}

			// TODO: Verify that this works. (Might be top-level only?)
			d->loadImages();
			break;
		}

		default:
			break;
	}

	return false; // Let system deal with other messages
}

/**
 * Property sheet callback procedure.
 * @param hDlg
 * @param uMsg
 * @param wParam
 * @param lParam
 */
UINT CALLBACK KeyManagerTabPrivate::callbackProc(HWND hWnd, UINT uMsg, LPPROPSHEETPAGE ppsp)
{
	switch (uMsg) {
		case PSPCB_CREATE: {
			// Must return true to enable the page to be created.
			return true;
		}

		case PSPCB_RELEASE: {
			// TODO: Do something here?
			break;
		}

		default:
			break;
	}

	return false;
}

/**
 * ListView subclass procedure.
 * @param hWnd		Control handle.
 * @param uMsg		Message.
 * @param wParam	WPARAM
 * @param lParam	LPARAM
 * @param uIdSubclass	Subclass ID. (usually the control ID)
 * @param dwRefData	KeyManagerTabPrivate*
 * @return
 */
LRESULT CALLBACK KeyManagerTabPrivate::ListViewSubclassProc(
	HWND hWnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (!dwRefData) {
		// No RP_ShellPropSheetExt. Can't do anything...
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg) {
		case WM_LBUTTONDBLCLK: {
			// Reference: http://www.cplusplus.com/forum/windows/107679/
			KeyManagerTabPrivate *const d =
				reinterpret_cast<KeyManagerTabPrivate*>(dwRefData);
			assert(d->hWndPropSheet != nullptr);
			if (!d->hWndPropSheet)
				return false;

			// Check for a double-click in the ListView.
			// ListView only directly supports editing of the
			// first column, so we have to handle it manually
			// for the second column (Value).
			LVHITTESTINFO lvhti;
			lvhti.pt.x = GET_X_LPARAM(lParam);
			lvhti.pt.y = GET_Y_LPARAM(lParam);

			// Check if this point maps to a valid "Value" subitem.
			int iItem = ListView_SubItemHitTest(hWnd, &lvhti);
			if (iItem < 0 || lvhti.iSubItem != 1) {
				// Not a "Value" subitem.
				break;
			}

			// Get the key.
			const KeyStoreWin32::Key *key = d->keyStore->getKey(iItem);
			assert(key != nullptr);
			if (!key)
				break;

			// Make the edit box visible at the subitem's location.
			// TODO: Subclass the edit box.
			//HWND hEditBox = GetDlgItem(d->hWndPropSheet, IDC_KEYMANAGER_EDIT);
			assert(d->hEditBox != nullptr);
			if (!d->hEditBox)
				break;

			// Copy the text from the ListView to the EDIT control.
			TCHAR szItemText[128];
			ListView_GetItemText(hWnd, iItem, lvhti.iSubItem, szItemText, _countof(szItemText));
			SetWindowText(d->hEditBox, szItemText);
			// FIXME: ES_AUTOHSCROLL causes some initial scrolling weirdness here,
			// but disabling it prevents entering more text than fits onscreen...
			Edit_SetSel(d->hEditBox, 0, -1);	// Select All

			d->iEditItem = iItem;
			d->bCancelEdit = false;
			d->bAllowKanji = key->allowKanji;

			// Set the EDIT control's position.
			RECT rectSubItem;
			ListView_GetSubItemRect(hWnd, iItem, lvhti.iSubItem, LVIR_BOUNDS, &rectSubItem);
			SetWindowPos(d->hEditBox, HWND_TOPMOST, rectSubItem.left, rectSubItem.top,
				rectSubItem.right - rectSubItem.left,
				rectSubItem.bottom - rectSubItem.top,
				SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);
			SetFocus(d->hEditBox);
			return true;
		}

		case WM_NCDESTROY:
			// Remove the window subclass.
			// Reference: https://devblogs.microsoft.com/oldnewthing/20031111-00/?p=41883
			RemoveWindowSubclass(hWnd, ListViewSubclassProc, uIdSubclass);
			break;

		default:
			break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * ListView EDIT control subclass procedure.
 * @param hWnd		Control handle.
 * @param uMsg		Message.
 * @param wParam	WPARAM
 * @param lParam	LPARAM
 * @param uIdSubclass	Subclass ID. (usually the control ID)
 * @param dwRefData	KeyManagerTabPrivate*
 * @return
 */
LRESULT CALLBACK KeyManagerTabPrivate::ListViewEditSubclassProc(
	HWND hWnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (!dwRefData) {
		// No RP_ShellPropSheetExt. Can't do anything...
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	KeyManagerTabPrivate *const d =
		reinterpret_cast<KeyManagerTabPrivate*>(dwRefData);
	assert(d->hWndPropSheet != nullptr);
	if (!d->hWndPropSheet)
		return false;

	switch (uMsg) {
		case WM_KILLFOCUS: {
			ShowWindow(hWnd, SW_HIDE);
			if (d->bCancelEdit)
				break;

			// NOTE: ListView_SetItem() doesn't work with LVS_OWNERDATA.
			// We'll have to edit the KeyStore directly.
			if (!d->keyStore)
				break;
			else if (d->iEditItem < 0 || d->iEditItem >= d->keyStore->totalKeyCount())
				break;

			// Save the key.
			TCHAR tbuf[128];
			tbuf[0] = 0;
			GetWindowText(hWnd, tbuf, _countof(tbuf));
			d->keyStore->setKey(d->iEditItem, T2U8(tbuf));

			// Item is no longer being edited.
			d->iEditItem = -1;
			break;
		}

		case WM_GETDLGCODE:
			return (DLGC_WANTALLKEYS | DefSubclassProc(hWnd, uMsg, wParam, lParam));

		case WM_CHAR: {
			// Reference: https://support.microsoft.com/en-us/help/102589/how-to-use-the-enter-key-from-edit-controls-in-a-dialog-box
			switch (wParam) {
				case VK_RETURN:
					// Finished editing.
					d->bCancelEdit = false;
					ShowWindow(hWnd, SW_HIDE);
					return true;
				case VK_ESCAPE:
					// Cancel editing.
					d->bCancelEdit = true;
					ShowWindow(hWnd, SW_HIDE);
					return true;
				default:
					break;
			}

			// Filter out invalid characters.

			// Always allow control characters and hexadecimal digits.
			if (iswcntrl((wint_t)wParam) || iswxdigit((wint_t)wParam)) {
				// This is a valid control character or hexadecimal digit.
				break;
			}

			// Check for kanji.
			if (d->bAllowKanji) {
				// Reference: http://www.localizingjapan.com/blog/2012/01/20/regular-expressions-for-japanese-text/
				if ((wParam >= 0x3400 && wParam <= 0x4DB5) ||
				    (wParam >= 0x4E00 && wParam <= 0x9FCB) ||
				    (wParam >= 0xF900 && wParam <= 0xFA6A))
				{
					// Valid kanji character.
					break;
				}
			}

			// Character is not allowed.
			return true;
		}

		case WM_KEYDOWN:
		case WM_KEYUP: {
			// Reference: https://support.microsoft.com/en-us/help/102589/how-to-use-the-enter-key-from-edit-controls-in-a-dialog-box
			switch (wParam) {
				case VK_RETURN:
					// Finished editing.
					d->bCancelEdit = false;
					ShowWindow(hWnd, SW_HIDE);
					return true;
				case VK_ESCAPE:
					// Cancel editing.
					d->bCancelEdit = true;
					ShowWindow(hWnd, SW_HIDE);
					return true;
				default:
					break;
			}
			break;
		}

		case WM_PASTE: {
			// Filter out text pasted in from the clipboard.
			// Reference: https://stackoverflow.com/questions/22263612/properly-handle-wm-paste-in-subclass-procedure
			if (!OpenClipboard(hWnd))
				return true;

			HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
			if (!hClipboardData) {
				CloseClipboard();
				return true;
			}

			const TCHAR *const pchData = static_cast<const TCHAR*>(GlobalLock(hClipboardData));
			if (!pchData) {
				// No data.
				CloseClipboard();
				return true;
			} else if (pchData[0] == 0) {
				// Empty string.
				// TODO: Paste anyway?
				GlobalUnlock(hClipboardData);
				CloseClipboard();
				return true;
			}

			// Filter out invalid characters.
			tstring tstr;
			tstr.reserve(_tcslen(pchData));
			for (const TCHAR *p = pchData; *p != 0; p++) {
				// Allow hexadecimal digits.
				if (_istxdigit(*p)) {
					tstr += *p;
				} else if (d->bAllowKanji) {
#ifdef UNICODE
					// Allow kanji.
					// Reference: http://www.localizingjapan.com/blog/2012/01/20/regular-expressions-for-japanese-text/
					if ((*p >= 0x3400 && *p <= 0x4DB5) ||
					    (*p >= 0x4E00 && *p <= 0x9FCB) ||
					    (*p >= 0xF900 && *p <= 0xFA6A))
					{
						tstr += *p;
					} else {
						// Invalid character.
						// Prevent the paste.
						GlobalUnlock(hClipboardData);
						CloseClipboard();
						return true;
					}
#else /* !UNICODE */
					// TODO: Shift-JIS support?
					// For now, assuming this is invalid.
					GlobalUnlock(hClipboardData);
					CloseClipboard();
					return true;
#endif /* UNICODE */
				} else {
					// Invalid character.
					// Prevent the paste.
					GlobalUnlock(hClipboardData);
					CloseClipboard();
					return true;
				}
			}

			GlobalUnlock(hClipboardData);
			CloseClipboard();

			if (!tstr.empty()) {
				// Insert the text.
				// TODO: Paste even if empty?
				Edit_ReplaceSel(hWnd, tstr.c_str());
			}
			return true;
		}

		case WM_NCDESTROY:
			// Remove the window subclass.
			// Reference: https://devblogs.microsoft.com/oldnewthing/20031111-00/?p=41883
			RemoveWindowSubclass(hWnd, ListViewSubclassProc, uIdSubclass);
			break;

		default:
			break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * ListView GetDispInfo function.
 * @param plvdi	[in/out] NMLVDISPINFO
 * @return True if handled; false if not.
 */
inline BOOL KeyManagerTabPrivate::ListView_GetDispInfo(NMLVDISPINFO *plvdi)
{
	LVITEM *const plvItem = &plvdi->item;
	if (plvItem->iItem < 0 || plvItem->iItem >= keyStore->totalKeyCount()) {
		// Index is out of range.
		return false;
	}

	const KeyStoreWin32::Key *const key = keyStore->getKey(plvItem->iItem);
	if (!key) {
		// No key...
		return false;
	}

	if (plvItem->mask & LVIF_TEXT) {
		// Fill in text.
		switch (plvItem->iSubItem) {
			case 0:
				// Key name.
				_tcscpy_s(plvItem->pszText, plvItem->cchTextMax, U82T_s(key->name));
				return true;
			case 1:
				// Value.
				_tcscpy_s(plvItem->pszText, plvItem->cchTextMax, U82T_s(key->value));
				return true;
			default:
				// No text for "Valid?".
				plvItem->pszText[0] = 0;
				return true;
		}
	}

	// Nothing to do here...
	return false;
}

/**
 * ListView CustomDraw function.
 * @param plvcd	[in/out] NMLVCUSTOMDRAW
 * @return Return value.
 */
inline int KeyManagerTabPrivate::ListView_CustomDraw(NMLVCUSTOMDRAW *plvcd)
{
	// Check if this is an "odd" row.
	bool isOdd;
	if (isComCtl32_v610) {
		// COMCTL32.dll v6.10: We're using groups, so
		// check the key index within the section.
		int sectIdx = -1, keyIdx = -1;
		int ret = keyStore->idxToSectKey((int)plvcd->nmcd.dwItemSpec, &sectIdx, &keyIdx);
		if (ret == 0) {
			isOdd = !!(keyIdx % 2);
		} else {
			// Unable to get sect/key.
			// Fall back to the flat index.
			isOdd = !!(plvcd->nmcd.dwItemSpec % 2);
		}
	} else {
		// COMCTL32.dll v6.00 or earlier: No groups.
		// Use the flat key index.
		isOdd = !!(plvcd->nmcd.dwItemSpec % 2);
	}

	// Make sure the "Value" column is drawn with a monospaced font.
	// Reference: https://www.codeproject.com/Articles/2890/Using-ListView-control-under-Win-API
	int result = CDRF_DODEFAULT;
	switch (plvcd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			// Request notifications for individual ListView items.
			result = CDRF_NOTIFYITEMDRAW;
			break;

		case CDDS_ITEMPREPAINT: {
			// Set the background color for alternating row colors.
			if (isOdd) {
				// NOTE: plvcd->clrTextBk is set to 0xFF000000 here,
				// not the actual default background color.
				// FIXME: On Windows 7:
				// - Standard row colors are 19px high.
				// - Alternate row colors are 17px high. (top and bottom lines ignored?)
				plvcd->clrTextBk = colorAltRow;
				result = CDRF_NOTIFYSUBITEMDRAW | CDRF_NEWFONT;
			} else {
				result = CDRF_NOTIFYSUBITEMDRAW;
			}
			break;
		}

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
			switch (plvcd->iSubItem) {
				case 1: {
					// "Value" column.
					// Use the monospaced font.
					HFONT hFontMono = fontHandler.monospacedFont();
					if (hFontMono) {
						SelectObject(plvcd->nmcd.hdc, hFontMono);
						result = CDRF_NEWFONT;
					}
					break;
				}

				case 2: {
					// "Valid?" column.
					// Draw the icon manually.
					const KeyStoreWin32::Key *const key = keyStore->getKey((int)plvcd->nmcd.dwItemSpec);
					assert(key != nullptr);
					if (!key)
						break;

					HICON hDrawIcon = nullptr;
					switch (key->status) {
						case KeyStoreWin32::Key::Status::Unknown:
							// Unknown...
							hDrawIcon = hIconUnknown;
							break;
						case KeyStoreWin32::Key::Status::NotAKey:
							// The key data is not in the correct format.
							hDrawIcon = hIconInvalid;
							break;
						case KeyStoreWin32::Key::Status::Empty:
							// Empty key.
							break;
						case KeyStoreWin32::Key::Status::Incorrect:
							// Key is incorrect.
							hDrawIcon = hIconInvalid;
							break;
						case KeyStoreWin32::Key::Status::OK:
							// Key is correct.
							hDrawIcon = hIconGood;
							break;
					}

					if (!hDrawIcon)
						break;

					const RECT *pRcSubItem = &plvcd->nmcd.rc;
					RECT rectTmp;
					if (pRcSubItem->right == 0 || pRcSubItem->bottom == 0) {
						// Windows XP: plvcd->nmcd.rc isn't initialized.
						// Get the subitem RECT manually.
						// TODO: Increase row height, or decrease icon size?
						// The icon is slightly too big for the default row
						// height on XP.
						BOOL bRet = ListView_GetSubItemRect(plvcd->nmcd.hdr.hwndFrom,
							(int)plvcd->nmcd.dwItemSpec, plvcd->iSubItem, LVIR_BOUNDS, &rectTmp);
						if (!bRet)
							break;
						pRcSubItem = &rectTmp;
					}

					// Custom drawing this subitem.
					result = CDRF_SKIPDEFAULT;

					// Set the row background color.
					// TODO: "Disabled" state?
					// NOTE: plvcd->clrTextBk is set to 0xFF000000 here,
					// not the actual default background color.
					HBRUSH hbr;
					if (plvcd->nmcd.uItemState & CDIS_SELECTED) {
						// Row is selected.
						hbr = (HBRUSH)(COLOR_HIGHLIGHT+1);
					} else if (isOdd) {
						// FIXME: On Windows 7:
						// - Standard row colors are 19px high.
						// - Alternate row colors are 17px high. (top and bottom lines ignored?)
						hbr = hbrAltRow;
					} else {
						// Standard row color. Draw it anyway in case
						// the theme was changed, since ListView only
						// partially recognizes theme changes.
						hbr = (HBRUSH)(COLOR_WINDOW+1);
					}
					FillRect(plvcd->nmcd.hdc, pRcSubItem, hbr);

					const int x = pRcSubItem->left + (((pRcSubItem->right - pRcSubItem->left) - iconSize) / 2);
					const int y = pRcSubItem->top + (((pRcSubItem->bottom - pRcSubItem->top) - iconSize) / 2);

					DrawIconEx(plvcd->nmcd.hdc, x, y, hDrawIcon,
						iconSize, iconSize, 0, nullptr, DI_NORMAL);
					break;
				}

				default:
					break;
			}
			break;

		default:
			break;
	}

	return result;
}

/**
 * Load images.
 */
void KeyManagerTabPrivate::loadImages(void)
{
	// Get the current DPI.
	const UINT dpi = rp_GetDpiForWindow(hWndPropSheet);
	unsigned int iconSize_new;
	if (dpi < 120) {
		// [96,120) dpi: Use 16x16.
		iconSize_new = 16;
	} else if (dpi <= 144) {
		// [120,144] dpi: Use 24x24.
		// TODO: Maybe needs to be slightly higher?
		iconSize_new = 24;
	} else {
		// >144dpi: Use 32x32.
		iconSize_new = 32;
	}

	if (iconSize == iconSize_new) {
		// Icons are already loaded.
		return;
	}

	// Save the new icon size.
	iconSize = iconSize_new;

	// Free the icons if they were already loaded.
	if (hIconUnknown) {
		DestroyIcon(hIconUnknown);
		hIconUnknown = nullptr;
	}
	if (hIconInvalid) {
		DestroyIcon(hIconInvalid);
		hIconInvalid = nullptr;
	}
	if (hIconGood) {
		DestroyIcon(hIconGood);
		hIconGood = nullptr;
	}

	// Load the icons.
	// NOTE: Using IDI_* will only return the 32x32 icon.
	// Need to get the icon from USER32 directly.
	HMODULE hUser32 = GetModuleHandle(_T("user32"));
	assert(hUser32 != nullptr);
	if (hUser32) {
		hIconUnknown = (HICON)LoadImage(hUser32,
			MAKEINTRESOURCE(102), IMAGE_ICON,
			iconSize_new, iconSize_new, 0);
		hIconInvalid = (HICON)LoadImage(hUser32,
			MAKEINTRESOURCE(103), IMAGE_ICON,
			iconSize_new, iconSize_new, 0);
	}

	// Load hIconGood from our own resource section.
	// Based on KDE Oxygen 5.35.0's base/16x16/actions/dialog-ok-apply.png
	hIconGood = (HICON)LoadImage(HINST_THISCOMPONENT,
		MAKEINTRESOURCE(IDI_KEY_VALID), IMAGE_ICON,
		iconSize_new, iconSize_new, 0);
}

/** "Import" menu actions. **/

/**
 * Import keys from Wii keys.bin. (BootMii format)
 */
void KeyManagerTabPrivate::importWiiKeysBin(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	const tstring tfilename = LibWin32Common::getOpenFileName(hWndPropSheet,
		// tr: Wii keys.bin dialog title.
		U82T_c(C_("KeyManagerTab", "Select Wii keys.bin File")),
		// tr: Wii keys.bin file filter. (RP format)
		C_("KeyManagerTab", "keys.bin|keys.bin|-|Binary Files|*.bin|application/octet-stream|All Files|*.*|-"),
		ts_keyFileDir.c_str());
	if (tfilename.empty())
		return;

	// Update ts_keyFileDir using the returned filename.
	updateKeyFileDir(tfilename);

	KeyStoreWin32::ImportReturn iret = keyStore->importWiiKeysBin(T2U8(tfilename).c_str());
	// TODO: Port showKeyImportReturnStatus from the KDE version.
	//d->showKeyImportReturnStatus(filename, U82T_c(C_("KeyManagerTab", "Wii keys.bin")), iret);
}

/**
 * Import keys from Wii U otp.bin.
 */
void KeyManagerTabPrivate::importWiiUOtpBin(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	const tstring tfilename = LibWin32Common::getOpenFileName(hWndPropSheet,
		// tr: Wii U otp.bin dialog title.
		U82T_c(C_("KeyManagerTab", "Select Wii U otp.bin File")),
		// tr: Wii U otp.bin file filter. (RP format)
		C_("KeyManagerTab", "otp.bin|otp.bin|-|Binary Files|*.bin|application/octet-stream|All Files|*.*|-"),
		ts_keyFileDir.c_str());
	if (tfilename.empty())
		return;

	// Update ts_keyFileDir using the returned filename.
	updateKeyFileDir(tfilename);

	KeyStoreWin32::ImportReturn iret = keyStore->importWiiUOtpBin(T2U8(tfilename).c_str());
	// TODO: Port showKeyImportReturnStatus from the KDE version.
	//d->showKeyImportReturnStatus(filename, U82T_c(C_("KeyManagerTab", "Wii U otp.bin"), iret);
}

/**
 * Import keys from 3DS boot9.bin.
 */
void KeyManagerTabPrivate::import3DSboot9bin(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	const tstring tfilename = LibWin32Common::getOpenFileName(hWndPropSheet,
		// tr: 3DS boot9.bin dialog title.
		U82T_c(C_("KeyManagerTab", "Select 3DS boot9.bin File")),
		// tr: 3DS boot9.bin file filter. (RP format)
		C_("KeyManagerTab", "boot9.bin|boot9.bin|-|Binary Files|*.bin|application/octet-stream|All Files|*.*|-"),
		ts_keyFileDir.c_str());
	if (tfilename.empty())
		return;

	// Update ts_keyFileDir using the returned filename.
	updateKeyFileDir(tfilename);

	KeyStoreWin32::ImportReturn iret = keyStore->import3DSboot9bin(T2U8(tfilename).c_str());
	// TODO: Port showKeyImportReturnStatus from the KDE version.
	//d->showKeyImportReturnStatus(filename, U82T_c(C_("KeyManagerTab", "3DS boot9.bin"), iret);
}

/**
 * Import keys from 3DS aeskeydb.bin.
 */
void KeyManagerTabPrivate::import3DSaeskeydb(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	const tstring tfilename = LibWin32Common::getOpenFileName(hWndPropSheet,
		// tr: aeskeydb.bin dialog title.
		U82T_c(C_("KeyManagerTab", "Select 3DS aeskeydb.bin File")),
		// tr: aeskeydb.bin file filter. (RP format)
		C_("KeyManagerTab", "aeskeydb.bin|aeskeydb.bin|-|Binary Files|*.bin|application/octet-stream|All Files|*.*|-"),
		ts_keyFileDir.c_str());
	if (tfilename.empty())
		return;

	// Update ts_keyFileDir using the returned filename.
	updateKeyFileDir(tfilename);

	KeyStoreWin32::ImportReturn iret = keyStore->import3DSaeskeydb(T2U8(tfilename).c_str());
	// TODO: Port showKeyImportReturnStatus from the KDE version.
	//d->showKeyImportReturnStatus(filename, U82T_c(C_("KeyManagerTab", "3DS aeskeydb.bin"), iret);
}

/** KeyManagerTab **/

KeyManagerTab::KeyManagerTab(void)
	: d_ptr(new KeyManagerTabPrivate())
{ }

KeyManagerTab::~KeyManagerTab()
{
	delete d_ptr;
}

/**
 * Create the HPROPSHEETPAGE for this tab.
 *
 * NOTE: This function can only be called once.
 * Subsequent invocations will return nullptr.
 *
 * @return HPROPSHEETPAGE.
 */
HPROPSHEETPAGE KeyManagerTab::getHPropSheetPage(void)
{
	RP_D(KeyManagerTab);
	assert(d->hPropSheetPage == nullptr);
	if (d->hPropSheetPage) {
		// Property sheet has already been created.
		return nullptr;
	}

	// tr: Tab title.
	const tstring tsTabTitle = U82T_c(C_("KeyManagerTab", "Key Manager"));

	PROPSHEETPAGE psp;
	psp.dwSize = sizeof(psp);
	psp.dwFlags = PSP_USECALLBACK | PSP_USETITLE | PSP_DLGINDIRECT;
	psp.hInstance = HINST_THISCOMPONENT;
	psp.pResource = LoadDialog_i18n(IDD_CONFIG_KEYMANAGER);
	psp.pszIcon = nullptr;
	psp.pszTitle = tsTabTitle.c_str();
	psp.pfnDlgProc = KeyManagerTabPrivate::dlgProc;
	psp.lParam = reinterpret_cast<LPARAM>(d);
	psp.pcRefParent = nullptr;
	psp.pfnCallback = KeyManagerTabPrivate::callbackProc;

	d->hPropSheetPage = CreatePropertySheetPage(&psp);
	return d->hPropSheetPage;
}

/**
 * Reset the contents of this tab.
 */
void KeyManagerTab::reset(void)
{
	RP_D(KeyManagerTab);
	d->reset();
}

/**
 * Load the default configuration.
 * This does NOT save, and will only emit modified()
 * if it's different from the current configuration.
 */
void KeyManagerTab::loadDefaults(void)
{
	// Not implemented for this tab.
}

/**
 * Save the contents of this tab.
 */
void KeyManagerTab::save(void)
{
	RP_D(KeyManagerTab);
	d->save();
}
