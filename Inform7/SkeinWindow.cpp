#include "stdafx.h"
#include "SkeinWindow.h"
#include "Messages.h"
#include "Inform.h"
#include "Dialogs.h"

#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(SkeinWindow, CScrollView)

BEGIN_MESSAGE_MAP(SkeinWindow, CScrollView)
  ON_WM_CREATE()
  ON_WM_SIZE()
  ON_WM_VSCROLL()
  ON_WM_HSCROLL()
  ON_WM_ERASEBKGND()
  ON_WM_LBUTTONUP()
  ON_WM_CONTEXTMENU()
  ON_WM_MOUSEACTIVATE()
  ON_WM_MOUSEMOVE()

  ON_MESSAGE(WM_RENAMENODE, OnRenameNode)
  ON_MESSAGE(WM_LABELNODE, OnLabelNode)
END_MESSAGE_MAP()

SkeinWindow::SkeinWindow() : m_skein(NULL), m_mouseOverNode(NULL), m_mouseOverMenu(false)
{
}

int SkeinWindow::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  if (CScrollView::OnCreate(lpCreateStruct) == -1)
    return -1;

  // Control for editing text
  if (m_edit.Create(ES_CENTER|ES_AUTOHSCROLL|WS_CHILD,this,0) == FALSE)
    return -1;

  SetFontsBitmaps();
  return 0;
}

void SkeinWindow::OnSize(UINT nType, int cx, int cy)
{
  CScrollView::OnSize(nType,cx,cy);

  if (m_skein != NULL)
    Layout(false);
}

void SkeinWindow::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
  if (pScrollBar != NULL && pScrollBar->SendChildNotifyLastMsg())
    return;
  if (pScrollBar == NULL)
  {
    if ((nSBCode == SB_THUMBPOSITION) || (nSBCode == SB_THUMBTRACK))
    {
      SCROLLINFO scroll;
      ::ZeroMemory(&scroll,sizeof scroll);
      scroll.cbSize = sizeof scroll;
      GetScrollInfo(SB_VERT,&scroll);
      nPos = scroll.nTrackPos;
    }
    OnScroll(MAKEWORD(0xFF,nSBCode),nPos);
  }
}

void SkeinWindow::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
  if (pScrollBar != NULL && pScrollBar->SendChildNotifyLastMsg())
    return;
  if (pScrollBar == NULL)
  {
    if ((nSBCode == SB_THUMBPOSITION) || (nSBCode == SB_THUMBTRACK))
    {
      SCROLLINFO scroll;
      ::ZeroMemory(&scroll,sizeof scroll);
      scroll.cbSize = sizeof scroll;
      GetScrollInfo(SB_HORZ,&scroll);
      nPos = scroll.nTrackPos;
    }
    OnScroll(MAKEWORD(nSBCode,0xFF),nPos);
  }
}

BOOL SkeinWindow::OnEraseBkgnd(CDC* pDC)
{
  return TRUE;
}

void SkeinWindow::OnLButtonUp(UINT nFlags, CPoint point)
{
  Skein::Node* node = NodeAtPoint(point);
  if (node != NULL)
  {
    // Is the user clicking on the "differs" badge?
    if ((node->GetDiffers() != Skein::Node::ExpectedSame) && (node->GetExpectedText().IsEmpty() == FALSE))
    {
      if (GetBadgeRect(m_nodes[node]).PtInRect(point))
      {
        GetParentFrame()->SendMessage(WM_SHOWTRANSCRIPT,(WPARAM)node,(LPARAM)GetSafeHwnd());
        CScrollView::OnLButtonUp(nFlags,point);
        return;
      }
    }

    // Is the user clicking on the context menu button?
    if (GetMenuButtonRect(m_nodes[node]).PtInRect(point))
    {
      CPoint sp(point);
      ClientToScreen(&sp);
      OnContextMenu(this,sp);
      CScrollView::OnLButtonUp(nFlags,point);
      return;
    }

    // Just select the node
    GetParentFrame()->SendMessage(WM_SELECTNODE,(WPARAM)node);
  }

  CScrollView::OnLButtonUp(nFlags,point);
}

