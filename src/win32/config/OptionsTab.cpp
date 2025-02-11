/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * OptionsTab.cpp: Options tab for rp-config.                              *
 *                                                                         *
 * Copyright (c) 2016-2021 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "OptionsTab.hpp"
#include "res/resource.h"

// LanguageComboBox
#include "LanguageComboBox.hpp"

// librpbase, librpfile
#include "librpbase/SystemRegion.hpp"
using namespace LibRpBase;
using namespace LibRpFile;

// C++ STL classes.
using std::tstring;

class OptionsTabPrivate
{
	public:
		OptionsTabPrivate();

	private:
		RP_DISABLE_COPY(OptionsTabPrivate)

	protected:
		/**
		 * Convert a bool value to BST_CHCEKED or BST_UNCHECKED.
		 * @param value Bool value.
		 * @return BST_CHECKED or BST_UNCHECKED.
		 */
		static inline int boolToBstChecked(bool value) {
			return (value ? BST_CHECKED : BST_UNCHECKED);
		}

		/**
		 * Convert BST_CHECKED or BST_UNCHECKED to a bool string.
		 * @param value BST_CHECKED or BST_UNCHECKED.
		 * @return Bool string.
		 */
		static inline const TCHAR *bstCheckedToBoolString(unsigned int value) {
			return (value == BST_CHECKED ? _T("true") : _T("false"));
		}

		/**
		 * Convert BST_CHECKED or BST_UNCHECKED to a bool.
		 * @param value BST_CHECKED or BST_UNCHECKED.
		 * @return bool.
		 */
		static inline bool bstCheckedToBool(unsigned int value) {
			return (value == BST_CHECKED);
		}

	public:
		/**
		 * Reset the configuration.
		 */
		void reset(void);

		/**
		 * Load the default configuration.
		 * This does NOT save, and will only emit modified()
		 * if it's different from the current configuration.
		 */
		void loadDefaults(void);

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

	public:
		// Property sheet.
		HPROPSHEETPAGE hPropSheetPage;
		HWND hWndPropSheet;

		// Has the user changed anything?
		bool changed;

		// PAL language codes for GameTDB.
		static const uint32_t pal_lc[];
		static const int pal_lc_idx_def;
};

/** OptionsTabPrivate **/

// PAL language codes for GameTDB.
// NOTE: 'au' is technically not a language code, but
// GameTDB handles it as a separate language.
// TODO: Combine with the KDE version.
// NOTE: Win32 LanguageComboBox uses a NULL-terminated pal_lc[] array.
const uint32_t OptionsTabPrivate::pal_lc[] = {'au', 'de', 'en', 'es', 'fr', 'it', 'nl', 'pt', 'ru', 0};
const int OptionsTabPrivate::pal_lc_idx_def = 2;

OptionsTabPrivate::OptionsTabPrivate()
	: hPropSheetPage(nullptr)
	, hWndPropSheet(nullptr)
	, changed(false)
{ }

/**
 * Reset the configuration.
 */
void OptionsTabPrivate::reset(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	// NOTE: This may re-check the configuration timestamp.
	const Config *const config = Config::instance();

	// Downloads
	CheckDlgButton(hWndPropSheet, IDC_EXTIMGDL, boolToBstChecked(config->extImgDownloadEnabled()));
	CheckDlgButton(hWndPropSheet, IDC_INTICONSMALL, boolToBstChecked(config->useIntIconForSmallSizes()));
	CheckDlgButton(hWndPropSheet, IDC_HIGHRESDL, boolToBstChecked(config->downloadHighResScans()));
	CheckDlgButton(hWndPropSheet, IDC_STOREFILEORIGININFO, boolToBstChecked(config->storeFileOriginInfo()));

	// Options
	// FIXME: Uncomment this once the "dangerous" permissions overlay
	// is working on Windows.
	/*
	CheckDlgButton(hWndPropSheet, IDC_DANGEROUSPERMISSIONS,
		boolToBstChecked(config->showDangerousPermissionsOverlayIcon()));
	*/
	CheckDlgButton(hWndPropSheet, IDC_ENABLETHUMBNAILONNETWORKFS,
		boolToBstChecked(config->enableThumbnailOnNetworkFS()));

	// FIXME: Remove this once the "dangerous" permissions overlay
	// is working on Windows.
	CheckDlgButton(hWndPropSheet, IDC_DANGEROUSPERMISSIONS, BST_UNCHECKED);
	EnableWindow(GetDlgItem(hWndPropSheet, IDC_DANGEROUSPERMISSIONS), FALSE);

	// PAL language code.
	const uint32_t lc = config->palLanguageForGameTDB();
	int idx = 0;
	for (; idx < ARRAY_SIZE_I(pal_lc); idx++) {
		if (pal_lc[idx] == lc)
			break;
	}
	if (idx >= ARRAY_SIZE_I(pal_lc)) {
		// Out of range. Default to 'en'.
		idx = pal_lc_idx_def;
	}
	ComboBox_SetCurSel(GetDlgItem(hWndPropSheet, IDC_PALLANGUAGEFORGAMETDB), idx);

	// No longer changed.
	changed = false;
}

