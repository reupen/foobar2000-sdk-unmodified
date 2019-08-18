#include "stdafx.h"
#include "CListControl.h"
#include "CListControlHeaderImpl.h" // redundant but makes intelisense quit showing false errors
#include "PaintUtils.h"
#include "GDIUtils.h"
#include "win32_utility.h"
#include <vsstyle.h>

enum {
	lineBelowHeaderCY = 1
};

static bool testDrawLineBelowHeader() {
	// Win10
	return GetOSVersionCode() >= 0xA00;
}

void CListControlHeaderImpl::InitializeHeaderCtrl(DWORD flags) {
	PFC_ASSERT(!IsHeaderEnabled());
	WIN32_OP( m_header.Create(*this,NULL,NULL,WS_CHILD | flags) != NULL);
	m_header.SetFont( GetFont() );

	if (testDrawLineBelowHeader()) {
		m_headerLine.Create( *this, NULL, NULL, WS_CHILD );
	}

	UpdateHeaderLayout();
}

void CListControlHeaderImpl::UpdateHeaderLayout() {
	CRect client; WIN32_OP_D( GetClientRect(client) );
	m_clientWidth = client.Width();
	if (IsHeaderEnabled()) {
		auto rc = client; 
		rc.left -= GetViewOffset().x;
		WINDOWPOS wPos = {};
		HDLAYOUT layout = {&rc, &wPos};
		if (m_header.Layout(&layout)) {
			m_header.SetWindowPos(wPos.hwndInsertAfter,wPos.x,wPos.y,wPos.cx,wPos.cy,wPos.flags | SWP_SHOWWINDOW);
			if (m_headerLine != NULL) m_headerLine.SetWindowPos(m_header, wPos.x, wPos.y + wPos.cy, wPos.cx, lineBelowHeaderCY, wPos.flags | SWP_SHOWWINDOW);
		} else {
			m_header.ShowWindow(SW_HIDE);
			if (m_headerLine != NULL) m_headerLine.ShowWindow(SW_HIDE);
		}
	}
}

int CListControlHeaderImpl::GetItemWidth() const {
	if (IsHeaderEnabled()) return m_itemWidth;
	else return m_clientWidth;
}

LRESULT CListControlHeaderImpl::OnSizePassThru(UINT,WPARAM,LPARAM p_lp) {
	UpdateHeaderLayout();

	ProcessAutoWidth();
	
	SetMsgHandled(FALSE);
	return 0;
}

void CListControlHeaderImpl::OnViewOriginChange(CPoint p_delta) {
	TParent::OnViewOriginChange(p_delta);
	if (p_delta.x != 0) UpdateHeaderLayout();
}

void CListControlHeaderImpl::SetHeaderFont(HFONT font) {
	if (IsHeaderEnabled()) {
		m_header.SetFont(font); UpdateHeaderLayout();
	}
}

LRESULT CListControlHeaderImpl::OnDividerDoubleClick(int,LPNMHDR hdr,BOOL&) {
	const NMHEADER * info = (const NMHEADER *) hdr;
	if (info->iButton == 0) {
		AutoColumnWidth((t_size)info->iItem);
	}
	return 0;
}

LRESULT CListControlHeaderImpl::OnHeaderItemClick(int,LPNMHDR p_hdr,BOOL&) {
	const NMHEADER * info = (const NMHEADER *) p_hdr;
	if (info->iButton == 0) {
		OnColumnHeaderClick((t_uint32)info->iItem);
	}
	return 0;
}

LRESULT CListControlHeaderImpl::OnHeaderItemChanged(int,LPNMHDR p_hdr,BOOL&) {
	const NMHEADER * info = (const NMHEADER*) p_hdr;
	if (info->pitem->mask & (HDI_WIDTH | HDI_ORDER)) {
		ProcessColumnsChange();
	}
	return 0;
}

LRESULT CListControlHeaderImpl::OnHeaderEndDrag(int,LPNMHDR hdr,BOOL&) {
	NMHEADER * info = (NMHEADER*) hdr;
	return OnColumnHeaderDrag(info->iItem,info->pitem->iOrder) ? TRUE : FALSE;
}

bool CListControlHeaderImpl::OnColumnHeaderDrag(t_size index, t_size newPos) {
	index = GetSubItemOrder(index);
	const t_size count = this->GetColumnCount();
	if ( count == 0 ) return false;
	std::vector<size_t> perm; perm.resize(count); pfc::create_move_items_permutation(&perm[0],count, pfc::bit_array_one(index), (int) newPos - (int) index );
	std::vector<int> order, newOrder; order.resize(count); newOrder.resize(count);
	WIN32_OP_D(m_header.GetOrderArray((int)count, &order[0]));
	for(t_size walk = 0; walk < count; ++walk) newOrder[walk] = order[perm[walk]];
	WIN32_OP_D(m_header.SetOrderArray((int)count, &newOrder[0]));
	OnColumnsChanged();
	return true;
}
t_size CListControlHeaderImpl::SubItemFromPointAbs(CPoint pt) const {
	
	auto order = GetColumnOrderArray();
	const t_size colCount = order.size();

	size_t item;
	if (! ItemFromPointAbs(pt, item ) ) item = pfc_infinite;

	long xWalk = 0;
	for(t_size _walk = 0; _walk < colCount; ) {
		const t_size walk = (t_size) order[_walk];

		size_t span = 1;
		if (item != pfc_infinite) span = this->GetSubItemSpan(item, walk);

		PFC_ASSERT( span == 1 || _walk == walk );

		if ( walk + span > colCount ) span = colCount - walk;

		long width = 0;
		for( size_t sub = 0; sub < span; ++ sub ) {
			width += (long)this->GetSubItemWidth(walk + sub);
		}

		if (xWalk + width > pt.x) return walk;
		xWalk += width;
		_walk += span;
	}
	return pfc_infinite;
}

static CRect CheckBoxRect(CRect rc) {
	if (rc.Width() > rc.Height()) {
		rc.right = rc.left + rc.Height();
	}
	return rc;
}