void SkeinWindow::OnContextMenu(CWnd* pWnd, CPoint point)
{
  // No menu if currently editing
  if (m_edit.IsWindowVisible())
    return;

  // No menu if running the game
  bool gameRunning = GetParentFrame()->SendMessage(WM_GAMERUNNING) != 0;
  bool gameWaiting = GetParentFrame()->SendMessage(WM_GAMEWAITING) != 0;
  if (gameRunning && !gameWaiting)
    return;

  // Find the node under the mouse, if any
  CPoint cp(point);
  ScreenToClient(&cp);
  Skein::Node* node = NodeAtPoint(cp);
  if (node == NULL)
    return;

  // Get the context menu
  CMenu popup;
  popup.LoadMenu(IDR_SKEIN);
  CMenu* menu = popup.GetSubMenu(0);

  // Update the state of the context menu items
  if (node->GetParent() == NULL)
  {
    menu->RemoveMenu(ID_SKEIN_INSERT_PREVIOUS,MF_BYCOMMAND);
    menu->RemoveMenu(ID_SKEIN_DELETE,MF_BYCOMMAND);
    menu->RemoveMenu(ID_SKEIN_EDIT,MF_BYCOMMAND);
    menu->RemoveMenu(ID_SKEIN_ADD_LABEL,MF_BYCOMMAND);
    menu->RemoveMenu(ID_SKEIN_EDIT_LABEL,MF_BYCOMMAND);
    menu->RemoveMenu(ID_SKEIN_DELETE_ALL,MF_BYCOMMAND);
  }
  if (node->GetLabel().IsEmpty())
    menu->RemoveMenu(ID_SKEIN_EDIT_LABEL,MF_BYCOMMAND);
  else
    menu->RemoveMenu(ID_SKEIN_ADD_LABEL,MF_BYCOMMAND);
  if (node->GetNumChildren() > 1)
    menu->RemoveMenu(ID_SKEIN_INSERT_NEXT,MF_BYCOMMAND);
  RemoveExcessSeparators(menu);
  if (gameRunning && m_skein->InCurrentThread(node))
  {
    menu->EnableMenuItem(ID_SKEIN_DELETE,MF_BYCOMMAND|MF_GRAYED);
    menu->EnableMenuItem(ID_SKEIN_DELETE_ALL,MF_BYCOMMAND|MF_GRAYED);
  }

  // Show the context menu
  int cmd = menu->TrackPopupMenuEx(TPM_LEFTALIGN|TPM_TOPALIGN|TPM_NONOTIFY|TPM_RETURNCMD,
    point.x,point.y,GetParentFrame(),NULL);

  // Act on the context menu choice
  switch (cmd)
  {
  case ID_SKEIN_PLAY:
    GetParentFrame()->SendMessage(WM_PLAYSKEIN,(WPARAM)node);
    break;
  case ID_SKEIN_EDIT:
    StartEdit(node,false);
    break;
  case ID_SKEIN_ADD_LABEL:
  case ID_SKEIN_EDIT_LABEL:
    Invalidate();
    StartEdit(node,true);
    break;
  case ID_SKEIN_INSERT_PREVIOUS:
    {
      Skein::Node* newNode = m_skein->AddNewParent(node);

      // Force a repaint so that the new node is drawn and recorded
      Invalidate();
      UpdateWindow();
      StartEdit(newNode,false);
    }
    break;
  case ID_SKEIN_INSERT_NEXT:
    {
      Skein::Node* newNode = NULL;
      switch (node->GetNumChildren())
      {
      case 0:
        newNode = m_skein->AddNew(node);
        break;
      case 1:
        newNode = m_skein->AddNewParent(node->GetChild(0));
        break;
      }
      if (newNode != NULL)
      {
        Invalidate();
        UpdateWindow();
        StartEdit(newNode,false);
      }
    }
    break;
  case ID_SKEIN_SPLIT_THREAD:
    {
      Skein::Node* newNode = m_skein->AddNew(node);
      Invalidate();
      UpdateWindow();
      StartEdit(newNode,false);
    }
    break;
  case ID_SKEIN_DELETE:
    m_skein->RemoveSingle(node);
    break;
  case ID_SKEIN_DELETE_ALL:
    m_skein->RemoveAll(node);
    break;
  case ID_SKEIN_BLESS:
    m_skein->Bless(node,false);
    break;
  case ID_SKEIN_BLESS_ALL:
    m_skein->Bless(node,true);
    break;
  case 0:
    // Make sure this window still has the focus
    SetFocus();
    break;
  }
}

int SkeinWindow::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
  // If the edit window is visible and the user clicks outside it, end editing
  if (m_edit.IsWindowVisible())
  {
    CPoint point(::GetMessagePos());
    ScreenToClient(&point);

    if (ChildWindowFromPoint(point) != &m_edit)
      m_edit.EndEdit();
  }
  return CScrollView::OnMouseActivate(pDesktopWnd,nHitTest,message);
}

