// intergifDlg.cpp : implementation file
//

#include "stdafx.h"
#include "igwin.h"
#include "intergifDlg.h"

extern "C" {
#include "animlib.h"
#include "intergif.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CIntergifDlg dialog

CIntergifDlg::CIntergifDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CIntergifDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CIntergifDlg)
    m_nTransType = 0;
    m_nTransPixel = 0;
    m_bLoop = FALSE;
    m_bJoin = FALSE;
    m_bInterlace = FALSE;
    m_optimisecolours = 255;
    m_palette = 0;
    m_bSplit = FALSE;
    //}}AFX_DATA_INIT
    // Note that LoadIcon does not require a subsequent DestroyIcon in Win32
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CIntergifDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CIntergifDlg)
    DDX_Control(pDX, IDC_INFILE, m_infile);
    DDX_Radio(pDX, IDC_TRANS_NONE, m_nTransType);
    DDX_Text(pDX, IDC_TRANS_PIXEL, m_nTransPixel);
    DDV_MinMaxUInt(pDX, m_nTransPixel, 0, 255);
    DDX_Check(pDX, IDC_LOOP, m_bLoop);
    DDX_Check(pDX, IDC_JOIN, m_bJoin);
    DDX_Check(pDX, IDC_INTERLACE, m_bInterlace);
    DDX_Text(pDX, IDC_PALETTE_COLOURS, m_optimisecolours);
    DDV_MinMaxInt(pDX, m_optimisecolours, 2, 256);
    DDX_Radio(pDX, IDC_PALETTE_KEEP, m_palette);
    DDX_Check(pDX, IDC_SPLIT, m_bSplit);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CIntergifDlg, CDialog)
    //{{AFX_MSG_MAP(CIntergifDlg)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_CONVERT, OnConvert)
    ON_BN_CLICKED(IDC_BROWSE, OnClickBrowse)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIntergifDlg message handlers

BOOL CIntergifDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);            // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    // TODO: Add extra initialization here

    return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CIntergifDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialog::OnPaint();
    }
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CIntergifDlg::OnQueryDragIcon()
{
    return (HCURSOR) m_hIcon;
}

void CIntergifDlg::Error()
{
    MessageBox( Anim_Error, "Error from InterGif", MB_ICONSTOP | MB_RETRYCANCEL );
}

void CIntergifDlg::OnConvert()
{
    CString name;

    UpdateData( TRUE );
    m_infile.GetWindowText( name );
    name += ".gif";

    CFileDialog saveas( FALSE, ".gif", name,
                        OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,
                        "GIF images (*.gif)|*.gif|All files (*.*)|*.*||" );

    if ( saveas.DoModal() == IDOK )
    {
        CString infile, outfile;
        anim_GIFflags agf = { 0 };

        m_infile.GetWindowText( infile );

        outfile = saveas.GetPathName();

        agf.bInterlace = m_bInterlace;
        agf.bLoop = m_bLoop;
        agf.nDefaultDelay = 8;      // probably not used

        if ( InterGif( infile, m_bJoin, m_bSplit, FALSE, agf,
                       (m_nTransType == 2), /* bForceTrans */
                       m_nTransPixel,
                       (m_nTransType == 1), /* bUseTrans */
                       (m_palette == 1),    /* b216 */
                       FALSE,               /* b256 */
                       FALSE,               /* bDither */
                       NULL,                /* palfile */
                       (m_palette == 0),    /* bSame */
                       (m_palette == 2 ? m_optimisecolours : -1 ),
                       outfile ) )
            m_infile.SetWindowText("");
        else
            Error();
    }
}

void CIntergifDlg::OnClickBrowse()
{
    CFileDialog dlg( TRUE, NULL, NULL, 0, NULL, this );

    dlg.m_ofn.lpstrTitle = "Select file to convert";

    dlg.m_ofn.Flags |= OFN_HIDEREADONLY;

    if ( dlg.DoModal() == IDOK )
    {
        m_infile.SetWindowText( dlg.GetPathName() );
    }
}


/////////////////////////////////////////////////////////////////////////////
// CFileTarget
// Like a CEdit but can grab the filename from a drag-and-drop

CFileTarget::CFileTarget()
{
}

CFileTarget::~CFileTarget()
{
}


BEGIN_MESSAGE_MAP(CFileTarget, CEdit)
    //{{AFX_MSG_MAP(CFileTarget)
    ON_WM_DROPFILES()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFileTarget message handlers

void CFileTarget::OnDropFiles(HDROP hDropInfo)
{
    char buffer[256];

    DragQueryFile( hDropInfo, 0, buffer, 256 );
    SetWindowText( buffer );
}
