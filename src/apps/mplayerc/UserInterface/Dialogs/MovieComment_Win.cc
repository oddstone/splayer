
// mfchostDoc.cpp : CmfchostDoc ���ʵ��
//

#include "stdafx.h"
#include "MovieComment_Win.h"
#include <exdispid.h>
#include "logging.h"
#include "../../MainFrm.h"
#include "base64.h"
#include "strings.h"
#include <fstream>
#include "../../Controller/ShareController.h"
#include "..\..\svplib\SVPToolBox.h"

IMPLEMENT_DYNAMIC(MovieComment, CDHtmlDialog)

BEGIN_MESSAGE_MAP(MovieComment, CDHtmlDialog)
  ON_WM_SIZE()
  ON_WM_TIMER()
END_MESSAGE_MAP()

BEGIN_DHTML_EVENT_MAP(MovieComment)
  DHTML_EVENT_ONCLICK(L"close_wnd", OnEventClose)
  DHTML_EVENT_ONCLICK(L"open_newlink", OpenNewLink)
END_DHTML_EVENT_MAP()

BEGIN_EVENTSINK_MAP(MovieComment, CDHtmlDialog)
  ON_EVENT(MovieComment, AFX_IDC_BROWSER, DISPID_NEWWINDOW3, OnEventNewLink, VTS_DISPATCH VTS_PBOOL VTS_UI4 VTS_BSTR VTS_BSTR)
END_EVENTSINK_MAP()

BEGIN_DISPATCH_MAP(MovieComment, CDHtmlDialog)
  DISP_FUNCTION(MovieComment, "CallSPlayer", CallSPlayer, VT_BSTR, VTS_BSTR VTS_BSTR)
END_DISPATCH_MAP()

MovieComment::MovieComment()
: m_initialize(0),
  m_oadlg(NULL)
{
  SupportContextMenu(FALSE);
  HostFlags(DOCHOSTUIFLAG_THEME | DOCHOSTUIFLAG_SCROLL_NO | DOCHOSTUIFLAG_NO3DBORDER
    | DOCHOSTUIFLAG_DISABLE_HELP_MENU | DOCHOSTUIFLAG_DIALOG | DOCHOSTUIFLAG_ENABLE_ACTIVEX_INACTIVATE_MODE
    | DOCHOSTUIFLAG_DISABLE_SCRIPT_INACTIVE | DOCHOSTUIFLAG_OVERRIDEBEHAVIORFACTORY
    );
}

MovieComment::~MovieComment()
{
  CloseOAuth();
}

BOOL MovieComment::OnEventNewLink(IDispatch **ppDisp, VARIANT_BOOL *Cancel,
                                  DWORD dwFlags, BSTR bstrUrlContext, BSTR bstrUrl)
{
  OpenNewLink(bstrUrl);

  *Cancel = VARIANT_TRUE;

  return S_OK;  
}

void MovieComment::OnSize(UINT nType, int cx, int cy)
{
  __super::OnSize(nType, cx, cy);

  CRect rc;
  WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
  GetWindowPlacement(&wp);
  GetWindowRect(&rc);
  rc-=rc.TopLeft();

  // destroy old region
  if((HRGN)m_rgn)
    m_rgn.DeleteObject();

  // create rounded rect region based on new window size
  if (wp.showCmd != SW_MAXIMIZE )
    m_rgn.CreateRoundRectRgn(0, 0, rc.Width(), rc.Height(), 7, 7);
  else
    m_rgn.CreateRectRgn( 0,0, rc.Width(), rc.Height() );

  SetWindowRgn(m_rgn,TRUE);
}

BSTR MovieComment::CallSPlayer(LPCTSTR p, LPCTSTR param)
{
  CString ret = L"0";
  std::wstring cmd(p);

  if (cmd.empty())
    ret = L"-1";
  else if (cmd == L"rev")
    ret = L"2";
  else if (cmd == L"reload")
  {
    UserShareController::GetInstance()->_Stop();
    UserShareController::GetInstance()->_Start();
  }
  else if (cmd == L"open")
  {
    AdjustMainWnd();
    ShowFrame();
  }
  else if (cmd == L"close")
    HideFrame();
  else if (cmd == L"openoauth")
    OpenOAuth(param);
  else if (cmd == L"closeoauth")
    CloseOAuth();
  else if (cmd == L"openlink")
    OpenNewLink(param);
  else if (cmd == L"snapshoot") 
    OnEventCapture(NULL);
  else if (cmd == L"curtime")
    ret = GetMovieTime(1).c_str();
  else if (cmd == L"totaltime")
    ret = GetMovieTime(0).c_str();
  else
    ret = L"-1";

  return ret.AllocSysString();
}