bool CListControlHeaderImpl::OnClickedSpecial(DWORD status, CPoint pt) {
	
	const DWORD maskButtons = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;

	if ( (status & maskButtons) != MK_LBUTTON ) return false;

	if (!GetCellTypeSupported()) return false;

	size_t item; size_t subItem;
	if (! this->GetItemAtPointAbsEx( pt + GetViewOffset(), item, subItem ) ) {
		return false;
	}

	bool bCapture = false;
	auto cellType = GetCellType(item, subItem);
	if ( !CellTypeReactsToMouseOver( cellType ) ) return false;
	auto rcHot = CellHotRect( item, subItem, cellType );
	if (!rcHot.PtInRect( pt )) return false;

	SetPressedItem(item, subItem);
	SetCaptureEx([=](UINT, DWORD newStatus, CPoint pt) {

		size_t newItem, newSubItem;
		if (!this->GetItemAtPointAbsEx(pt + GetViewOffset(), newItem, newSubItem) || newItem != item || newSubItem != subItem) {
			ClearPressedItem(); return false;
		}

		DWORD buttons = newStatus & maskButtons;
		if (buttons == 0) {
			// button released?
			this->defer( [=] {
				OnSubItemClicked(item, subItem, pt);
			} );
			ClearPressedItem(); return false;
		}
		if (buttons != MK_LBUTTON) {
			// another button pressed?
			ClearPressedItem(); return false;
		}

		return true;
	});
	return true;
}

bool CListControlHeaderImpl::CellTypeUsesSpecialHitTests( cellType_t ct ) {
	return ct == cell_hyperlink || ( ct >= cell_button && ct < cell_button_total );
}

bool CListControlHeaderImpl::OnClickedSpecialHitTest(CPoint pt) {
	if ( ! GetCellTypeSupported() ) return false;
	auto ct = GetCellTypeAtPointAbs( pt + GetViewOffset() );
	return CellTypeUsesSpecialHitTests(ct);
}

void CListControlHeaderImpl::OnItemClicked(t_size item, CPoint pt) {
	t_size subItem = SubItemFromPointAbs(pt + GetViewOffset());
	if (subItem != ~0) {
		if ( this->GetCellTypeSupported() ) {
			auto ct = this->GetCellType(item, subItem );
			// we don't handle hyperlink & button clicks thru here
			if (CellTypeUsesSpecialHitTests(ct)) return;
		}
		OnSubItemClicked(item, subItem, pt);
	}
}

std::vector<int> CListControlHeaderImpl::GetColumnOrderArray() const {
	const size_t cCount = this->GetColumnCount();
	std::vector<int> order; 
	if ( cCount > 0 ) {
		order.resize(cCount);
		if (IsHeaderEnabled()) {
			WIN32_OP_D(m_header.GetOrderArray((int)cCount, &order[0]));
		} else {
			for (size_t c = 0; c < cCount; ++c) order[c] = (int)c;
		}
	}
	return order;
}

void CListControlHeaderImpl::RenderItemText(t_size item,const CRect & itemRect,const CRect & updateRect,CDCHandle dc, bool allowColors) {
	
	t_uint32 xWalk = itemRect.left;
	CRect subItemRect(itemRect);
	auto order = GetColumnOrderArray();
	const size_t cCount = order.size();
	SelectObjectScope fontScope(dc,GetFont());
	for(t_size _walk = 0; _walk < cCount; ) {
		const t_size walk = order[_walk];
		
		size_t span = GetSubItemSpan(item, walk);

		PFC_ASSERT( walk == _walk || span == 1 );

		t_uint32 width = GetSubItemWidth(walk);

		if ( span > 1 ) {
			if ( walk + span > cCount ) span = cCount - walk;
			for( size_t extraWalk = 1; extraWalk < span; ++ extraWalk ) {
				width += GetSubItemWidth(walk + extraWalk);
			}
		}

		subItemRect.left = xWalk; subItemRect.right = xWalk + width;
		CRect subUpdate;
		if (subUpdate.IntersectRect(subItemRect, updateRect)) {
			DCStateScope scope(dc);
			if (dc.IntersectClipRect(subItemRect) != NULLREGION) {
				RenderSubItemText(item,walk,subItemRect,subUpdate,dc, allowColors);
			}
		}
		xWalk += width;

		_walk += span;
	}
}

t_size CListControlHeaderImpl::GetSubItemOrder(t_size subItem) const {
	if ( ! IsHeaderEnabled( ) ) return subItem;
	HDITEM hditem = {};
	hditem.mask = HDI_ORDER;
	WIN32_OP_D( m_header.GetItem( (int) subItem, &hditem ) );
	return (t_size) hditem.iOrder;
}

size_t CListControlHeaderImpl::GetSubItemSpan(size_t row, size_t column) const {
	return 1;
}

uint32_t CListControlHeaderImpl::GetSubItemWidth(t_size subItem) const {
	if ( ! IsHeaderEnabled( ) ) {
		// Should be overridden for custom columns layout
		PFC_ASSERT( GetColumnCount() == 1 );
		PFC_ASSERT( subItem == 0 );
		return GetItemWidth();
	}

	if ( subItem < m_colRuntime.size() ) return m_colRuntime[subItem].m_widthPixels;
	PFC_ASSERT( !"bad column idx");
	return 0;
}

int CListControlHeaderImpl::GetHeaderItemWidth( int which ) {
	HDITEM hditem = {};
	hditem.mask = HDI_WIDTH;
	WIN32_OP_D( m_header.GetItem( which, &hditem) );
	return hditem.cxy;
}

void CListControlHeaderImpl::OnColumnsChanged() {
	if ( IsHeaderEnabled() ) {
		for( size_t walk = 0; walk < m_colRuntime.size(); ++ walk ) {
			m_colRuntime[walk].m_widthPixels = GetHeaderItemWidth( (int) walk );
		}
	}
	this->OnViewAreaChanged();
}

void CListControlHeaderImpl::ResetColumns(bool update) {
	m_colRuntime.clear();
	m_itemWidth = 0;
	PFC_ASSERT(IsHeaderEnabled());
	for(;;) {
		int count = m_header.GetItemCount();
		if (count <= 0) break;
		m_header.DeleteItem(count - 1);
	}
	if (update) OnColumnsChanged();
}