void SkeinWindow::OnMouseMove(UINT nFlags, CPoint point)
{
  Skein::Node* mouseOverNode = NodeAtPoint(point);
  bool mouseOverMenu = false;
  if (mouseOverNode)
  {
    if (GetMenuButtonRect(m_nodes[mouseOverNode]).PtInRect(point))
      mouseOverMenu = true;
  }

  if ((m_mouseOverNode != mouseOverNode) || (m_mouseOverMenu != mouseOverMenu))
  {
    m_mouseOverNode = mouseOverNode;
    m_mouseOverMenu = mouseOverMenu;
    Invalidate();
  }

  CScrollView::OnMouseMove(nFlags,point);
}

LRESULT SkeinWindow::OnRenameNode(WPARAM node, LPARAM line)
{
  Skein::Node* theNode = (Skein::Node*)node;
  if (m_skein->IsValidNode(theNode) == false)
    return 0;

  m_skein->SetLine(theNode,(LPWSTR)line);
  return 0;
}

LRESULT SkeinWindow::OnLabelNode(WPARAM node, LPARAM line)
{
  Skein::Node* theNode = (Skein::Node*)node;
  if (m_skein->IsValidNode(theNode) == false)
    return 0;

  m_skein->SetLabel(theNode,(LPWSTR)line);
  return 0;
}

void SkeinWindow::OnDraw(CDC* pDC)
{
  // Clear out any previous node positions
  m_nodes.clear();

  // Get the dimensions of the window
  CRect client;
  GetClientRect(client);

  // Create a memory device context
  CDC dc;
  dc.CreateCompatibleDC(pDC);

  // Create a memory bitmap
  CDibSection bitmap;
  if (bitmap.CreateBitmap(pDC->GetSafeHdc(),client.Width(),client.Height()) == FALSE)
    return;
  CBitmap* oldBitmap = CDibSection::SelectDibSection(dc,&bitmap);
  CFont* oldFont = dc.SelectObject(theApp.GetFont(InformApp::FontDisplay));
  CPoint origin = pDC->GetViewportOrg();

  // Clear the background
  dc.FillSolidRect(client,theApp.GetColour(InformApp::ColourBack));

  if (m_skein->IsActive())
  {
    // Redo the layout if needed
    m_skein->Layout(dc,m_fontSize.cx*10,false);

    // Work out the position of the centre of the root node
    CPoint rootCentre(origin);
    rootCentre.y += GetNodeYPos(0,1);

    // If there is no horizontal scrollbar, centre the root node
    BOOL horiz, vert;
    CheckScrollBars(horiz,vert);
    if (horiz)
      rootCentre.x += GetTotalSize().cx/2;
    else
      rootCentre.x += client.Width()/2;

    // Get the end node of the selected thread
    Skein::Node* threadEnd = (Skein::Node*)
      GetParentFrame()->SendMessage(WM_TRANSCRIPTEND);

    // Draw all nodes
    for (int i = 0; i < 2; i++)
    {
      DrawNodeTree(i,m_skein->GetRoot(),threadEnd,dc,bitmap,client,
        CPoint(0,0),rootCentre,GetNodeYPos(1,0));
    }

    // If the edit window is visible, exclude the area under it to reduce flicker
    if (m_edit.IsWindowVisible())
    {
      CRect editRect;
      m_edit.GetWindowRect(&editRect);
      ScreenToClient(&editRect);
      editRect -= pDC->GetViewportOrg();
      pDC->ExcludeClipRect(editRect);
    }
  }

  // Draw the memory bitmap on the window's device context
  pDC->BitBlt(-origin.x,-origin.y,client.Width(),client.Height(),&dc,0,0,SRCCOPY);

  // Restore the original device context settings
  dc.SelectObject(oldFont);
  dc.SelectObject(oldBitmap);
}

void SkeinWindow::PostNcDestroy()
{
  // Do nothing
}

void SkeinWindow::SetSkein(Skein* skein)
{
  m_skein = skein;
  m_skein->AddListener(this);
  Layout(false);
}

void SkeinWindow::Layout(bool force)
{
  CRect client;
  GetClientRect(client);
  SetScrollSizes(MM_TEXT,GetLayoutSize(force),client.Size());
}

void SkeinWindow::PrefsChanged(void)
{
  m_boldFont.DeleteObject();
  SetFontsBitmaps();

  Layout(true);
  Invalidate();
}

void SkeinWindow::SkeinChanged(Skein::Change change)
{
  if (GetSafeHwnd() == 0)
    return;

  switch (change)
  {
  case Skein::TreeChanged:
  case Skein::NodeTextChanged:
    Layout(false);
    Invalidate();
    break;
  case Skein::ThreadChanged:
  case Skein::NodeColourChanged:
  case Skein::TranscriptThreadChanged:
    Invalidate();
    break;
  default:
    ASSERT(FALSE);
    break;
  }
}

