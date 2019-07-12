#pragma once

// ================================================================================
// Main CListControl implementation
// 
// For ready-to-use CListControl specializations, 
// see CListControlSimple.h and CListControlOwnerData.h
// ================================================================================


#pragma comment(lib, "uxtheme.lib")

#include <functional>
#include <list>
#include <vector>
#include <set>
#include "CMiddleDragImpl.h"
#include "wtl-pp.h"
#include "gesture.h"
#include "gdiplus_helpers.h"

#define CListControl_ScrollWindowFix

#ifdef CListControl_ScrollWindowFix
#define WS_EX_COMPOSITED_CListControl 0
#else
#define WS_EX_COMPOSITED_CListControl WS_EX_COMPOSITED
#endif


typedef std::function< bool ( UINT, DWORD, CPoint ) > CaptureProc_t;

typedef CWinTraits<WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_COMPOSITED_CListControl> CListControlTraits;

class CListControlImpl : public CWindowImpl<CListControlImpl,CWindow,CListControlTraits> {
public:
	CListControlImpl() : m_wheelAccumX(), m_wheelAccumY(), m_viewOrigin(0,0), m_sizeAsyncPending(false), m_dpi(QueryScreenDPIEx()), m_ensureVisibleUser() {}
	
	DECLARE_WND_CLASS_EX(TEXT("{4B94B650-C2D8-40de-A0AD-E8FADF62D56C}"),CS_DBLCLKS,COLOR_WINDOW);
	
	void CreateInDialog( CWindow wndDialog, UINT replaceControlID );

	enum {
		MSG_SIZE_ASYNC = WM_USER + 13,
		MSG_EXEC_DEFERRED,
		UserMsgBase
	};