void CListControlHeaderImpl::SetColumn( size_t which, const char * label, DWORD fmtFlags, bool updateView) {
	PFC_ASSERT( IsHeaderEnabled() );
	pfc::stringcvt::string_os_from_utf8 labelOS(label);

	HDITEM item = {};
	item.mask = HDI_TEXT | HDI_FORMAT;
	item.fmt = fmtFlags | HDF_STRING;
	item.pszText = const_cast<TCHAR*>(labelOS.get_ptr());
	m_header.SetItem( (int) which, &item );

	if (updateView) OnColumnsChanged();
}

void CListControlHeaderImpl::ResizeColumn(t_size index, t_uint32 widthPixels, bool updateView) {
	PFC_ASSERT( IsHeaderEnabled() );
	PFC_ASSERT( index < m_colRuntime.size() );
	HDITEM item = {};
	item.mask = HDI_WIDTH;
	item.cxy = widthPixels;
	m_header.SetItem( (int) index, &item );
	m_colRuntime[index].m_widthPixels = widthPixels;
	RecalcItemWidth();
	if (updateView) OnColumnsChanged();
}

void CListControlHeaderImpl::DeleteColumns( pfc::bit_array const & mask, bool updateView ) {
	int nDeleted = 0;
	const size_t oldCount = GetColumnCount();
	mask.for_each(true, 0, oldCount, [&] (size_t idx) {
		int iDelete = (int) idx - nDeleted;
		bool bDeleted = m_header.DeleteItem( iDelete );
		PFC_ASSERT( bDeleted );
		if ( bDeleted ) ++ nDeleted;
		} );

	pfc::remove_mask_t( m_colRuntime, mask );

	ColumnWidthFix();

	if (updateView) {
		OnColumnsChanged();
	}

}

bool CListControlHeaderImpl::DeleteColumn(size_t index, bool update) {
	PFC_ASSERT( IsHeaderEnabled() );

	if (!m_header.DeleteItem( (int) index )) return false;

	pfc::remove_mask_t( m_colRuntime, pfc::bit_array_one( index ) );

	ColumnWidthFix();

	if (update) {
		OnColumnsChanged();
	}

	return true;
}

void CListControlHeaderImpl::SetSortIndicator( size_t whichColumn, bool isUp ) {
	HeaderControl_SetSortIndicator( GetHeaderCtrl(), (int) whichColumn, isUp );
}

void CListControlHeaderImpl::ClearSortIndicator() {
	HeaderControl_SetSortIndicator(GetHeaderCtrl(), -1, false);
}

bool CListControlHeaderImpl::HaveAutoWidthContentColumns() const {
	for( auto i = m_colRuntime.begin(); i != m_colRuntime.end(); ++ i ) {
		if ( i->autoWidthContent() ) return true;
	}
	return false;
}
bool CListControlHeaderImpl::HaveAutoWidthColumns() const {
	for( auto i = m_colRuntime.begin(); i != m_colRuntime.end(); ++ i ) {
		if ( i->autoWidth() ) return true;
	}
	return false;
}
void CListControlHeaderImpl::AddColumnEx( const char * label, uint32_t widthPixelsAt96DPI, DWORD fmtFlags, bool update ) {
	uint32_t w = widthPixelsAt96DPI;
	if ( w <= columnWidthMax ) {
		w = MulDiv( w, m_dpi.cx, 96 );
	}
	AddColumn( label, w, fmtFlags, update );
}

void CListControlHeaderImpl::AddColumnDLU( const char * label, uint32_t widthDLU, DWORD fmtFlags, bool update ) {
	uint32_t w = widthDLU;
	if ( w <= columnWidthMax ) {
		w = ::MapDialogWidth( GetParent(), w );
	}
	AddColumn( label, w, fmtFlags, update );
}

void CListControlHeaderImpl::AddColumn(const char * label, uint32_t width, DWORD fmtFlags,bool update) {
	if (! IsHeaderEnabled( ) ) InitializeHeaderCtrl();

	pfc::stringcvt::string_os_from_utf8 labelOS(label);
	HDITEM item = {};
	item.mask = HDI_TEXT | HDI_FORMAT;
	if ( width != UINT32_MAX ) {
		item.cxy = width;
		item.mask |= HDI_WIDTH;
	}
	
	item.pszText = const_cast<TCHAR*>(labelOS.get_ptr());
	item.fmt = HDF_STRING | fmtFlags;
	int iColumn;
	WIN32_OP_D( (iColumn = m_header.InsertItem(m_header.GetItemCount(),&item) ) >= 0 );
	colRuntime_t rt;
	rt.m_text = label;
	rt.m_userWidth = width;
	if ( width <= columnWidthMax ) {
		m_itemWidth += width;
		rt.m_widthPixels = width;
	}
	m_colRuntime.push_back( std::move(rt) );
	
	if (update) OnColumnsChanged();

	ProcessAutoWidth();
}

void CListControlHeaderImpl::RenderBackground(CDCHandle dc, CRect const& rc) {
	__super::RenderBackground(dc,rc);
#if 0
	if ( m_drawLineBelowHeader && IsHeaderEnabled()) {
		CRect rcHeader;
		if (m_header.GetWindowRect(rcHeader)) {
			// Draw a grid line below header
			int y = rcHeader.Height();
			if ( y >= rc.top && y < rc.bottom ) {
				CDCPen pen(dc, GridColor());
				SelectObjectScope scope(dc, pen);
				dc.MoveTo(rc.left, y);
				dc.LineTo(rc.right, y);
			}
		}
	}
#endif
}

CRect CListControlHeaderImpl::GetClientRectHook() const {
	CRect rcClient = __super::GetClientRectHook();
	if (m_header != NULL) {
		PFC_ASSERT( m_header.IsWindow() );
		CRect rcHeader;
		if (m_header.GetWindowRect(rcHeader)) {
			int h = rcHeader.Height();
			if ( m_headerLine != NULL ) h += lineBelowHeaderCY;
			rcClient.top = pfc::min_t(rcClient.bottom,rcClient.top + h);
		}
	}
	return rcClient;
}