std::wstring MovieComment::GetMovieTime(int i)
{
  __int64 rtDur = 0;
  CMainFrame* mf = (CMainFrame*)AfxGetMainWnd();

  if (mf && mf->pMC)
    i==0 ? mf->pMS->GetDuration(&rtDur) :
           mf->pMS->GetCurrentPosition(&rtDur);

  wchar_t buf[64];
  _i64tow(rtDur, buf, 10);
  return std::wstring(buf);
}

BOOL MovieComment::OnInitDialog()
{
  DhtmlDlgBase::OnInitDialog();

  CalcWndPos();
  HideFrame();

  SupportJSCallBack();
  m_initialize = 1;
  ClearFrame();

  return TRUE;
}
void MovieComment::ClearFrame()
{
	if (!m_hWnd || !m_initialize)
		return;

	DhtmlDlgBase::ClearFrame();
}

HRESULT MovieComment::OnEventCapture(IHTMLElement* pElement)
{
  try
  {
    IHTMLStyle* imgstyle = NULL;

    IHTMLElement* imgele = NULL;
    IHTMLElement* dataele = NULL;
    IHTMLElement* extele = NULL;
    IHTMLElement* test = NULL;

    GetElement(L"cut-image-data", &dataele);
    GetElement(L"cut-image-ext", &extele);
    GetElement(L"cut-image-div", &imgele);
    if (!dataele || !extele || !imgele)
      return S_FALSE;

    imgele->get_style(&imgstyle);
    if (!imgstyle)
      return S_FALSE;

    CSVPToolBox svptool;
    std::wstring imgpath;
    CString tpath;

    svptool.GetAppDataPath(imgpath);
    imgpath += L"\\SVPSub";
    _wmkdir(imgpath.c_str());

    tpath.Format(L"%s\\tmp_smst_%d.jpg", imgpath.c_str(), time(NULL));
    imgpath = tpath;

    CMainFrame* cmf = (CMainFrame*)AfxGetMainWnd();
    if (!cmf)
      return S_FALSE;

    cmf->SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
    cmf->SnapShootImage(imgpath, (cmf->GetVideoSize().cx > 960)?TRUE:FALSE);

    CComVariant imgsrc = imgpath.c_str();
    if (S_FALSE == imgele->setAttribute(L"src", imgsrc))
      return S_FALSE;
    imgstyle->put_display(L"");

    std::ifstream fs;
    fs.open(imgpath.c_str(), std::ios::binary|std::ios::in);
    if (fs.bad())
      return S_FALSE;

    fs.seekg(0, std::ios::end);
    UINT filesize = fs.tellg();
    fs.seekg(0, std::ios_base::beg);

    unsigned char* buf = new unsigned char[filesize];
    if (!buf)
      return S_FALSE;

    fs.read((char*)buf, filesize);
    fs.close();

    std::string bufstr = base64_encode(buf, filesize);
    delete [] buf;

    CComVariant imgdata = Strings::Utf8StringToWString(bufstr).c_str();
    dataele->setAttribute(L"value", imgdata);
    CComVariant imgext = AfxGetAppSettings().SnapShotExt;
    extele->setAttribute(L"value", imgext);
  }
  catch (...)
  {
    ;
  }

  return S_FALSE;
}

HRESULT MovieComment::OpenNewLink(IHTMLElement *pElement)
{
  CComBSTR tagName;
  CComBSTR url;
  CComVariant v;
  pElement->get_tagName(&tagName);
  pElement->getAttribute(L"href", 0, &v);
  v.CopyTo(&url);
  
  if (tagName == L"A" && url)
    OpenNewLink(url);

  return S_FALSE;
}

void MovieComment::OpenNewLink(LPCTSTR url)
{
  std::wstring str(url);
  if (str.find(L"http://") != std::string::npos && str.length() > 8)
    ShellExecute(NULL, L"open", str.c_str(), L"", L"", SW_SHOW);
}

HRESULT MovieComment::OnEventClose(IHTMLElement* /*pElement*/)
{
  HideFrame();
  return S_OK;
}

void MovieComment::ShowFrame()
{
  DhtmlDlgBase::ShowFrame();
  AdjustMainWnd();
}