void SkeinWindow::SkeinEdited(bool edited)
{
  if (GetSafeHwnd() != 0)
    GetParentFrame()->SendMessage(WM_PROJECTEDITED);
}

void SkeinWindow::SkeinShowNode(Skein::Node* node, Skein::Show why)
{
  if (GetSafeHwnd() == 0)
    return;

  switch (why)
  {
  case Skein::JustShow:
  case Skein::JustSelect:
  case Skein::ShowSelect:
  case Skein::ShowNewLine:
    {
      // Work out the position of the node
      int x = (GetTotalSize().cx/2)+node->GetX();
      int y = GetNodeYPos(node->GetDepth()-1,1);
      if (y < 0)
        y = 0;

      // Centre the node horizontally
      CRect client;
      GetClientRect(client);
      x -= client.Width()/2;

      // Only change the co-ordinates if there are scrollbars
      BOOL horiz, vert;
      CheckScrollBars(horiz,vert);
      if (horiz == FALSE)
        x = 0;
      if (vert == FALSE)
        y = 0;

      ScrollToPosition(CPoint(x,y));
    }
    break;
  case Skein::ShowNewTranscript:
    break;
  default:
    ASSERT(FALSE);
    break;
  }
}

CSize SkeinWindow::GetLayoutSize(bool force)
{
  CSize size(0,0);
  if (m_skein->IsActive())
  {
    CDC* dc = AfxGetMainWnd()->GetDC();
    CFont* font = dc->SelectObject(theApp.GetFont(InformApp::FontDisplay));

    // Redo the layout if needed
    m_skein->Layout(*dc,m_fontSize.cx*10,force);

    // The width is the width of all nodes below the root
    size.cx = m_skein->GetRoot()->GetTreeWidth(*dc,m_fontSize.cx*10)+
      (m_fontSize.cx*10);

    // The height comes from the maximum tree depth
    size.cy = GetNodeYPos(m_skein->GetRoot()->GetMaxDepth()-1,2);

    dc->SelectObject(font);
    AfxGetMainWnd()->ReleaseDC(dc);
  }
  return size;
}

int SkeinWindow::GetNodeYPos(int nodes, int ends)
{
  return (int)(m_fontSize.cy*((2.8*nodes)+(2.2*ends)));
}

void SkeinWindow::SetFontsBitmaps(void)
{
  // Set the edit control's font
  CFont* font = theApp.GetFont(InformApp::FontDisplay);
  m_edit.SetFont(font);

  // Create a font
  LOGFONT fontInfo;
  ::ZeroMemory(&fontInfo,sizeof fontInfo);
  font->GetLogFont(&fontInfo);
  fontInfo.lfWeight = FW_BOLD;
  m_boldFont.CreateFontIndirect(&fontInfo);

  // Work out the size of the font
  m_fontSize = theApp.MeasureFont(font);

  // Load bitmaps
  m_bitmaps[BackActive] = GetImage("Skein-active");
  m_bitmaps[BackUnselected] = GetImage("Skein-unselected");
  m_bitmaps[BackSelected] = GetImage("Skein-selected");
  m_bitmaps[MenuActive] = GetImage("Skein-active-menu");
  m_bitmaps[MenuUnselected] = GetImage("Skein-unselected-menu");
  m_bitmaps[MenuSelected] = GetImage("Skein-selected-menu");
  m_bitmaps[MenuOver] = GetImage("Skein-over-menu");
  m_bitmaps[DiffersBadge] = GetImage("SkeinDiffersBadge");
}

void SkeinWindow::DrawNodeTree(int phase, Skein::Node* node, Skein::Node* threadEnd, CDC& dc,
  CDibSection& bitmap, const CRect& client, const CPoint& parentCentre,
  const CPoint& siblingCentre, int spacing)
{
  CPoint nodeCentre(siblingCentre.x+node->GetX(),siblingCentre.y);

  switch (phase)
  {
  case 0:
    // Draw a line connecting the node to its parent
    if (node->GetParent() != NULL)
    {
      DrawNodeLine(dc,bitmap,client,parentCentre,nodeCentre,
        theApp.GetColour(InformApp::ColourSkeinLine));
    }
    break;
  case 1:
    // Draw the node
    DrawNode(node,dc,bitmap,client,nodeCentre,m_skein->InThread(node,threadEnd));
    break;
  }

  // Draw all the node's children
  CPoint childSiblingCentre(siblingCentre.x,siblingCentre.y+spacing);
  for (int i = 0; i < node->GetNumChildren(); i++)
  {
    DrawNodeTree(phase,node->GetChild(i),threadEnd,dc,bitmap,client,
      nodeCentre,childSiblingCentre,spacing);
  }
}

