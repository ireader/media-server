
// rtspclientDlg.cpp : implementation file
//

#include "stdafx.h"
#include "rtspclient.h"
#include "rtspclientDlg.h"
#include "rtsp-client.h"

#include "sys/sock.h"
#include "sys/process.h"

#include "rtp-avp-udp.h"
#include "h264-source.h"

#include "hls-server.h"
#include "H264Reader.h"

#include "http-server.h"
#include "StdCFile.h"

//#define MPEG_TS
#define MPEG_PS

#if defined(MPEG_TS)
#include "mpeg-ts.h"
#elif defined(MPEG_PS)
#include "mpeg-ps.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static unsigned char s_buffer[8*1024*1024];

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CrtspclientDlg dialog




CrtspclientDlg::CrtspclientDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CrtspclientDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CrtspclientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CrtspclientDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, &CrtspclientDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CrtspclientDlg::OnBnClickedCancel)
END_MESSAGE_MAP()

static void OnH264Write(void* param, const void* packet, size_t bytes)
{
	FILE *fp = (FILE*)param;
	fwrite(packet, 1, bytes, fp);
	fflush(fp);
}

#include "../video/h264-util.h"
static void OnReadH264(void* ts, const void* data, int bytes)
{
	static int64_t pcr = 0;
	static int64_t pts = 0;

	int i;
	int j = 0;
	const unsigned char* p = (const unsigned char*)data;
	for(i = 0; i + 4 < bytes; i++)
	{
		if(0x00 == p[i] && 0x00 == p[i+1] && 0x01 == p[i+2])
		{
			int naltype = p[i+3] & 0x1f;
			if(7 != naltype && 8 != naltype && 9 != naltype)
			{
				while(j > 0 && 0x00==s_buffer[j-1])
				{
					--j; // remove zero_bytes;
				}
			}
		}

		s_buffer[j++] = p[i];
	}

	while(i < bytes)
		s_buffer[j++] = p[i++];
	data = s_buffer;
	bytes = j;

	if(0 == pcr)
	{
		time_t t;
		time(&t);
		pts = 3600;//t * 90000;
		pcr = pts * 300;
	}
	else
	{
		pts += 90 * 40; // 90kHZ * 40ms
	}

	//if(pts < 90000 + 90 * 3000)
#if defined(MPEG_TS)
		mpeg_ts_write(ts, 0x1b, pts, pts, data, bytes);
#elif defined(MPEG_PS)
	mpeg_ps_write(ts, STREAM_VIDEO_H264, pts, pts, data, bytes);
#endif
}

static int OnHTTP(void* param, void* session, const char* method, const char* path)
{
	TRACE("OnHTTP: %s\n", path);
	if(0 == strcmp("/1.m3u8", path))
	{
		const char* m3u8 = 
			"#EXTM3U\n"
			"#EXT-X-VERSION:3\n"
			"#EXT-X-TARGETDURATION:16\n"
			"#EXT-X-MEDIA-SEQUENCE:0\n"
			"#EXTINF:16,\n"
			"1.ts\n"
			"#EXTINF:16,\n"
			"2.ts\n"
			"#EXTINF:16,\n"
			"3.ts\n";
			//"#EXT-X-ENDLIST\n";

		void* bundle = http_bundle_alloc(strlen(m3u8)+1);
		void* ptr = http_bundle_lock(bundle);
		strcpy((char*)ptr, m3u8);
		http_bundle_unlock(bundle, strlen(m3u8)+1);

		http_server_set_content_type(session, "application/vnd.apple.mpegURL");
		http_server_send(session, 200,bundle);
		http_bundle_free(bundle);
	}
	else if(0 == strcmp("/1.ts", path))
	{
		StdCFile file("e:\\2.ts", "rb");
		long sz = file.GetFileSize();
		void* bundle = http_bundle_alloc(sz);
		void* ptr = http_bundle_lock(bundle);
		file.Read(ptr, sz);
		http_bundle_unlock(bundle, sz);

		//socket_send_all_by_time(sock, p, sz, 0, 5000);
		http_server_set_content_type(session, "video/MP2T");
		http_server_send(session, 200, bundle);
		http_bundle_free(bundle);
	}
	else if(0 == strcmp("/sjz.mov", path))
	{
		StdCFile file("e:\\sjz.mov", "rb");
		long sz = file.GetFileSize();
		void* bundle = http_bundle_alloc(sz);
		void* ptr = http_bundle_lock(bundle);
		file.Read(ptr, sz);
		http_bundle_unlock(bundle, sz);

		//socket_send_all_by_time(sock, p, sz, 0, 5000);
		http_server_set_content_type(session, "video/MP2T");
		http_server_send(session, 200, bundle);
		http_bundle_free(bundle);
	}
	else
	{
		StdCFile file("e:\\2.ts", "rb");
		long sz = file.GetFileSize();
		void* bundle = http_bundle_alloc(sz);
		void* ptr = http_bundle_lock(bundle);
		file.Read(ptr, sz);
		http_bundle_unlock(bundle, sz);

		//socket_send_all_by_time(sock, p, sz, 0, 5000);
		http_server_set_content_type(session, "video/MP2T");
		http_server_send(session, 200, bundle);
		http_bundle_free(bundle);

		//assert(0);
	}

	return 0;
}