CRect CListControlHeaderImpl::GetItemTextRectHook(size_t, size_t, CRect const & rc) const {
	return GetItemTextRect( rc );
}

CRect CListControlHeaderImpl::GetItemTextRect(const CRect & itemRect) const {
	CRect rc ( itemRect );
	rc.DeflateRect(GetColumnSpacing(),0);
	return rc;
}

void CListControlHeaderImpl::SetColumnSort(t_size which, bool isUp) {
	HeaderControl_SetSortIndicator(m_header,(int)which,isUp);
}

void CListControlHeaderImpl::SetColumnFormat(t_size which, DWORD format) {
	HDITEM item = {};
	item.mask = HDI_FORMAT;
	item.fmt = HDF_STRING | format;
	WIN32_OP_D( m_header.SetItem((int)which,&item) );
}

DWORD CListControlHeaderImpl::GetColumnFormat(t_size which) const {
	if (!IsHeaderEnabled()) return HDF_LEFT;
	HDITEM hditem = {};
	hditem.mask = HDI_FORMAT;
	WIN32_OP_D( m_header.GetItem( (int) which, &hditem) );
	return hditem.fmt;
}

BOOL CListControlHeaderImpl::OnSetCursor(CWindow wnd, UINT nHitTest, UINT message) {
#if 0
	// no longer meaningful since SetCapture on mouse over was added to track hot status
	if ( message != 0 && GetCellTypeSupported() ) {
		CPoint pt( (LPARAM) GetMessagePos() );
		WIN32_OP_D( ScreenToClient( &pt ) );
		if ( GetCellTypeAtPointAbs( pt + GetViewOffset() ) == cell_hyperlink ) {
			SetCursor(LoadCursor(NULL, IDC_HAND)); return TRUE;
		}
	}
#endif
	SetMsgHandled(FALSE); return FALSE;
}

bool CListControlHeaderImpl::GetItemAtPointAbsEx(CPoint pt, size_t & outItem, size_t & outSubItem) const {
	size_t item, subItem;
	if (ItemFromPointAbs(pt, item)) {
		subItem = SubItemFromPointAbs(pt);
		if (subItem != pfc_infinite) {
			outItem = item; outSubItem = subItem; return true;
		}
	}
	return false;
}

CListControlHeaderImpl::cellType_t CListControlHeaderImpl::GetCellTypeAtPointAbs(CPoint pt) const {
	size_t item, subItem;
	if ( GetItemAtPointAbsEx( pt, item, subItem) ) {
		return GetCellType( item, subItem );
	}
	return cell_none;
}

static void RenderCheckbox( CTheme & theme, CWindow wnd, CDCHandle dc, CRect rcCheckBox, unsigned stateFlags, bool bRadio ) {

	const int part = bRadio ? BP_RADIOBUTTON : BP_CHECKBOX;

	const bool bDisabled = ! wnd.IsWindowEnabled();
	const bool bPressed = (stateFlags & CListControlHeaderImpl::cellState_pressed ) != 0;
	const bool bHot = ( stateFlags & CListControlHeaderImpl::cellState_hot ) != 0;

	if (theme != NULL && IsThemePartDefined(theme, part, 0)) {
		int state = 0;
		if (bDisabled) {
			state = bPressed ? CBS_CHECKEDDISABLED : CBS_DISABLED;
		} else if ( bHot ) {
			state = bPressed ? CBS_CHECKEDHOT : CBS_HOT;
		} else {
			state = bPressed ? CBS_CHECKEDNORMAL : CBS_NORMAL;
		}

		CSize size;
		if (SUCCEEDED(GetThemePartSize(theme, dc, part, state, rcCheckBox, TS_TRUE, &size))) {
			if (size.cx <= rcCheckBox.Width() && size.cy <= rcCheckBox.Height()) {
				CRect rc = rcCheckBox;
				rc.left += ( rc.Width() - size.cx ) / 2;
				rc.top += ( rc.Height() - size.cy ) / 2;
				rc.right = rc.left + size.cx;
				rc.bottom = rc.top + size.cy;
				DrawThemeBackground(theme, dc, part, state, rc, &rc);
				return;
			}
		}
	}
	int stateEx = bRadio ? DFCS_BUTTONRADIO : DFCS_BUTTONCHECK;
	if ( bPressed ) stateEx |= DFCS_CHECKED;
	if ( bDisabled ) stateEx |= DFCS_INACTIVE;
	else if ( bHot ) stateEx |= DFCS_HOT;
	DrawFrameControl(dc, rcCheckBox, DFC_BUTTON, stateEx);
}

static void RenderButton( CTheme & theme, CWindow wnd, CDCHandle dc, CRect rcButton, CRect rcUpdate, CListControlHeaderImpl::cellState_t cellState ) {

	const int part = BP_PUSHBUTTON;

	enum {
		stNormal = PBS_NORMAL,
		stHot = PBS_HOT,
		stDisabled = PBS_DISABLED,
		stPressed = PBS_PRESSED,
	};

	int state = 0;
	if (!wnd.IsWindowEnabled()) state = stDisabled;
	if ( cellState & CListControlHeaderImpl::cellState_pressed ) state = stPressed;
	else if ( cellState & CListControlHeaderImpl::cellState_hot ) state = stHot;
	else state = stNormal;

	CRect rcClient  = rcButton;

	if (theme != NULL && IsThemePartDefined(theme, part, 0)) {
		DrawThemeBackground(theme, dc, part, state, rcClient, &rcUpdate);
	} else {
		int stateEx = DFCS_BUTTONPUSH;
		switch (state) {
		case stPressed: stateEx |= DFCS_PUSHED; break;
		case stDisabled: stateEx |= DFCS_INACTIVE; break;
		}
		DrawFrameControl(dc, rcClient, DFC_BUTTON, stateEx);
	}
}