void OptionsTabPrivate::loadDefaults(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	// TODO: Get the defaults from Config.
	// For now, hard-coding everything here.
	// Downloads
	static const bool extImgDownloadEnabled_default = true;
	static const bool useIntIconForSmallSizes_default = true;
	static const bool downloadHighResScans_default = true;
	static const bool storeFileOriginInfo_default = true;
	static const int palLanguageForGameTDB_default =
		OptionsTabPrivate::pal_lc_idx_def;	// cboGameTDBPAL index ('en')
	// Options
	static const bool showDangerousPermissionsOverlayIcon_default = true;
	static const bool enableThumbnailOnNetworkFS_default = false;
	bool isDefChanged = false;

	// Downloads
	bool cur = bstCheckedToBool(IsDlgButtonChecked(hWndPropSheet, IDC_EXTIMGDL));
	if (cur != extImgDownloadEnabled_default) {
		CheckDlgButton(hWndPropSheet, IDC_EXTIMGDL, boolToBstChecked(extImgDownloadEnabled_default));
		isDefChanged = true;
	}
	cur = bstCheckedToBool(IsDlgButtonChecked(hWndPropSheet, IDC_INTICONSMALL));
	if (cur != useIntIconForSmallSizes_default) {
		CheckDlgButton(hWndPropSheet, IDC_INTICONSMALL, boolToBstChecked(useIntIconForSmallSizes_default));
		isDefChanged = true;
	}
	cur = bstCheckedToBool(IsDlgButtonChecked(hWndPropSheet, IDC_HIGHRESDL));
	if (cur != downloadHighResScans_default) {
		CheckDlgButton(hWndPropSheet, IDC_HIGHRESDL, boolToBstChecked(downloadHighResScans_default));
		isDefChanged = true;
	}
	cur = bstCheckedToBool(IsDlgButtonChecked(hWndPropSheet, IDC_STOREFILEORIGININFO));
	if (cur != storeFileOriginInfo_default) {
		CheckDlgButton(hWndPropSheet, IDC_STOREFILEORIGININFO,
			boolToBstChecked(storeFileOriginInfo_default));
		isDefChanged = true;
	}
	HWND cboGameTDBPAL = GetDlgItem(hWndPropSheet, IDC_PALLANGUAGEFORGAMETDB);
	int idx = ComboBox_GetCurSel(cboGameTDBPAL);
	if (idx != palLanguageForGameTDB_default) {
		ComboBox_SetCurSel(cboGameTDBPAL, idx);
		isDefChanged = true;
	}

	// Options
	// FIXME: Uncomment this once the "dangerous" permissions overlay
	// is working on Windows.
	/*
	cur = bstCheckedToBool(IsDlgButtonChecked(hWndPropSheet, IDC_DANGEROUSPERMISSIONS));
	if (cur != showDangerousPermissionsOverlayIcon_default) {
		CheckDlgButton(hWndPropSheet, IDC_DANGEROUSPERMISSIONS,
			boolToBstChecked(showDangerousPermissionsOverlayIcon_default));
		isDefChanged = true;
	}
	*/
	cur = bstCheckedToBool(IsDlgButtonChecked(hWndPropSheet, IDC_ENABLETHUMBNAILONNETWORKFS));
	if (cur != enableThumbnailOnNetworkFS_default) {
		CheckDlgButton(hWndPropSheet, IDC_ENABLETHUMBNAILONNETWORKFS,
			boolToBstChecked(enableThumbnailOnNetworkFS_default));
		isDefChanged = true;
	}

	if (isDefChanged) {
		this->changed = true;
		PropSheet_Changed(GetParent(hWndPropSheet), hWndPropSheet);
	}
}