void SkeinWindow::DrawNode(Skein::Node* node, CDC& dc, CDibSection& bitmap, const CRect& client,
  const CPoint& centre, bool selected)
{
  // Store the current device context properties
  UINT align = dc.GetTextAlign();
  int mode = dc.GetBkMode();

  // Set the device context properties
  dc.SetTextAlign(TA_TOP|TA_LEFT);
  dc.SetBkMode(TRANSPARENT);

  // Get the text associated with the node
  LPCWSTR line = node->GetLine();
  LPCWSTR label = node->GetLabel();
  int width = node->GetLineWidth(dc);

  // Check if this node is visible before drawing
  CRect nodeArea(centre,CSize(width+m_fontSize.cx*10,m_fontSize.cy*3));
  nodeArea.OffsetRect(nodeArea.Width()/-2,nodeArea.Height()/-2);
  CRect intersect;
  if (intersect.IntersectRect(client,nodeArea))
  {
    CDibSection* back = m_bitmaps[selected ? BackSelected : BackUnselected];

    bool gameRunning = GetParentFrame()->SendMessage(WM_GAMERUNNING) != 0;
    Skein::Node* playNode = gameRunning ? m_skein->GetPlayed() : NULL;
    while (playNode != NULL)
    {
      if (playNode == node)
      {
        back = m_bitmaps[BackActive];
        break;
      }
      playNode = playNode->GetParent();
    }

    // Write out the node's label, if any
    if (ShowLabel(node))
    {
      SIZE size;
      ::GetTextExtentPoint32W(dc.GetSafeHdc(),label,(UINT)wcslen(label),&size);
      CRect labelArea(centre.x-(size.cx/2)-m_fontSize.cx,centre.y-(int)(1.6*m_fontSize.cy),
        centre.x+(size.cx/2)+m_fontSize.cx,centre.y-(int)(0.6*m_fontSize.cy));
      dc.SetBkColor(theApp.GetColour(InformApp::ColourBack));
      dc.SetTextColor(theApp.GetColour(InformApp::ColourText));
      ::ExtTextOutW(dc.GetSafeHdc(),centre.x-(size.cx/2),labelArea.top,ETO_OPAQUE,
        labelArea,label,(UINT)wcslen(label),NULL);
    }

    // Draw the node's background
    DrawNodeBack(node,bitmap,centre,width,back);

    // Change the font, if needed
    CFont* oldFont = NULL;
    int textWidth = node->GetLineTextWidth();
    if (node == m_skein->GetRoot())
    {
      oldFont = dc.SelectObject(&m_boldFont);
      CSize textSize;
      if (::GetTextExtentPoint32W(dc.GetSafeHdc(),line,(UINT)wcslen(line),&textSize))
        textWidth = textSize.cx;
    }

    // Write out the node's line
    dc.SetTextColor(theApp.GetColour(InformApp::ColourBack));
    ::ExtTextOutW(dc.GetSafeHdc(),centre.x-(textWidth/2),centre.y-(m_fontSize.cy/2),0,
      NULL,line,(UINT)wcslen(line),NULL);

    // Put the old font back
    if (oldFont != NULL)
      dc.SelectObject(oldFont);
  }

  // Reset the device context properties
  dc.SetTextAlign(align);
  dc.SetBkMode(mode);
}

void SkeinWindow::DrawNodeBack(Skein::Node* node, CDibSection& bitmap, const CPoint& centre,
  int width, CDibSection* back)
{
  int y = centre.y-(back->GetSize().cy/2)+(int)(0.12*m_fontSize.cy);
  int edgeWidth = (m_fontSize.cx*7)/2;

  // Draw the rounded edges of the background
  bitmap.AlphaBlend(back,0,0,edgeWidth,back->GetSize().cy,
    centre.x-(width/2)-edgeWidth,y,FALSE);
  bitmap.AlphaBlend(back,back->GetSize().cx-edgeWidth,0,edgeWidth,back->GetSize().cy,
    centre.x+(width/2),y,FALSE);

  // Draw the rest of the background
  {
    int x = centre.x-(width/2);
    while (x < centre.x+(width/2))
    {
      int w = back->GetSize().cx-(2*edgeWidth);
      ASSERT(w > 0);
      if (x+w > centre.x+(width/2))
        w = centre.x+(width/2)-x;
      bitmap.AlphaBlend(back,edgeWidth,0,w,back->GetSize().cy,x,y,FALSE);
      x += w;
    }
  }

  CRect nodeRect(
    CPoint(centre.x-(width/2)-edgeWidth,y),
    CSize(width+(2*edgeWidth),back->GetSize().cy));

  // Draw the "differs badge", if needed
  if ((node->GetDiffers() != Skein::Node::ExpectedSame) && (node->GetExpectedText().IsEmpty() == FALSE))
  {
    CRect badgeRect = GetBadgeRect(nodeRect);
    bitmap.AlphaBlend(m_bitmaps[DiffersBadge],badgeRect.left,badgeRect.top);
  }

  // Draw the context menu button, if needed
  if (node == m_mouseOverNode)
  {
    CDibSection* menu = NULL;
    if (back == m_bitmaps[BackActive])
      menu = m_bitmaps[MenuActive];
    else if (back == m_bitmaps[BackUnselected])
      menu = m_bitmaps[MenuUnselected];
    else if (back == m_bitmaps[BackSelected])
      menu = m_bitmaps[MenuSelected];
    if (menu != NULL)
    {
      CRect menuRect = GetMenuButtonRect(nodeRect,menu);
      bitmap.AlphaBlend(menu,menuRect.left,menuRect.top);
      if (m_mouseOverMenu)
        bitmap.AlphaBlend(m_bitmaps[MenuOver],menuRect.left,menuRect.top);
    }
  }

  // Store the node's size and position
  m_nodes[node] = nodeRect;
}