void CListControlHeaderImpl::RenderSubItemTextInternal2(DWORD hdrFormat, cellType_t cellType, cellState_t cellState, const CRect & subItemRect, CDCHandle dc, const char * text, bool allowColors, double textScale, CRect rcHot, CRect rcText) {

	// add drawing of any new custom cell types here
	PFC_ASSERT( cellType == cell_hyperlink || cellType == cell_button || cellType == cell_text || cellType == cell_multitext || cellType == cell_button_lite || cellType == cell_button_glyph || cellType == cell_checkbox || cellType == cell_radiocheckbox );
	
	const bool bHyperLink = (cellType == cell_hyperlink) && allowColors;
	const bool bPressed = (cellState & cellState_pressed) != 0;
	const bool bHot = (cellState & cellState_hot) != 0;
	const bool bGlyph = (cellType == cell_button_glyph);

	if ( bGlyph ) {
		textScale *= 1.3;
	}
	
	if ( cellType == cell_button_glyph ) cellType = cell_button_lite; // from here on cell_button_glyph is the same as cell_button_lite
	
	if ( ( bHot || bPressed ) && cellType == cell_button_lite ) cellType = cell_button;

	pfc::stringcvt::string_os_from_utf8 cvt(text);
	CRect clip = rcText;

	const uint32_t fgWas = dc.GetTextColor();

	if (cellType == cell_button) {
		RenderButton( themeFor("BUTTON"), *this, dc, rcHot, rcHot, cellState );
	}

	const t_uint32 format = PaintUtils::DrawText_TranslateHeaderAlignment(hdrFormat);
	const t_uint32 bk = dc.GetBkColor();
	const t_uint32 fg = bHyperLink ? GetSysColorHook(colorHighlight) : fgWas;
	const t_uint32 hl = (allowColors ? GetSysColorHook(colorHighlight) : fg);
	const t_uint32 colors[3] = { PaintUtils::BlendColor(bk, fg, 33), fg, hl };

	CFontHandle fontRestore;
	CFont fontOverride;

	bool bUnderline = false, bBold = false;
	if ( bHyperLink && bHot ) {
		bUnderline = true;
	}

#if 0
	if ( bHyperLink && bPressed ) {
		bBold = true;
	}

	if (cellType == cell_button_lite && bPressed) {
		bBold = true;
	}
#endif

	if ( bUnderline || bBold || textScale != 1.0 ) {
		LOGFONT data;
		if (dc.GetCurrentFont().GetLogFont(&data)) {
			if ( bUnderline ) data.lfUnderline = TRUE;
			if ( bBold ) data.lfWeight = FW_BOLD;
			if ( textScale != 1.0 ) data.lfHeight = pfc::rint32( data.lfHeight * textScale );
			if (fontOverride.CreateFontIndirect( & data )) {
				fontRestore = dc.SelectFont( fontOverride );
			}
		}
	}
	

	

	switch (cellType) {
	case cell_radiocheckbox:
	case cell_checkbox:
		if (subItemRect.Width() > subItemRect.Height() ) {
			CRect rcCheckbox = subItemRect;
			rcCheckbox.right = rcCheckbox.left + rcCheckbox.Height();
			RenderCheckbox( themeFor("BUTTON"), *this, dc, rcCheckbox, cellState, (cellType == cell_radiocheckbox) );
			SetTextColorScope cs(dc, colors[1]);
			CRect rcText = subItemRect;
			rcText.left = rcCheckbox.right;
			dc.DrawText(cvt, (int)cvt.length(), rcText, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_LEFT);
		} else {
			RenderCheckbox(themeFor("BUTTON"), *this, dc, subItemRect, cellState, (cellType == cell_radiocheckbox));
		}
		break;
	case cell_button:
	case cell_button_lite:
		{
			SetTextColorScope cs(dc, colors[1]);
			dc.DrawText(cvt, (int)cvt.length(), clip, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_CENTER);
		}
		break;
	case cell_multitext:
		{
			SetTextColorScope cs(dc, colors[1]);
			auto format2 = DT_NOPREFIX | DT_VCENTER | format;
			CRect rcDraw = clip;
			dc.DrawText(cvt, (int)cvt.length(), rcDraw, format | DT_CALCRECT);
			auto txSize = rcDraw.Size();
			rcDraw = clip;
			if ( txSize.cy < rcDraw.Height() ) {
				int sub = rcDraw.Height() - txSize.cy;
				rcDraw.top += sub/2;
				rcDraw.bottom = rcDraw.top + txSize.cy;
			}
			dc.DrawText(cvt, (int)cvt.length(), rcDraw, format2);
		}
		break;
	default:
		PaintUtils::TextOutColorsEx(dc, cvt, clip, format, colors);
		break;
	}
	

	if ( fontRestore ) dc.SelectFont( fontRestore );

	dc.SetTextColor(fgWas);
}

void CListControlHeaderImpl::RenderSubItemTextInternal(t_size subItem, const CRect & subItemRect, CDCHandle dc, const char * text, bool allowColors) {
	pfc::stringcvt::string_os_from_utf8 cvt(text);
	CRect clip = GetItemTextRect(subItemRect);
	const t_uint32 format = PaintUtils::DrawText_TranslateHeaderAlignment(GetColumnFormat(subItem));
	if (true) {
		const t_uint32 bk = dc.GetBkColor();
		const t_uint32 fg = dc.GetTextColor();
		const t_uint32 hl = (allowColors ? GetSysColorHook(colorHighlight) : fg);
		const t_uint32 colors[3] = {PaintUtils::BlendColor(bk, fg, 33), fg, hl};

		PaintUtils::TextOutColorsEx(dc, cvt, clip, format, colors);
		
		dc.SetTextColor(fg);
	} else {
		dc.DrawText(cvt,(int)cvt.length(),clip,DT_NOPREFIX | DT_END_ELLIPSIS | DT_SINGLELINE | DT_VCENTER | format );
	}
}
void CListControlHeaderImpl::RenderSubItemText(t_size item, t_size subItem,const CRect & subItemRect,const CRect & updateRect,CDCHandle dc, bool allowColors) {
	pfc::string_formatter label;
	const auto cellType = GetCellType( item, subItem );
	const bool bHaveText = GetSubItemText(item,subItem,label);
	if (! bHaveText ) {
		switch(cellType) {
		case cell_text:
		case cell_multitext:
		case cell_hyperlink:
			return;// nothing to do
		}

		label = ""; //sanity
	}

	bool bPressed;
	cellState_t state = cellState_none;
	if ( cellType == cell_checkbox || cellType == cell_radiocheckbox ) bPressed = this->GetCellCheckState(item, subItem);
	else bPressed = (item == m_pressedItem) && (subItem == m_pressedSubItem);
	bool bHot = (item == m_hotItem) && ( subItem == m_hotSubItem );
	if ( bPressed ) state |= cellState_pressed;
	if ( bHot ) state |= cellState_hot;
	auto rcText = GetItemTextRectHook(item, subItem, subItemRect);
	auto rcHot = CellHotRect(item, subItem, cellType, subItemRect);
	RenderSubItemTextInternal2( GetColumnFormat(subItem), cellType, state, subItemRect, dc, label, allowColors, CellTextScale(item, subItem), rcHot, rcText );
}