	BEGIN_MSG_MAP_EX(CListControlImpl)
		MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, MousePassThru);
		MESSAGE_HANDLER_EX(MSG_EXEC_DEFERRED, OnExecDeferred);
		MESSAGE_HANDLER(WM_PAINT,OnPaint);
		MSG_WM_PRINTCLIENT(OnPrintClient);
		MESSAGE_HANDLER(WM_VSCROLL,OnVScroll);
		MESSAGE_HANDLER(WM_HSCROLL,OnHScroll);
		MESSAGE_HANDLER(WM_SIZE,OnSize);
		MESSAGE_HANDLER(WM_MOUSEHWHEEL,OnHWheel);
		MESSAGE_HANDLER(WM_MOUSEWHEEL,OnVWheel);
		MESSAGE_HANDLER(WM_LBUTTONDOWN,SetFocusPassThru);
		MESSAGE_HANDLER(WM_RBUTTONDOWN,SetFocusPassThru);
		MESSAGE_HANDLER(WM_MBUTTONDOWN,SetFocusPassThru);
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK,SetFocusPassThru);
		MESSAGE_HANDLER(WM_RBUTTONDBLCLK,SetFocusPassThru);
		MESSAGE_HANDLER(WM_MBUTTONDBLCLK,SetFocusPassThru);
		MESSAGE_HANDLER(WM_CREATE,OnCreatePassThru);
		MESSAGE_HANDLER(WM_ERASEBKGND,OnEraseBkgnd);
		MESSAGE_HANDLER(MSG_SIZE_ASYNC,OnSizeAsync);
		MESSAGE_HANDLER(WM_GESTURE, OnGesture)
		MSG_WM_THEMECHANGED(OnThemeChanged)
		MSG_WM_KILLFOCUS(OnKillFocus)
		MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
	END_MSG_MAP()

	virtual void ReloadData() { OnViewAreaChanged(); }
	virtual void ReloadItems( pfc::bit_array const & mask ) { UpdateItems( mask); }

	//! Hookable function called in response to reordering of items. Redraws the view and updates internal data to reflect the change.
	virtual void OnItemsReordered( const size_t * order, size_t count );
	//! Hookable function called in response to removal of items. Redraws the view and updates internal data to reflect the change.
	virtual void OnItemsRemoved( pfc::bit_array const & mask, size_t oldCount ) { ReloadData(); }
	//! Hookable function called in response to insertion of items. Redraws the view and updates internal data to reflect the change.
	virtual void OnItemsInserted( size_t at, size_t count, bool bSelect ) { ReloadData(); }

	void ReloadItem(size_t i) { ReloadItems( pfc::bit_array_one(i) ); }
	void OnViewAreaChanged() {OnViewAreaChanged(GetViewOrigin());}
	void OnViewAreaChanged(CPoint p_originOverride);
	void UpdateGroupHeader(int p_id);
	void UpdateItems(const pfc::bit_array & p_mask);
	void UpdateItemsAndHeaders(const pfc::bit_array & p_mask);
	void UpdateItem(t_size p_item) {UpdateItems(pfc::bit_array_one(p_item));}
	void UpdateItemsAll() {Invalidate();}
	void EnsureItemVisible(t_size p_item, bool bUser = false);
	void EnsureHeaderVisible(int p_group);
	virtual void EnsureVisibleRectAbs(const CRect & p_rect);
	CRect GetItemRect(t_size p_item) const;
	bool GetGroupHeaderRect(int p_group,CRect & p_rect) const;
	CRect GetItemRectAbs(t_size p_item) const;
	bool GetGroupHeaderRectAbs(int p_group,CRect & p_rect) const;
	CPoint GetViewOrigin() const {return m_viewOrigin;}
	CPoint GetViewOffset() const {return GetViewOrigin() - GetClientOrigin();}
	int GetViewAreaWidth() const {return GetItemWidth();}
	int GetViewAreaHeight() const;
	CRect GetViewAreaRectAbs() const;
	CRect GetViewAreaRect() const;
	CRect GetValidViewOriginArea() const;
	t_size GetGroupCount() const;
	bool GetItemRangeAbs(const CRect & p_rect,t_size & p_base,t_size & p_count) const;
	bool GetItemRangeAbsInclHeaders(const CRect & p_rect,t_size & p_base,t_size & p_count) const;
	bool GetItemRange(const CRect & p_rect,t_size & p_base,t_size & p_count) const;
	void MoveViewOriginNoClip(CPoint p_target);
	void MoveViewOrigin(CPoint p_target);
	CPoint ClipViewOrigin(CPoint p_origin) const;
	void MoveViewOriginDelta(CPoint p_delta) {MoveViewOrigin( GetViewOrigin() + p_delta );}
	void MoveViewOriginDeltaNoClip(CPoint p_delta) {MoveViewOriginNoClip( GetViewOrigin() + p_delta );}
	bool ItemFromPoint(CPoint const & p_pt,t_size & p_item) const {return ItemFromPointAbs(p_pt + GetViewOffset(),p_item);}
	bool GroupHeaderFromPoint(CPoint const & p_pt,int & p_group) const {return GroupHeaderFromPointAbs(p_pt + GetViewOffset(),p_group);}
	bool ItemFromPointAbs(CPoint const & p_pt,t_size & p_item) const;
	bool GroupHeaderFromPointAbs(CPoint const & p_pt,int & p_group) const;

	bool ResolveGroupRange(int p_id,t_size & p_base,t_size & p_count) const;

	virtual int GetGroupHeaderHeight() const {return 0;}
	virtual int GetItemHeight() const {return 0;}
	virtual int GetItemWidth() const {return 0;}
	virtual t_size GetItemCount() const {return 0;}
	virtual int GetItemGroup(t_size p_item) const {return 0;}
	//override optionally
	virtual void RenderItem(t_size p_item,const CRect & p_itemRect,const CRect & p_updateRect,CDCHandle p_dc);
	//override optionally
	virtual void RenderGroupHeader(int p_group,const CRect & p_headerRect,const CRect & p_updateRect,CDCHandle p_dc);

	//called by default RenderItem implementation
	virtual void RenderItemText(t_size p_item,const CRect & p_itemRect,const CRect & p_updateRect,CDCHandle p_dc, bool allowColors) {}
	//called by default RenderItem implementation
	virtual void RenderGroupHeaderText(int p_group,const CRect & p_headerRect,const CRect & p_updateRect,CDCHandle p_dc) {}

	virtual void OnViewOriginChange(CPoint p_delta) {}
	virtual void RenderOverlay(const CRect & p_updaterect,CDCHandle p_dc) {}
	virtual bool FixedOverlayPresent() {return false;}

	virtual CRect GetClientRectHook() const {CRect temp; if (!GetClientRect(temp)) temp.SetRectEmpty(); return temp;}

	enum {
		colorText = COLOR_WINDOWTEXT,
		colorBackground = COLOR_WINDOW,
		colorHighlight = COLOR_HOTLIGHT,
		colorSelection = COLOR_HIGHLIGHT,
	};

	virtual COLORREF GetSysColorHook( int colorIndex ) const;
	
	//! Called by CListControlWithSelectionBase.
	virtual void OnItemClicked(t_size item, CPoint pt) {}
	//! Called by CListControlWithSelectionBase.
	virtual void OnGroupHeaderClicked(int groupId, CPoint pt) {}

	//! Return true to indicate that some area of the control has a special purpose and clicks there should not trigger changes in focus/selection.
	virtual bool OnClickedSpecialHitTest(CPoint pt) { return false; }
	virtual bool OnClickedSpecial(DWORD status, CPoint pt) {return false;}

	virtual bool AllowScrollbar(bool vertical) const {return true;}

	CPoint GetClientOrigin() const {return GetClientRectHook().TopLeft();}
	CRect GetVisibleRectAbs() const {
		CRect view = GetClientRectHook();
		view.OffsetRect( GetViewOrigin() - view.TopLeft() );
		return view;
	}

	bool IsSameItemOrHeaderAbs(const CPoint & p_point1, const CPoint & p_point2) const;

	void AddItemToUpdateRgn(HRGN p_rgn, t_size p_index) const;
	void AddGroupHeaderToUpdateRgn(HRGN p_rgn, int id) const;

	t_size InsertIndexFromPoint(const CPoint & p_pt) const;
	//! Translate point to insert location for drag and drop. \n
	//! Made virtual so it can be specialized to allow only specific drop locations.
	virtual t_size InsertIndexFromPointEx(const CPoint & pt, bool & bInside) const;

	virtual void ListHandleResize();
	
	//! Can smooth-scroll *now* ? Used to suppress smooth scroll on temporary basis due to specific user operations in progress
	virtual bool CanSmoothScroll() const { return true; }
	//! Is smooth scroll enabled by user?
	virtual bool UserEnabledSmoothScroll() const;
	virtual bool ToggleSelectedItemsHook(pfc::bit_array const & mask) { return false; }

	void SetCaptureEx(CaptureProc_t proc);
	void SetCaptureMsgHandled(BOOL v) { this->SetMsgHandled(v); }

	SIZE GetDPI() const { return this->m_dpi;}