/**
 * Save the configuration.
 */
void OptionsTabPrivate::save(void)
{
	assert(hWndPropSheet != nullptr);
	if (!hWndPropSheet)
		return;

	// NOTE: This may re-check the configuration timestamp.
	const Config *const config = Config::instance();
	const char *const filename = config->filename();
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

	const tstring tfilename = U82T_c(filename);

	// Downloads
	const TCHAR *btstr = bstCheckedToBoolString(IsDlgButtonChecked(hWndPropSheet, IDC_EXTIMGDL));
	WritePrivateProfileString(_T("Downloads"), _T("ExtImageDownload"), btstr, tfilename.c_str());

	btstr = bstCheckedToBoolString(IsDlgButtonChecked(hWndPropSheet, IDC_INTICONSMALL));
	WritePrivateProfileString(_T("Downloads"), _T("UseIntIconForSmallSizes"), btstr, tfilename.c_str());

	btstr = bstCheckedToBoolString(IsDlgButtonChecked(hWndPropSheet, IDC_HIGHRESDL));
	WritePrivateProfileString(_T("Downloads"), _T("DownloadHighResScans"), btstr, tfilename.c_str());

	btstr = bstCheckedToBoolString(IsDlgButtonChecked(hWndPropSheet, IDC_STOREFILEORIGININFO));
	WritePrivateProfileString(_T("Downloads"), _T("StoreFileOriginInfo"), btstr, tfilename.c_str());

	int idx = ComboBox_GetCurSel(GetDlgItem(hWndPropSheet, IDC_PALLANGUAGEFORGAMETDB));
	if (idx < 0 || idx >= ARRAY_SIZE_I(pal_lc)) {
		idx = pal_lc_idx_def;
	}
	WritePrivateProfileString(_T("Downloads"), _T("PalLanguageForGameTDB"),
		SystemRegion::lcToTString(pal_lc[idx]).c_str(), tfilename.c_str());

	// Options
	// FIXME: Uncomment this once the "dangerous" permissions overlay
	// is working on Windows.
	/*
	btstr = bstCheckedToBoolString(IsDlgButtonChecked(hWndPropSheet, IDC_DANGEROUSPERMISSIONS));
	WritePrivateProfileString(_T("Options"), _T("ShowDangerousPermissionsOverlayIcon"), btstr, tfilename.c_str());
	*/

	btstr = bstCheckedToBoolString(IsDlgButtonChecked(hWndPropSheet, IDC_ENABLETHUMBNAILONNETWORKFS));
	WritePrivateProfileString(_T("Options"), _T("EnableThumbnailOnNetworkFS"), btstr, tfilename.c_str());

	// No longer changed.
	changed = false;
}

/**
 * Dialog procedure.
 * @param hDlg
 * @param uMsg
 * @param wParam
 * @param lParam
 */