void SkeinWindow::DrawNodeLine(CDC& dc, CDibSection& bitmap, const CRect& client,
  const CPoint& from, const CPoint& to, COLORREF fore)
{
  int p1x = from.x;
  int p1y = from.y+(int)(0.8*m_fontSize.cy);
  int p2x = to.x;
  int p2y = to.y-(int)(0.7*m_fontSize.cy);

  // Check if we need to draw the line
  CRect lineArea(p1x,p1y,p2x+1,p2y);
  lineArea.NormalizeRect();
  if (lineArea.Width() == 0)
    lineArea.right++;
  if (lineArea.Height() == 0)
    lineArea.bottom++;
  CRect intersect;
  if (intersect.IntersectRect(client,lineArea) == FALSE)
    return;

  // Special case for a vertical line
  if (p1x == p2x)
  {
    COLORREF back = theApp.GetColour(InformApp::ColourBack);
    dc.SetBkColor(back);

    CPen pen1;
    pen1.CreatePen(PS_SOLID,0,fore);
    CPen* oldPen = dc.SelectObject(&pen1);
    dc.MoveTo(p1x,p1y);
    dc.LineTo(p2x,p2y);

    CPen pen2;
    pen2.CreatePen(PS_SOLID,0,LinePixelColour(0.66,fore,back));
    dc.SelectObject(&pen2);
    dc.MoveTo(p1x-1,p1y);
    dc.LineTo(p2x-1,p2y);
    dc.MoveTo(p1x+1,p1y);
    dc.LineTo(p2x+1,p2y);

    dc.SelectObject(oldPen);
    return;
  }

  if (p2x < p1x)
  {
    int t;
    t = p2x;
    p2x = p1x;
    p1x = t;
    t = p2y;
    p2y = p1y;
    p1y = t;
  }

  int incMajor = 0, incMinor = 0;
  int d = 0, x = 0, y = 0, two_v_dx, two_v_dy;
  double invDenom, two_dx_invDenom, two_dy_invDenom;
  int count = 0;

  // Draw an anti-aliased line
  int dx = p2x - p1x;
  int dy = p2y - p1y;
  if ((dy >= 0) && (dy <= dx))
  {
    d = 2*dy-dx;
    incMajor = 2*dy;
    incMinor = 2*(dy-dx);

    two_v_dx = 0;
    invDenom = 1.0/(2.0*sqrt((double)((dx*dx)+(dy*dy))));
    two_dx_invDenom = 2.0*dx*invDenom;

    x = p1x;
    y = p1y;

    DrawLinePixel(dc,bitmap,x,y,0.0,fore);
    DrawLinePixel(dc,bitmap,x,y+1,(two_dx_invDenom/1.5),fore);
    DrawLinePixel(dc,bitmap,x,y-1,(two_dx_invDenom/1.5),fore);

    while (x < p2x)
    {
      if (d <= 0)
      {
        two_v_dx = d+dx;
        d += incMajor;
        x++;
      }
      else
      {
        two_v_dx = d-dx;
        d += incMinor;
        x++;
        y++;
      }

      DrawLinePixel(dc,bitmap,x,y,fabs((two_v_dx*invDenom)/1.5),fore);
      DrawLinePixel(dc,bitmap,x,y+1,fabs((two_dx_invDenom-(two_v_dx*invDenom))/1.5),fore);
      DrawLinePixel(dc,bitmap,x,y-1,fabs((two_dx_invDenom+(two_v_dx*invDenom))/1.5),fore);
      count = (count > 6) ? 0 : count+1;
    }
  }

  if ((dy < 0) && (-dy <= dx))
  {
    d = 2*dy+dx;
    incMajor = 2*dy;
    incMinor = 2*(dy+dx);

    two_v_dx = 0;
    invDenom = 1.0/(2.0*sqrt((double)((dx*dx)+(dy*dy))));
    two_dx_invDenom = 2.0*dx*invDenom;

    x = p1x;
    y = p1y;

    DrawLinePixel(dc,bitmap,x,y,0.0,fore);
    DrawLinePixel(dc,bitmap,x,y+1,(two_dx_invDenom/1.5),fore);
    DrawLinePixel(dc,bitmap,x,y-1,(two_dx_invDenom/1.5),fore);

    while (x < p2x)
    {
      if (d <= 0)
      {
        two_v_dx = d+dx;
        d += incMinor;
        x++;
        y--;
      }
      else
      {
        two_v_dx = d-dx;
        d += incMajor;
        x++;
      }

      DrawLinePixel(dc,bitmap,x,y,fabs((two_v_dx*invDenom)/1.5),fore);
      DrawLinePixel(dc,bitmap,x,y+1,fabs((two_dx_invDenom-(two_v_dx*invDenom))/1.5),fore);
      DrawLinePixel(dc,bitmap,x,y-1,fabs((two_dx_invDenom+(two_v_dx*invDenom))/1.5),fore);
      count = (count > 6) ? 0 : count+1;
    }
  }

  if ((dy > 0) && (dy > dx))
  {
    d = 2*dx-dy;
    incMajor = 2*dx;
    incMinor = 2*(dx-dy);

    two_v_dy = 0;
    invDenom = 1.0/(2.0*sqrt((double)((dx*dx)+(dy*dy))));
    two_dy_invDenom = 2.0*dy*invDenom;

    x = p1x;
    y = p1y;

    DrawLinePixel(dc,bitmap,x,y,0.0,fore);
    DrawLinePixel(dc,bitmap,x+1,y,(two_dy_invDenom/1.5),fore);
    DrawLinePixel(dc,bitmap,x-1,y,(two_dy_invDenom/1.5),fore);

    while (y < p2y)
    {
      if (d <= 0)
      {
        two_v_dy = d + dy;
        d += incMajor;
        y++;
      }
      else
      {
        two_v_dy = d - dy;
        d += incMinor;
        y++;
        x++;
      }

      DrawLinePixel(dc,bitmap,x,y,fabs((two_v_dy*invDenom)/1.5),fore);
      DrawLinePixel(dc,bitmap,x+1,y,fabs((two_dy_invDenom-(two_v_dy*invDenom))/1.5),fore);
      DrawLinePixel(dc,bitmap,x-1,y,fabs((two_dy_invDenom+(two_v_dy*invDenom))/1.5),fore);
      count = (count > 6) ? 0 : count+1;
    }
  }

  if ((dy < 0) && (-dy > dx))
  {
    d = 2*dx+dy;
    incMajor = 2*dx;
    incMinor = 2*(dx+dy);

    two_v_dy = 0;
    invDenom = 1.0/(2.0*sqrt((double)((dx*dx)+(dy*dy))));
    two_dy_invDenom = 2.0*fabs((double)dy)*invDenom;

    x = p1x;
    y = p1y;

    DrawLinePixel(dc,bitmap,x,y,0.0,fore);
    DrawLinePixel(dc,bitmap,x+1,y,(two_dy_invDenom/1.5),fore);
    DrawLinePixel(dc,bitmap,x-1,y,(two_dy_invDenom/1.5),fore);

    while (y > p2y)
    {
      if (d <= 0)
      {
        two_v_dy = d-dy;
        d += incMajor;
        y--;
      }
      else
      {
        two_v_dy = d+dy;
        d += incMinor;
        y--;
        x++;
      }

      DrawLinePixel(dc,bitmap,x,y,fabs((two_v_dy*invDenom)/1.5),fore);
      DrawLinePixel(dc,bitmap,x+1,y,fabs((two_dy_invDenom-(two_v_dy*invDenom))/1.5),fore);
      DrawLinePixel(dc,bitmap,x-1,y,fabs((two_dy_invDenom+(two_v_dy*invDenom))/1.5),fore);
      count = (count > 6) ? 0 : count+1;
    }
  }
}