private:
	void RenderRect(const CRect & p_rect,CDCHandle p_dc);
	int HandleWheel(int & p_accum,int p_delta, bool bHoriz);

	void OnKillFocus(CWindow);
	void OnWindowPosChanged(LPWINDOWPOS);
	void PaintContent(CRect rcPaint, HDC dc);
	void OnPrintClient(HDC dc, UINT uFlags);
	LRESULT OnPaint(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnVScroll(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnHScroll(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnSize(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnSizeAsync(UINT,WPARAM,LPARAM,BOOL&) {ListHandleResize();return 0;}
	LRESULT OnVWheel(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnHWheel(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnGesture(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT SetFocusPassThru(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnCreatePassThru(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnEraseBkgnd(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT MousePassThru(UINT, WPARAM, LPARAM);

	void OnThemeChanged();
	int GetScrollThumbPos(int which);
	void RefreshSliders();
	void RefreshSlider(bool p_vertical);

	void OnSizeAsync_Trigger();
	static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
	bool MouseWheelFromHook(UINT msg, LPARAM data);

	bool m_suppressMouseWheel = false;
	int m_wheelAccumX, m_wheelAccumY;
	CPoint m_viewOrigin;
	bool m_sizeAsyncPending;
	CPoint m_gesturePoint;
protected:
	pfc::map_t<pfc::string8, CTheme, pfc::comparator_strcmp> m_themeCache;
	CTheme & themeFor( const char * what );
	CTheme & theme() { return themeFor("LISTVIEW");}

	const SIZE m_dpi;
	CGestureAPI m_gestureAPI;
	bool m_ensureVisibleUser;
	CaptureProc_t m_captureProc;
	void defer( std::function<void () > f );
	LRESULT OnExecDeferred(UINT, WPARAM, LPARAM);


	// Overlays our stuff on top of generic DoDragDrop call.
	// Currently catches mouse wheel messages in mid-drag&drop and handles them in our view.
	HRESULT DoDragDrop(LPDATAOBJECT pDataObj, LPDROPSOURCE pDropSource, DWORD dwOKEffects, LPDWORD pdwEffect);

private:
	bool m_defferredMsgPending = false;
	std::list<std::function<void ()> > m_deferred;
};

class CListControlFontOps : public CListControlImpl {
private:
	typedef CListControlImpl TParent;
public:
	CListControlFontOps();
	BEGIN_MSG_MAP_EX(CListControlFontOps)
		MESSAGE_HANDLER(WM_GETFONT,OnGetFont);
		MESSAGE_HANDLER(WM_SETFONT,OnSetFont);
		CHAIN_MSG_MAP(TParent);
	END_MSG_MAP()
	CFontHandle GetFont() const { return m_font; }
	void SetFont(HFONT font, bool bUpdateView = true);
protected:
	CFontHandle GetGroupHeaderFont() const {return (HFONT)m_groupHeaderFont;}
	virtual double GroupHeaderFontScale() const { return 1.25; }
	virtual int GroupHeaderFontWeight(int origVal) const {
		//return pfc::min_t<int>(FW_BLACK, origVal + 200);
		return origVal; 
	}

	//! Overridden implementations should always forward the call to the base class.
	virtual void OnSetFont(bool bUpdatingView) {}

	int GetGroupHeaderHeight() const {return m_groupHeaderHeight;}
	int GetItemHeight() const {return m_itemHeight;}
	
private:
	LRESULT OnSetFont(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnGetFont(UINT,WPARAM,LPARAM,BOOL&);
	void UpdateGroupHeaderFont();
	void CalculateHeights();
	int m_itemHeight, m_groupHeaderHeight;
	CFontHandle m_font;
	CFont m_groupHeaderFont;
};

class CListControlHeaderImpl : public CListControlFontOps {
private:
	typedef CListControlFontOps TParent;
public:
	CListControlHeaderImpl() {}


	BEGIN_MSG_MAP_EX(CListControlHeaderImpl)
		MESSAGE_HANDLER(WM_KEYDOWN,OnKeyDown);
		MESSAGE_HANDLER(WM_SYSKEYDOWN,OnKeyDown);
		MESSAGE_HANDLER_EX(WM_SIZE,OnSizePassThru);
		NOTIFY_CODE_HANDLER(HDN_ITEMCHANGED,OnHeaderItemChanged);
		NOTIFY_CODE_HANDLER(HDN_ENDDRAG,OnHeaderEndDrag);
		NOTIFY_CODE_HANDLER(HDN_ITEMCLICK,OnHeaderItemClick);
		NOTIFY_CODE_HANDLER(HDN_DIVIDERDBLCLICK,OnDividerDoubleClick);
		MSG_WM_SETCURSOR(OnSetCursor);
		MSG_WM_MOUSEMOVE(OnMouseMove)
		MSG_WM_DESTROY(OnDestroy)
		CHAIN_MSG_MAP(TParent)
	END_MSG_MAP()

	CRect GetClientRectHook() const;

	enum cellType_t {
		cell_none = 0,
		cell_text,
		cell_multitext,
		cell_hyperlink,
		cell_button,
		cell_button_lite,
		cell_button_glyph,
		cell_button_total,
		cell_checkbox,
		cell_radiocheckbox,
		cell_checkbox_total,
	};

	typedef uint32_t cellState_t;
	enum {
		cellState_none = 0,
		cellState_hot = 1 << 0,
		cellState_pressed = 1 << 1,
	};

	void InitializeHeaderCtrl(DWORD flags = HDS_FULLDRAG);
	void InitializeHeaderCtrlSortable() {InitializeHeaderCtrl(HDS_FULLDRAG | HDS_BUTTONS);}
	CHeaderCtrl GetHeaderCtrl() const {return m_header;}
	void SetSortIndicator( size_t whichColumn, bool isUp );
	void ClearSortIndicator();
protected:
	struct GetOptimalWidth_Cache {
		//! For temporary use.
		pfc::string8_fastalloc m_stringTemp, m_stringTempUnfuckAmpersands;
		//! For temporary use.
		pfc::stringcvt::string_wide_from_utf8_t<pfc::alloc_fast_aggressive> m_convertTemp;
		//! Our DC for measuring text. Correct font pre-selected.
		CDCHandle m_dc;

		t_uint32 GetStringTempWidth();
	};

	void UpdateHeaderLayout();
	void OnViewOriginChange(CPoint p_delta);
	void SetHeaderFont(HFONT font);
	void RenderItemText(t_size item,const CRect & itemRect,const CRect & updateRect,CDCHandle dc, bool allowColors);
	void RenderGroupHeaderText(int id,const CRect & headerRect,const CRect & updateRect,CDCHandle dc);

	// If creating a custom headerless multi column scheme, override these to manipulate your columns
	virtual size_t GetColumnCount() const;
	virtual uint32_t GetSubItemWidth(size_t subItem) const;
	//! Indicate how many columns a specific row/column cell spans\n
	//! This makes sense only if the columns can't be user-reordered
	virtual size_t GetSubItemSpan(size_t row, size_t column) const;

	t_size GetSubItemOrder(t_size subItem) const;
	int GetItemWidth() const override;
	bool IsHeaderEnabled() const {return m_header.m_hWnd != NULL;}
	void ResetColumns(bool update = true);
	void AddColumn(const char * label, t_uint32 widthPixels, DWORD fmtFlags = HDF_LEFT,bool update = true);
	void AddColumnAutoWidth( const char * label, DWORD fmtFlags = HDF_LEFT, bool bUpdate = true) { AddColumn(label, UINT32_MAX, fmtFlags, bUpdate); }
	bool DeleteColumn(size_t index, bool updateView = true);
	void DeleteColumns( pfc::bit_array const & mask, bool updateView = true);
	void ResizeColumn(t_size index, t_uint32 widthPixels, bool updateView = true);
	void SetColumn( size_t which, const char * title, DWORD fmtFlags = HDF_LEFT, bool updateView = true);
	//! Converts an item/subitem rect to a rect in which the text should be rendered, removing spacing to the left/right of the text.
	CRect GetItemTextRect(CRect const & itemRect);
	//! Override for custom spacing to the left/right of the text in each column.
	virtual t_uint32 GetColumnSpacing() const {return MulDiv(4,m_dpi.cx,96);}
	//! Override for column-header-click sorting.
	virtual void OnColumnHeaderClick(t_size index) {}
	//! Override to supply item labels.
	virtual bool GetSubItemText(t_size item, t_size subItem, pfc::string_base & out) const {return false;}
	//! Override if you support groups.
	virtual bool GetGroupHeaderText(int id, pfc::string_base & out) const {return false;}
	//! Override optionally.
	virtual void RenderSubItemText(t_size item, t_size subItem,const CRect & subItemRect,const CRect & updateRect,CDCHandle dc, bool allowColors);

	virtual void OnColumnsChanged() {OnViewAreaChanged();}

	virtual t_uint32 GetOptimalSubItemWidth(t_size item, t_size subItem, GetOptimalWidth_Cache & cache) const;
	uint32_t GetOptimalColumnWidth( size_t index ) const;
	uint32_t GetOptimalColumnWidthFixed( const char * fixedText) const;

	virtual t_uint32 GetOptimalGroupHeaderWidth(int which) const;

	
	bool GetItemAtPointAbsEx( CPoint pt, size_t & outItem, size_t & outSubItem ) const;
	cellType_t GetCellTypeAtPointAbs( CPoint pt ) const;
	virtual cellType_t GetCellType( size_t item, size_t subItem ) const { return cell_text; }
	virtual bool AllowTypeFindInCell( size_t item, size_t subItem ) const;
	virtual bool GetCellTypeSupported() const { return false; } // optimization hint, some expensive checks can be suppressed if cell types are not used for this view
	virtual bool GetCellCheckState( size_t item, size_t subItem ) const { return false; }
	virtual void SetCellCheckState( size_t item, size_t subItem, bool value ) {}
	virtual bool ToggleSelectedItemsHook(const pfc::bit_array & mask);

	void RenderSubItemTextInternal(t_size subItem, const CRect & subItemRect, CDCHandle dc, const char * text, bool allowColors);
	void RenderSubItemTextInternal2( DWORD format, cellType_t cellType, cellState_t state, const CRect & subItemRect, CDCHandle dc, const char * text, bool allowColors, double textScale, CRect rcHot );

	t_uint32 GetOptimalColumnWidth(t_size which, GetOptimalWidth_Cache & cache) const;
	t_uint32 GetOptimalSubItemWidthSimple(t_size item, t_size subItem) const;
	
	void AutoColumnWidths(const pfc::bit_array & mask,bool expandLast = false);
	void AutoColumnWidths() {AutoColumnWidths(pfc::bit_array_true());}
	void AutoColumnWidth(t_size which) {AutoColumnWidths(pfc::bit_array_one(which));}

	virtual bool OnColumnHeaderDrag(t_size index, t_size newOrder);

	void OnItemClicked(t_size item, CPoint pt);
	virtual void OnSubItemClicked(t_size item, t_size subItem,CPoint pt);
	virtual bool OnClickedSpecialHitTest(CPoint pt);
	virtual bool OnClickedSpecial(DWORD status, CPoint pt);

	CRect GetSubItemRectAbs(t_size item,t_size subItem) const;
	CRect GetSubItemRect(t_size item,t_size subItem) const;

	t_size SubItemFromPointAbs(CPoint pt) const;

	static bool CellTypeReactsToMouseOver( cellType_t ct );
	virtual CRect CellHotRect( size_t item, size_t subItem, cellType_t ct, CRect rcCell );
	CRect CellHotRect( size_t item, size_t subItem, cellType_t ct );
	virtual double CellTextScale(size_t item, size_t subItem) { return 1; }

	// HDF_* constants for this column, override when not using list header control. Used to control text alignment.
	virtual DWORD GetColumnFormat(t_size which) const;
	void SetColumnFormat(t_size which,DWORD format);
	void SetColumnSort(t_size which, bool isUp);

	std::vector<int> GetColumnOrderArray() const;

	bool AllowScrollbar(bool vertical) const override;
private:

	void ProcessColumnsChange() {RecalcItemWidth();OnColumnsChanged();}
	LRESULT OnSizePassThru(UINT,WPARAM,LPARAM);
	LRESULT OnHeaderItemClick(int,LPNMHDR,BOOL&);
	LRESULT OnDividerDoubleClick(int,LPNMHDR,BOOL&);
	LRESULT OnHeaderItemChanged(int,LPNMHDR,BOOL&);
	LRESULT OnHeaderEndDrag(int,LPNMHDR,BOOL&);
	LRESULT OnKeyDown(UINT,WPARAM,LPARAM,BOOL&);
	void OnDestroy();
	BOOL OnSetCursor(CWindow wnd, UINT nHitTest, UINT message);
	void OnMouseMove(UINT nFlags, CPoint point);

	void RecalcItemWidth(); // FIXED width math
	void ProcessAutoWidth(); // DYNAMIC width math
	void ColumnWidthFix(); // Call either of the above after columns have been changed

	int m_itemWidth = 0;
	int m_clientWidth = 0;
	CHeaderCtrl m_header;
	std::set<int> m_autoWidthColumns;

	//for group headers
	GdiplusScope m_gdiPlusScope;

	void SetPressedItem(size_t row, size_t column);
	void ClearPressedItem() {SetPressedItem(SIZE_MAX, SIZE_MAX);}
	void SetHotItem( size_t row, size_t column );
	void ClearHotItem() { SetHotItem(SIZE_MAX, SIZE_MAX); }

	size_t m_pressedItem = SIZE_MAX, m_pressedSubItem = SIZE_MAX;
	size_t m_hotItem = SIZE_MAX, m_hotSubItem = SIZE_MAX;
};

class CListControlTruncationTooltipImpl : public CListControlHeaderImpl {
private:
	typedef CListControlHeaderImpl TParent;
public:
	CListControlTruncationTooltipImpl();

	BEGIN_MSG_MAP_EX(CListControlTruncationTooltipImpl)
		MESSAGE_HANDLER(WM_MOUSEHOVER,OnHover);
		MESSAGE_HANDLER(WM_MOUSEMOVE,OnMouseMovePassThru);
		MESSAGE_HANDLER(WM_TIMER,OnTimer);
		MESSAGE_HANDLER(WM_DESTROY,OnDestroyPassThru);
		CHAIN_MSG_MAP(TParent)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO,OnTTGetDispInfo);
		NOTIFY_CODE_HANDLER(TTN_POP,OnTTPop);
		NOTIFY_CODE_HANDLER(TTN_SHOW,OnTTShow);
	END_MSG_MAP()

	void OnViewOriginChange(CPoint p_delta) {TParent::OnViewOriginChange(p_delta);TooltipRemove();}
protected:
	virtual bool GetTooltipData( CPoint ptAbs, pfc::string_base & text, CRect & rc, CFontHandle & font) const;
private:
	enum {
		KTooltipTimer = 0x51dbee9e,
		KTooltipTimerDelay = 50,
	};
	LRESULT OnHover(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnMouseMovePassThru(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnTimer(UINT,WPARAM,LPARAM,BOOL&);
	LRESULT OnTTGetDispInfo(int,LPNMHDR,BOOL&);
	LRESULT OnTTShow(int,LPNMHDR,BOOL&);
	LRESULT OnTTPop(int,LPNMHDR,BOOL&);
	LRESULT OnDestroyPassThru(UINT,WPARAM,LPARAM,BOOL&);

	void InitTooltip();
	void TooltipActivateAbs(const char * label, const CRect & rect);
	void TooltipActivate(const char * label, const CRect & rect);
	void TooltipRemoveCheck(LPARAM pos);
	void TooltipRemoveCheck();
	void TooltipRemove();
	void TooltipUpdateFont();
	void OnSetFont(bool) {TooltipUpdateFont();}
	bool IsRectFullyVisibleAbs(CRect const & r);
	bool IsRectPartiallyObscuredAbs(CRect const & r) const;
	CRect m_tooltipRect;
	CToolTipCtrl m_tooltip;
	TOOLINFO m_toolinfo;
	pfc::stringcvt::string_os_from_utf8 m_tooltipText;
	CFontHandle m_tooltipFont;
};

typedef CMiddleDragImpl<CListControlTruncationTooltipImpl> CListControl;


#if 0
//! A class that just declares commonly overridden methods for copy pasting into new declarations
class CListControlCopyPasteFodder : public CListControl {
public:
	virtual int GetItemHeight() const;
	virtual int GetItemWidth() const;
	virtual t_size GetItemCount() const;
	virtual void OnSubItemClicked(t_size item, t_size subItem, CPoint pt);
	virtual void OnItemClicked(t_size item, CPoint pt);
	virtual bool AllowScrollbar(bool vertical) const;

	virtual void RequestReorder(size_t const * order, size_t count) ;
	virtual void RequestRemoveSelection();
	virtual void ExecuteDefaultAction(t_size index) ;

	// OPTIONAL, column manipulation, count/width/span are irrelevant if you add columns to the header on startup
	virtual t_size GetColumnCount() const;
	virtual t_uint32 GetSubItemWidth(t_size subItem) const;
	virtual size_t GetSubItemSpan(size_t row, size_t column) const;
	virtual bool GetSubItemText(t_size item, t_size subItem, pfc::string_base & out) const;

	virtual cellType_t GetCellType(size_t item, size_t subItem) const;
	virtual bool GetCellTypeSupported() const;
	virtual bool GetCellCheckState(size_t item, size_t subItem) const;
	virtual void SetCellCheckState(size_t item, size_t subItem, bool value);
	virtual bool ToggleSelectedItemsHook(const pfc::bit_array & mask);

	// OPTIONAL, drag&drop
	virtual uint32_t QueryDragDropTypes() const;
	virtual DWORD DragDropAccept(IDataObject * obj, bool & showDropMark);
	virtual pfc::com_ptr_t<IDataObject> MakeDataObject();
	virtual void OnDrop(IDataObject * obj, CPoint pt);
	virtual DWORD DragDropSourceEffects();
	virtual void DragDropSourceSucceeded(DWORD effect);
};
#endif