static unsigned char* search_start_code(unsigned char* stream, int bytes)
{
	unsigned char *p;
	for(p = stream; p+3<stream+bytes; p++)
	{
		if(0x00 == p[0] && 0x00 == p[1] && 0x00 == p[2] && 0x01 == p[3])
			return p;
	}
	return NULL;
}

typedef void (*OnH264Data)(void* param, const void* data, int bytes);
static int H264File(OnH264Data callback, void* param)
{
	FILE *fp;
	int length;
	unsigned char* p;
	
	fp = fopen("e:\\sjz.h264", "rb");
	length = fread(s_buffer, 1, sizeof(s_buffer), fp);
	fclose(fp);

	p = s_buffer;

	while(p)
	{
		unsigned char* p1;
		unsigned char* p2;
		unsigned int nal_unit_type;

		p1 = p;

		while(p1)
		{
			p1 += 4;
			p2 = search_start_code(p1, length-(p1-s_buffer));
			if(!p2)
			{
				// file end ? 
				callback(param, p, length - (p-s_buffer));
				return 0;
			}

			nal_unit_type = 0x1F & p1[0];
			if( (nal_unit_type>0 && nal_unit_type <= 5) // data slice
				|| 10==nal_unit_type // end of sequence
				|| 11==nal_unit_type) // end of stream
			{
				callback(param, p, p2-p);
				p = p2;
				break;
			}

			p1 = p2;
		}
	}

	return 1;
}

static void OnReadFile(void* camera, const void* data, int bytes)
{
	//static int64_t pcr = 0;
	//static int64_t pts = 0;

	//if(0 == pcr)
	//{
	//	time_t t;
	//	time(&t);
	//	pts = 412536249;//t * 90000;
	//}
	//else
	//{
	//	pts += 90 * 40; // 90kHZ * 40ms
	//}

	//pcr = pts * 300;
	hsl_server_input(camera, data, bytes, 0x1b);
	Sleep(40);
}

static int STDCALL OnLiveThread(IN void* param)
{
	while(1)
	{
//		H264Reader reader;
//		reader.Open("e:\\sjz.h264");

		//while(reader.Read(OnReadFile, param) > 0)
		//	Sleep(40);

		H264File(OnReadFile, param);
	}

	return 0;
}

BOOL CrtspclientDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	socket_init();
	rtp_avp_init();
//	aio_socket_init(1);
	
//	thread_t thread;
//	thread_create(&thread, OnThread, NULL);

	//http_server_init();
	//void* server = http_server_create(NULL, 80);
	//http_server_set_handler(server, OnHTTP, NULL);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CrtspclientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CrtspclientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

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

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CrtspclientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

static void OnPSRead(void* param, const void* packet, size_t bytes)
{
	FILE* fp = (FILE*)param;

	fwrite(packet, 1, bytes, fp);
	fflush(fp);
}