void CListControlHeaderImpl::RenderGroupHeaderText(int id,const CRect & headerRect,const CRect & updateRect,CDCHandle dc) {
	pfc::string_formatter label;
	if (GetGroupHeaderText(id,label)) {
		SelectObjectScope fontScope(dc,GetGroupHeaderFont());
		pfc::stringcvt::string_os_from_utf8 cvt(label);
		CRect contentRect(GetItemTextRect(headerRect));
		dc.DrawText(cvt,(int)cvt.length(),contentRect,DT_NOPREFIX | DT_END_ELLIPSIS | DT_SINGLELINE | DT_VCENTER | DT_LEFT );
		SIZE txSize;
		const int lineSpacing = contentRect.Height() / 2;
		if (dc.GetTextExtent(cvt,(int)cvt.length(),&txSize)) {
			if (txSize.cx + lineSpacing < contentRect.Width()) {
				const CPoint center = contentRect.CenterPoint();
				const CPoint pt1(contentRect.left + txSize.cx + lineSpacing, center.y), pt2(contentRect.right, center.y);
				const COLORREF lineColor = PaintUtils::BlendColor(dc.GetTextColor(),dc.GetBkColor(),25);

#ifndef CListControl_ScrollWindowFix
#error FIXME CMemoryDC needed
#endif
				PaintUtils::DrawSmoothedLine(dc, pt1, pt2, lineColor, 1.0 * (double)m_dpi.cy / 96.0);
			}
		}
	}
}

uint32_t CListControlHeaderImpl::GetOptimalColumnWidthFixed(const char * fixedText) const {
	CWindowDC dc(*this);
	SelectObjectScope fontScope(dc, GetFont());
	GetOptimalWidth_Cache cache;
	cache.m_dc = dc;
	cache.m_stringTemp = fixedText;
	return cache.GetStringTempWidth() + this->GetColumnSpacing() * 2;
}

t_uint32 CListControlHeaderImpl::GetOptimalSubItemWidth(t_size item, t_size subItem, GetOptimalWidth_Cache & cache) const {
	const t_uint32 base = this->GetColumnSpacing() * 2;
	if (GetSubItemText(item,subItem,cache.m_stringTemp)) {
		return base + cache.GetStringTempWidth();
	} else {
		return base;
	}
}

t_uint32 CListControlHeaderImpl::GetOptimalWidth_Cache::GetStringTempWidth() {
	if (m_stringTemp.replace_string_ex(m_stringTempUnfuckAmpersands, "&", "&&") > 0) {
		m_convertTemp.convert(m_stringTempUnfuckAmpersands);
	} else {
		m_convertTemp.convert(m_stringTemp);
	}
	return PaintUtils::TextOutColors_CalcWidth(m_dc, m_convertTemp);
}

t_uint32 CListControlHeaderImpl::GetOptimalColumnWidth(t_size which, GetOptimalWidth_Cache & cache) const {
	const t_size totalItems = GetItemCount();
	t_uint32 val = 0;
	for(t_size item = 0; item < totalItems; ++item) {
		pfc::max_acc( val, GetOptimalSubItemWidth( item, which, cache ) );
	}
	return val;
}

t_uint32 CListControlHeaderImpl::GetOptimalSubItemWidthSimple(t_size item, t_size subItem) const {
	CWindowDC dc(*this);
	SelectObjectScope fontScope(dc, GetFont() );
	GetOptimalWidth_Cache cache;
	cache.m_dc = dc;
	return GetOptimalSubItemWidth(item, subItem, cache);
}

LRESULT CListControlHeaderImpl::OnKeyDown(UINT,WPARAM wp,LPARAM,BOOL& bHandled) {
	switch(wp) {
	case VK_ADD:
		if (IsKeyPressed(VK_CONTROL)) {
			AutoColumnWidths();
			return 0;
		}
		break;
	}
	bHandled = FALSE;
	return 0;
}

uint32_t CListControlHeaderImpl::GetOptimalColumnWidth(size_t colIndex) const {
	CWindowDC dc(*this);
	SelectObjectScope fontScope(dc, GetFont());
	GetOptimalWidth_Cache cache;
	cache.m_dc = dc;
	uint32_t ret = 0;
	const auto itemCount = GetItemCount();
	for (t_size walk = 0; walk < itemCount; ++walk) {
		pfc::max_acc(ret, GetOptimalSubItemWidth(walk, colIndex, cache));
	}
	return ret;
}