void SkeinWindow::DrawLinePixel(CDC& dc, CDibSection& bitmap, int x, int y, double i,
  COLORREF fore)
{
  CSize size = bitmap.GetSize();
  if ((x < 0) || (y < 0) || (x >= size.cx) || (y >= size.cy))
    return;

  COLORREF back = bitmap.GetPixelColour(x,y);
  int r = (int)(((1.0-i)*GetRValue(fore))+(i*GetRValue(back)));
  int g = (int)(((1.0-i)*GetGValue(fore))+(i*GetGValue(back)));
  int b = (int)(((1.0-i)*GetBValue(fore))+(i*GetBValue(back)));
  bitmap.SetPixel(x,y,(r<<16)|(g<<8)|b);
}

COLORREF SkeinWindow::LinePixelColour(double i, COLORREF fore, COLORREF back)
{
  int r = (int)(((1.0-i)*GetRValue(fore))+(i*GetRValue(back)));
  int g = (int)(((1.0-i)*GetGValue(fore))+(i*GetGValue(back)));
  int b = (int)(((1.0-i)*GetBValue(fore))+(i*GetBValue(back)));
  return RGB(r,g,b);
}

CDibSection* SkeinWindow::GetImage(const char* name)
{
  // Is the image in the cache?
  CString skeinName;
  skeinName.Format("%s-scaled",name);
  CDibSection* dib = theApp.GetCachedImage(skeinName);
  if (dib != NULL)
    return dib;

  // Create the scaled image
  dib = theApp.CreateScaledImage(theApp.GetCachedImage(name),
    m_fontSize.cx*0.15,m_fontSize.cy*0.06);
  theApp.CacheImage(skeinName,dib);
  return dib;
}