static void OnData(void* param, unsigned char nal, const void* data, int bytes)
{
	FILE *fp = (FILE*)param;
	int startcode = 0x01000000;
	fwrite(&startcode, 1, 4, fp);
	fwrite(&nal, 1, 1, fp);
	fwrite(data, 1, bytes, fp);
	fflush(fp);
}

void CrtspclientDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	//OnOK();

#if 0
	unsigned char buffer[188] = {0};
	FILE *fp = fopen("e:\\fileSequence0.ts", "rb");
	while(fread(buffer, 188, 1, fp) > 0)
	{
		ts_packet_dec(buffer, 188);
	}
	fclose(fp);
#else
	int r, n;
	const unsigned char *p;
	FILE *wfp = fopen("e:\\1.svac", "wb");
	//FILE *rfp = fopen("e:\\1.svac.ps", "rb");
	FILE *rfp = fopen("e:\\svac.ps", "rb");
	//FILE *rfp = fopen("e:\\sjz.ps", "rb");
	while(r = fread(s_buffer, 1, sizeof(s_buffer), rfp))
	{
		p = s_buffer;
		while(r > 0)
		{
			n = mpeg_ps_packet_dec(p, r, OnPSRead, wfp);
			p += n;
			r -= n;
		}
	}
	fclose(rfp);
	fclose(wfp);
#endif


	//int rtp, rtcp;
	////void* rtsp = rtsp_open("rtsp://127.0.0.1/sjz.264");
	//void* rtsp = rtsp_open("rtsp://192.168.11.229/sjz.264");
	//rtsp_media_get_rtp_socket(rtsp, 0, &rtp, &rtcp);
	//void *queue = rtp_queue_create();
	//void *avp = rtp_avp_udp_create(rtp, rtcp, queue);
	//rtp_avp_udp_start(avp);
	//rtsp_play(rtsp);

	//FILE *fp = fopen("e:\\r.bin", "wb");
	//void* h264 = h264_source_create(queue, OnData, fp);
	//while(1)
	//{
	//	h264_source_process(h264);
	//}
	//h264_source_destroy(h264);
	//fclose(fp);
}

void CrtspclientDlg::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	//OnCancel();

#if defined(MPEG_TS)
#if 1
	H264Reader reader;
	reader.Open("e:\\sjz.h264");

	FILE *fp = fopen("e:\\2.ts", "wb");
	void* ts = mpeg_ts_create(OnH264Write, fp);
	while(reader.Read(OnReadH264, ts) > 0);
	mpeg_ts_destroy(ts);
	fclose(fp);
#else
	int sid = 0;
	int len = 0;
	static char ptr[2*1024*1024] = {0};

	FILE *fp2 = fopen("e:\\2.ts", "wb");
	void* ts = mpeg_ts_create(OnTSWrite, fp2);
	
	FILE *fp = fopen("e:\\0.raw", "rb");
	while(4 == fread(&sid, 1, 4, fp))
	{
		fread(&len, 1, 4, fp);
		fread(ptr, 1, len, fp);

		static int64_t pcr = 0;
		static int64_t pts = 0;

		if(0 == pcr)
		{
			time_t t;
			time(&t);
			pts = 90000;//t * 90000;
			pcr = pts * 300;
		}
		else
		{
			pts += 90 * 40; // 90kHZ * 40ms
		}

		if(pts < 90000 + 90 * 4000)
			mpeg_ts_write(ts, sid, pts, pts, ptr, len);
	}
	mpeg_ts_destroy(ts);
	fclose(fp);
	fclose(fp2);
#endif

#elif defined(MPEG_PS)
	H264Reader reader;
	reader.Open("e:\\sjz.h264");

	static uint8_t svac_video[] = { 0x2a, 0x0a, 0x7f, 0xff, 0x00, 0x00, 0x07, 0x08, 0x1f, 0xfe, 0x2c, 0x24 };
	assert(sizeof(svac_video) == 0x0c);

	FILE *fp = fopen("e:\\sjz.ps", "wb");
	void* ps = mpeg_ps_create(OnH264Write, fp);
	mpeg_ps_add_stream(ps, STREAM_VIDEO_H264, NULL, 0);
	while(reader.Read(OnReadH264, ps) > 0);
	mpeg_ps_destroy(ps);
	fclose(fp);
#endif
}