void CListControlHeaderImpl::AutoColumnWidths(const pfc::bit_array & mask, bool expandLast) {
	PFC_ASSERT( IsHeaderEnabled() );
	if (!IsHeaderEnabled()) return;
	const t_size itemCount = GetItemCount();
	if (itemCount == 0) return;
	const t_size columnCount = (t_size) m_header.GetItemCount();
	if (columnCount == 0) return;
	pfc::array_t<t_uint32> widths; widths.set_size(columnCount); widths.fill_null();
	{
		CWindowDC dc(*this);
		SelectObjectScope fontScope(dc,GetFont());
		GetOptimalWidth_Cache cache;
		cache.m_dc = dc;
		for(t_size walk = 0; walk < itemCount; ++walk) {
			for(t_size colWalk = mask.find_first(true,0,columnCount); colWalk < columnCount; colWalk = mask.find_next(true,colWalk,columnCount)) {
				pfc::max_acc(widths[colWalk], GetOptimalSubItemWidth(walk,colWalk,cache));
			}
		}
	}

	if (expandLast) {
		uint32_t usedWidth = 0; size_t lastCol = SIZE_MAX;
		pfc::array_t<int> order; order.set_size(columnCount);
		WIN32_OP_D( m_header.GetOrderArray((int)columnCount,order.get_ptr()) );
		for(size_t _walk = 0; _walk < columnCount; ++_walk) {
			const size_t colWalk = (size_t) order[_walk];
			PFC_ASSERT( colWalk < columnCount );
			if (mask[colWalk]) {
				lastCol = colWalk;
				usedWidth += widths[colWalk];
			} else {
				usedWidth += GetSubItemWidth(colWalk);
			}
		}
		if (lastCol != SIZE_MAX) {
			t_uint32 clientWidth = this->GetClientRectHook().Width(); 
			if (clientWidth > 0) --clientWidth; // $!@# scrollbar hack
			if (usedWidth < clientWidth) {
				widths[lastCol] += clientWidth - usedWidth;
			}
		}
	}
	for(t_size colWalk = mask.find_first(true,0,columnCount); colWalk < columnCount; colWalk = mask.find_next(true,colWalk,columnCount)) {
		ResizeColumn(colWalk,widths[colWalk],false);
	}
	ProcessColumnsChange();
}

t_uint32 CListControlHeaderImpl::GetOptimalGroupHeaderWidth(int which) const {
	CWindowDC dc(*this);
	SelectObjectScope fontScope(dc,GetGroupHeaderFont());
	GetOptimalWidth_Cache cache; cache.m_dc = dc;
	const t_uint32 base = this->GetColumnSpacing() * 2;
	if (GetGroupHeaderText(which,cache.m_stringTemp)) {
		return base + cache.GetStringTempWidth();
	} else {
		return base;
	}
}

size_t CListControlHeaderImpl::GetColumnCount() const {
	if ( ! IsHeaderEnabled() ) return 1;
#if PFC_DEBUG
	int iHeaderCount = m_header.GetItemCount();
	PFC_ASSERT( m_colRuntime.size() == (size_t) iHeaderCount );
#endif
	return m_colRuntime.size();
}

void CListControlHeaderImpl::ColumnWidthFix() {
	if ( this->HaveAutoWidthColumns() ) {
		ProcessAutoWidth();
	} else {
		RecalcItemWidth();
	}
}

void CListControlHeaderImpl::ProcessAutoWidth() {
	if ( HaveAutoWidthColumns() ) {
		const int clientWidth = this->GetClientRectHook().Width();

		if ( ! this->HaveAutoWidthContentColumns( ) && clientWidth == m_itemWidth) return;

		const size_t count = GetColumnCount();
		uint32_t totalNonAuto = 0;
		size_t numAutoWidth = 0;
		for(size_t walk = 0; walk < count; ++ walk ) {
			if ( m_colRuntime[walk].autoWidth() ) {
				++ numAutoWidth;
			} else {
				totalNonAuto += GetSubItemWidth(walk);
			}
		}
		int toDivide = clientWidth - totalNonAuto;
		if ( toDivide < 0 ) toDivide = 0;

		size_t numLeft = numAutoWidth;
		auto worker = [&] ( size_t iCol ) {
			auto & rt = m_colRuntime[iCol];
			int lo = this->GetOptimalColumnWidthFixed( rt.m_text.c_str() );
			if ( rt.autoWidthContent() ) {
				int lo2 = this->GetOptimalColumnWidth( iCol );
				if ( lo < lo2 ) lo = lo2;
			}
			int width = (int)(toDivide / numLeft);
			if ( width < lo ) width = lo;
			
			HDITEM item = {};
			item.mask = HDI_WIDTH;
			item.cxy = width;
			WIN32_OP_D( m_header.SetItem( iCol, &item ) );
			rt.m_widthPixels = width;

			if ( toDivide > width ) {
				toDivide -= width;
			} else {
				toDivide = 0;
			}
			-- numLeft;

		};
		for( size_t iCol = 0; iCol < count; ++ iCol ) {
			if (m_colRuntime[iCol].autoWidthContent() ) worker(iCol);
		}
		for( size_t iCol = 0; iCol < count; ++ iCol ) {
			if ( m_colRuntime[iCol].autoWidthPlain() ) worker(iCol);
		}

		RecalcItemWidth();
		OnColumnsChanged();
		m_header.Invalidate();
	}
}

void CListControlHeaderImpl::RecalcItemWidth() {
	int total = 0;
	const t_size count = GetColumnCount();
	for(t_size walk = 0; walk < count; ++walk) total += GetSubItemWidth(walk);
	m_itemWidth = total;
}

CRect CListControlHeaderImpl::GetSubItemRectAbs(t_size item,t_size subItem) const {
	CRect rc = GetItemRectAbs(item);
	auto order = GetColumnOrderArray();
	const t_size colCount = order.size();
	for(t_size _walk = 0; _walk < colCount; ++_walk) {
		const t_size walk = (t_size) order[_walk];

		t_size width = this->GetSubItemWidth(walk);
		if (subItem == walk) {
			
			size_t span = GetSubItemSpan(item, walk);
			if ( walk + span > colCount ) span = colCount - walk;
			for( size_t extra = 1; extra < span; ++ extra ) {
				width += GetSubItemWidth( walk + extra);
			}
			
			rc.right = rc.left + (long)width;

			return rc;
		} else {
			rc.left += (long)width;
		}
	}
	throw pfc::exception_invalid_params();
}

CRect CListControlHeaderImpl::GetSubItemRect(t_size item,t_size subItem) const {
	CRect rc = GetSubItemRectAbs(item,subItem); rc.OffsetRect(-GetViewOffset()); return rc;
}

void CListControlHeaderImpl::SetHotItem(size_t row, size_t column) {
	if ( m_hotItem != row  || m_hotSubItem != column ) {
		if (m_hotItem != pfc_infinite) InvalidateRect(GetSubItemRect(m_hotItem, m_hotSubItem));
		m_hotItem = row; m_hotSubItem = column;
		if (m_hotItem != pfc_infinite) InvalidateRect(GetSubItemRect(m_hotItem, m_hotSubItem));
	}
}