CRect SkeinWindow::GetMenuButtonRect(const CRect& nodeRect, CDibSection* menu)
{
  if (menu == NULL)
    menu = m_bitmaps[MenuUnselected];

  CSize sz = menu->GetSize();
  return CRect(CPoint(nodeRect.right-sz.cx+(sz.cx/3),nodeRect.top-(sz.cy/4)),sz);
}

CRect SkeinWindow::GetBadgeRect(const CRect& nodeRect)
{
  CSize sz = m_bitmaps[DiffersBadge]->GetSize();
  return CRect(CPoint(nodeRect.left,nodeRect.top-(sz.cy/4)),sz);
}

void SkeinWindow::RemoveExcessSeparators(CMenu* menu)
{
  bool allow = true;
  int i = 0;
  while (i < menu->GetMenuItemCount())
  {
    bool removed = false;

    if (menu->GetMenuItemID(i) == 0)
    {
      if (!allow)
      {
        menu->RemoveMenu(i,MF_BYPOSITION);
        removed = true;
      }
      allow = false;
    }
    else
      allow = true;

    if (!removed)
      i++;
  }
}

Skein::Node* SkeinWindow::NodeAtPoint(const CPoint& point)
{
  std::map<Skein::Node*,CRect>::const_iterator it;
  for (it = m_nodes.begin(); it != m_nodes.end(); ++it)
  {
    CRect r = it->second;
    r.InflateRect(m_fontSize.cx/2,m_fontSize.cy/4);
    if (r.PtInRect(point))
      return it->first;
  }
  return NULL;
}

bool SkeinWindow::ShowLabel(Skein::Node* node)
{
  if (node->HasLabel())
    return true;
  if (m_edit.IsWindowVisible() && m_edit.EditingLabel(node))
    return true;
  return false;
}

void SkeinWindow::StartEdit(Skein::Node* node, bool label)
{
  std::map<Skein::Node*,CRect>::const_iterator it = m_nodes.find(node);
  if (it != m_nodes.end())
  {
    CDibSection* back = m_bitmaps[BackUnselected];
    CRect nodeRect = it->second;

    if (label)
    {
      nodeRect.InflateRect((node->GetLabelTextWidth()-nodeRect.Width())/2,0);
      nodeRect.InflateRect(m_fontSize.cx,0);
      nodeRect.top += (back->GetSize().cy/2);
      nodeRect.top -= (int)(0.12*m_fontSize.cy);
      nodeRect.top -= (int)(1.6*m_fontSize.cy);
      nodeRect.bottom = nodeRect.top + m_fontSize.cy+1;
    }
    else
    {
      nodeRect.DeflateRect(m_fontSize.cx*3,0);
      nodeRect.top += (back->GetSize().cy/2);
      nodeRect.top -= (int)(0.12*m_fontSize.cy);
      nodeRect.top -= (int)(0.5*m_fontSize.cy);
      nodeRect.bottom = nodeRect.top + m_fontSize.cy+1;
    }

    m_edit.SetFont(theApp.GetFont(InformApp::FontDisplay));
    m_edit.StartEdit(node,nodeRect,label);
  }
}