BOOL MovieComment::AdjustMainWnd()
{
  RECT rc;
  GetParent()->GetWindowRect(&rc);
  CRect rect(rc);
  BOOL ret = FALSE;

  int width = 0;
  int height = 0;
  if (m_oadlg && m_oadlg->IsWindowVisible())
  {
      width = 600;
      height = 550;
      ret = TRUE;
  }
  else if (IsWindowVisible())
  {
      width = 300;
      height = 470;
      ret = TRUE;
  }

  if (ret && rect.Width() < width || rect.Height() < height)
  {
    if (rect.Width() >= width)
      width = rect.Width();
    if (rect.Height() >= height)
      height = rect.Height();
    m_mainrc.left = (GetSystemMetrics(SM_CXSCREEN)-width)/2; 
    m_mainrc.top = (GetSystemMetrics(SM_CYSCREEN)-height)/2;
    m_mainrc.right = m_mainrc.left+width;
    m_mainrc.bottom = m_mainrc.top+height;
    GetParent()->MoveWindow(&m_mainrc);
  }
  return ret;
}

void MovieComment::CalcWndPos()
{
  RECT rc;
  GetParent()->GetWindowRect(&rc);

  CRect c1(m_mainrc);
  CRect c2(rc);

  if (m_oadlg && m_oadlg->IsWindowVisible() || IsWindowVisible())
  {
    if (c1.Width() != c2.Width() || c1.Height() != c2.Height())
    {
      CloseOAuth();
      HideFrame();
    }
  }

  m_mainrc = rc;
  rc.top = rc.bottom - 410;
  rc.left = rc.left + 20;
  rc.right = 240;
  rc.bottom = 320;

  SetFramePos(rc);

  if (m_oadlg)
    m_oadlg->CalcOauthPos();
}

void MovieComment::CloseOAuth()
{
  if (m_oadlg)
  {
    delete m_oadlg;
    m_oadlg = NULL;
	  m_oauthHeight = 0;
	  m_detalt = 0;
	  m_offset = 0;
  }
}

void MovieComment::OnTimer(UINT_PTR nIDEvent)
{
	// update
	if (nIDEvent == 1)
	{
		m_detalt += (timeGetTime()-m_ut);
		m_oauthHeight = m_detalt*(m_offset/m_dt);
		m_ut = timeGetTime();
	}
	// render
	else if (nIDEvent == 2)
		RenderAni();

	if (timeGetTime()-m_st >= m_dt)
	{
		KillTimer(1);
		KillTimer(2);
		m_oauthHeight = 400;
		RenderAni();
		if (m_oadlg)
			m_oadlg->m_btnclose.ShowWindow(SW_SHOW);
	}
}

void MovieComment::OpenOAuth(LPCTSTR str)
{
  std::wstring url(str);
  if (url.empty() || url.find(L"http://") == std::string::npos)
    return;

  if (m_oadlg)
	  CloseOAuth();

  CMainFrame* cmf = (CMainFrame*)AfxGetMainWnd();
  if (cmf && cmf->GetMediaState() == State_Running)
	  cmf->OnPlayPlaypause();

  m_oadlg = new OAuthDlg;
  m_oadlg->CreateFrame(DS_SETFONT|DS_FIXEDSYS|WS_POPUP|WS_DISABLED,WS_EX_NOACTIVATE);

  m_oadlg->ClearFrame();
  m_oadlg->SetUrl(url);
  m_oadlg->ShowFrame();
  AdjustMainWnd();
  m_oadlg->CalcOauthPos(FALSE);

  RECT rc = m_oadlg->m_currect;
  rc.bottom = 0;
  m_oadlg->SetFramePos(rc);

  m_st = m_ut = timeGetTime();
  m_dt = 300.0f;

  m_oauthHeight = 0;
  m_detalt = 0;
  m_offset = 400.f;
  SetTimer(1, 1, NULL);
  SetTimer(2, 33, NULL);
}

STDMETHODIMP MovieComment::TranslateAccelerator(LPMSG lpMsg, const GUID* /*pguidCmdGroup*/, DWORD /*nCmdID*/)
{
  switch (lpMsg->message)
  {
  case WM_KEYDOWN:
    switch (lpMsg->wParam)
    {
    case VK_RETURN:
      return S_OK;
    case VK_ESCAPE:
      HideFrame();
      return S_OK;  // let me handle
    }
    break;
  }

  return S_FALSE;
}

void MovieComment::RenderAni()
{
	if (m_oadlg)
	{
		RECT rc = m_oadlg->m_currect;
		rc.bottom = m_oauthHeight;
		m_oadlg->SetWindowPos(NULL, rc.left, rc.top, rc.right, rc.bottom,
			SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOREDRAW|SWP_NOREPOSITION|SWP_NOZORDER);
	}
}