void CListControlHeaderImpl::SetPressedItem(size_t row, size_t column) {
	if (m_pressedItem != row || m_pressedSubItem != column) {
		if (m_pressedItem != pfc_infinite) InvalidateRect(GetSubItemRect(m_pressedItem, m_pressedSubItem));
		m_pressedItem = row; m_pressedSubItem = column;
		if (m_pressedItem != pfc_infinite) InvalidateRect(GetSubItemRect(m_pressedItem, m_pressedSubItem));
	}
}


bool CListControlHeaderImpl::ToggleSelectedItemsHook(const pfc::bit_array & mask) {
	if (this->GetCellTypeSupported() ) {
		bool handled = false;
		bool setTo = true;

		mask.walk(GetItemCount(), [&](size_t idx) {
			auto ct = this->GetCellType(idx, 0);
			if ( ct == cell_radiocheckbox ) {
				if (!handled) {
					handled = true;
					setTo = !this->GetCellCheckState(idx, 0);
					this->SetCellCheckState(idx, 0, setTo);
				}
			} else if ( ct == cell_checkbox) {
				if (!handled) {
					handled = true;
					setTo = ! this->GetCellCheckState(idx,0);
				}
				this->SetCellCheckState(idx,0,setTo);
			}
		});

		if (handled) return true;
	}
	return __super::ToggleSelectedItemsHook(mask);
}

void CListControlHeaderImpl::OnSubItemClicked(t_size item, t_size subItem, CPoint pt) {
	auto ct = GetCellType(item, subItem);
	if ( ct == cell_checkbox || ct == cell_radiocheckbox ) {
		if (CheckBoxRect(GetSubItemRect(item, subItem)).PtInRect(pt)) {
			this->SetCellCheckState( item, subItem, ! GetCellCheckState( item, subItem ) );
		}
	}
}


bool CListControlHeaderImpl::AllowTypeFindInCell(size_t item, size_t subItem) const {
	switch(GetCellType( item, subItem ) ) {
	default:
		return false;
		// By default typefind in text cells and checkbox labels
	case cell_text:
	case cell_multitext:
	case cell_checkbox:
	case cell_radiocheckbox:
		return true;
	}
}

bool CListControlHeaderImpl::CellTypeReactsToMouseOver(cellType_t ct) {
	if ( ct == cell_hyperlink ) return true;
	if ( ct >= cell_button && ct < cell_button_total ) return true;
	if ( ct >= cell_checkbox && ct <= cell_checkbox_total ) return true;
	return false;
}

CRect CListControlHeaderImpl::CellHotRect( size_t, size_t, cellType_t ct, CRect rcCell) {
	if ( ct >= cell_checkbox && ct < cell_checkbox_total ) {
		return CheckBoxRect(rcCell);
	}
	return rcCell;
}
CRect CListControlHeaderImpl::CellHotRect(size_t item, size_t subItem, cellType_t ct) {
	return CellHotRect( item, subItem, ct, GetSubItemRect( item, subItem ) );
}
void CListControlHeaderImpl::OnMouseMove(UINT nFlags, CPoint pt) {
	const DWORD maskButtons = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
	if (GetCellTypeSupported() && (nFlags & maskButtons) == 0 ) {
		size_t item; size_t subItem;
		if (this->GetItemAtPointAbsEx(pt + GetViewOffset(), item, subItem)) {
			auto ct = this->GetCellType( item, subItem );
			if (CellTypeReactsToMouseOver(ct) ) {
				auto rc = CellHotRect( item, subItem, ct );
				if ( PtInRect( rc, pt ) ) {
					if (ct == cell_hyperlink) {
						SetCursor(LoadCursor(NULL, IDC_HAND));
					}
					SetHotItem(item, subItem);
					SetCaptureEx([=](UINT msg, DWORD newStatus, CPoint pt) {
						if ((newStatus & maskButtons) != 0 || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL ) {
							// A button has been pressed or wheel has been moved
							this->ClearHotItem();
							SetCaptureMsgHandled(FALSE);
							return false;
						}

						if ( ! PtInRect( rc, pt ) ) {
							// Left the rect
							this->ClearHotItem();
							SetCaptureMsgHandled(FALSE);
							return false;
						}

						return true;
					});
				}
			}
		}
	}
	SetMsgHandled(FALSE);
}

bool CListControlHeaderImpl::AllowScrollbar(bool vertical) const {
	if ( vertical ) {
		// vertical
		return true;
	} else {
		// horizontal
		if (! IsHeaderEnabled( ) ) return false; // no header?
		return true;
	}
}

void CListControlHeaderImpl::OnDestroy() {
	m_header = NULL;
	SetMsgHandled(FALSE);
}


uint32_t CListControlHeaderImpl::GetColumnsBlankWidth( size_t colExclude ) const {
	auto client = this->GetClientRectHook().Width();
	int item = GetItemWidth();
	if (colExclude) item -= GetSubItemWidth(colExclude);
	if ( item  < 0 ) item = 0;
	if ( item < client ) return (uint32_t)( client - item );
	else return 0;
}

void CListControlHeaderImpl::SizeColumnToContent( size_t which, uint32_t minWidth ) {
	auto width = this->GetOptimalColumnWidth( which );
	if ( width < minWidth ) width = minWidth;
	this->ResizeColumn( which, width );
}

void CListControlHeaderImpl::SizeColumnToContentFillBlank( size_t which ) {
	this->SizeColumnToContent( which, this->GetColumnsBlankWidth(which) );
}

HBRUSH CListControlHeaderImpl::OnCtlColorStatic(CDCHandle dc, CStatic wndStatic) {
	if ( wndStatic == m_headerLine ) {
		COLORREF col = GridColor();
		dc.SetDCBrushColor( col );
		return (HBRUSH) GetStockObject(DC_BRUSH);
	}
	SetMsgHandled(FALSE);
	return NULL;
}

void CListControlHeaderImpl::ReloadData() {
	__super::ReloadData();
	if ( this->HaveAutoWidthContentColumns( ) ) {
		this->ColumnWidthFix();
	}
}