INT_PTR CALLBACK OptionsTabPrivate::dlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_INITDIALOG: {
			// Get the pointer to the property sheet page object. This is 
			// contained in the LPARAM of the PROPSHEETPAGE structure.
			LPPROPSHEETPAGE pPage = reinterpret_cast<LPPROPSHEETPAGE>(lParam);
			if (!pPage)
				return TRUE;

			// Get the pointer to the OptionsTabPrivate object.
			OptionsTabPrivate *const d = reinterpret_cast<OptionsTabPrivate*>(pPage->lParam);
			if (!d)
				return TRUE;

			assert(d->hWndPropSheet == nullptr);
			d->hWndPropSheet = hDlg;

			// Store the D object pointer with this particular page dialog.
			SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

			// Initialize the PAL language dropdown.
			// TODO: "Force PAL" option.
			HWND cboLanguage = GetDlgItem(hDlg, IDC_PALLANGUAGEFORGAMETDB);
			assert(cboLanguage != nullptr);
			if (cboLanguage) {
				LanguageComboBox_SetForcePAL(cboLanguage, true);
				LanguageComboBox_SetLCs(cboLanguage, pal_lc);
			}

			// Reset the configuration. 338
			d->reset();
			return TRUE;
		}

		case WM_NOTIFY: {
			auto *const d = reinterpret_cast<OptionsTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No OptionsTabPrivate. Can't do anything...
				return FALSE;
			}

			NMHDR *pHdr = reinterpret_cast<NMHDR*>(lParam);
			switch (pHdr->code) {
				case PSN_APPLY:
					// Save settings.
					if (d->changed) {
						d->save();
					}
					break;

				case PSN_SETACTIVE:
					// Enable the "Defaults" button.
					RpPropSheet_EnableDefaults(GetParent(hDlg), true);
					break;

				default:
					break;
			}
			break;
		}

		case WM_COMMAND: {
			auto *const d = reinterpret_cast<OptionsTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No OptionsTabPrivate. Can't do anything...
				return FALSE;
			}

			if (HIWORD(wParam) != BN_CLICKED &&
			    HIWORD(wParam) != CBN_SELCHANGE)
			{
				// Unexpected notification.
				break;
			}

			// A checkbox has been adjusted, or a dropdown
			// box has had its selection changed.
			// Page has been modified.
			PropSheet_Changed(GetParent(hDlg), hDlg);
			d->changed = true;
			break;
		}

		case WM_RP_PROP_SHEET_RESET: {
			auto *const d = reinterpret_cast<OptionsTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No OptionsTabPrivate. Can't do anything...
				return FALSE;
			}

			// Reset the tab.
			d->reset();
			break;
		}

		case WM_RP_PROP_SHEET_DEFAULTS: {
			auto *const d = reinterpret_cast<OptionsTabPrivate*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
			if (!d) {
				// No OptionsTabPrivate. Can't do anything...
				return FALSE;
			}

			// Load the defaults.
			d->loadDefaults();
			break;
		}

		default:
			break;
	}

	return FALSE; // Let system deal with other messages
}

/**
 * Property sheet callback procedure.
 * @param hDlg
 * @param uMsg
 * @param wParam
 * @param lParam
 */
UINT CALLBACK OptionsTabPrivate::callbackProc(HWND hWnd, UINT uMsg, LPPROPSHEETPAGE ppsp)
{
	switch (uMsg) {
		case PSPCB_CREATE: {
			// Must return TRUE to enable the page to be created.
			return TRUE;
		}

		case PSPCB_RELEASE: {
			// TODO: Do something here?
			break;
		}

		default:
			break;
	}

	return FALSE;
}

/** OptionsTab **/

OptionsTab::OptionsTab(void)
	: d_ptr(new OptionsTabPrivate())
{ }

OptionsTab::~OptionsTab()
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
HPROPSHEETPAGE OptionsTab::getHPropSheetPage(void)
{
	RP_D(OptionsTab);
	assert(d->hPropSheetPage == nullptr);
	if (d->hPropSheetPage) {
		// Property sheet has already been created.
		return nullptr;
	}

	// tr: Tab title.
	const tstring tsTabTitle = U82T_c(C_("OptionsTab", "Options"));

	PROPSHEETPAGE psp;
	psp.dwSize = sizeof(psp);	
	psp.dwFlags = PSP_USECALLBACK | PSP_USETITLE | PSP_DLGINDIRECT;
	psp.hInstance = HINST_THISCOMPONENT;
	psp.pResource = LoadDialog_i18n(IDD_CONFIG_OPTIONS);
	psp.pszIcon = nullptr;
	psp.pszTitle = tsTabTitle.c_str();
	psp.pfnDlgProc = OptionsTabPrivate::dlgProc;
	psp.lParam = reinterpret_cast<LPARAM>(d);
	psp.pcRefParent = nullptr;
	psp.pfnCallback = OptionsTabPrivate::callbackProc;

	d->hPropSheetPage = CreatePropertySheetPage(&psp);
	return d->hPropSheetPage;
}

/**
 * Reset the contents of this tab.
 */
void OptionsTab::reset(void)
{
	RP_D(OptionsTab);
	d->reset();
}

/**
 * Load the default configuration.
 * This does NOT save, and will only emit modified()
 * if it's different from the current configuration.
 */
void OptionsTab::loadDefaults(void)
{
	RP_D(OptionsTab);
	d->loadDefaults();
}

/**
 * Save the contents of this tab.
 */
void OptionsTab::save(void)
{
	RP_D(OptionsTab);
	if (d->changed) {
		d->save();
	}
}
