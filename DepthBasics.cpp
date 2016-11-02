//------------------------------------------------------------------------------
// <copyright file="DepthBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#include "stdafx.h"
#include <strsafe.h>
#include "resource.h"
#include "DepthBasics.h"
#include <iostream>
#include <fstream>

int frameCounter=0;
using namespace std;
static const int        cColorWidth = 1920;
static const int        cColorHeight = 1080;

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CDepthBasics application;
    application.Run(hInstance, nShowCmd);
}

void CreateFolder(const wchar_t * path)
{
	if (!CreateDirectory(path, NULL))
	{
		return;
	}
}

/// <summary>
/// Constructor
/// </summary>
CDepthBasics::CDepthBasics() :
    m_hWnd(NULL),
    m_nStartTime(0),
    m_nLastCounter(0),
    m_nFramesSinceUpdate(0),
    m_fFreq(0),
    m_nNextStatusTime(0LL),
    m_bSaveScreenshot(false),
    m_pKinectSensor(NULL),
	m_pMultiSourceFrameReader(NULL),
	m_pDepthFrameReader(NULL),
	m_pColorFrameReader(NULL),
    m_pD2DFactory(NULL),
    m_pDrawDepth(NULL),
    m_pDepthRGBX(NULL),
	m_bStartSync(false),
	m_bWriteDepthFile(false),
	m_sThread(nullptr),
	m_sthreadId(0),
	m_frameCounter(0),
	m_bWriteRGBD(false),
	m_Master(false)
{
    LARGE_INTEGER qpf = {0};
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }

    // create heap storage for depth pixel data in RGBX format
    m_pDepthRGBX = new RGBQUAD[cDepthWidth * cDepthHeight];
	m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];

	// prepare the data and capture folders
	CreateFolder(L"data");
	CreateFolder(L"capture");
}


/// <summary>
/// Destructor
/// </summary>
CDepthBasics::~CDepthBasics()
{
    // clean up Direct2D renderer
    if (m_pDrawDepth)
    {
        delete m_pDrawDepth;
        m_pDrawDepth = NULL;
    }

    if (m_pDepthRGBX)
    {
        delete [] m_pDepthRGBX;
        m_pDepthRGBX = NULL;
    }

    // clean up Direct2D
    SafeRelease(m_pD2DFactory);

    // done with depth frame reader
    //SafeRelease(m_pDepthFrameReader);
	SafeRelease(m_pMultiSourceFrameReader);

    // close the Kinect Sensor
    if (m_pKinectSensor)
    {
        m_pKinectSensor->Close();
    }

    SafeRelease(m_pKinectSensor);
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CDepthBasics::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc;

    // Dialog custom window class
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"DepthBasicsAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        NULL,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CDepthBasics::MessageRouter, 
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);

	// start listening
	StartThreadFunc();
	//std::thread thread1(mythreadtask);
	//socket_init();
	//ReadSocket();
    // Main message loop
    while (WM_QUIT != msg.message)
    {
        Update();
		
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if (hWndApp && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

void CDepthBasics::socket_init_client() {
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wchar_t buffer[40];

	wVersionRequested = MAKEWORD(1, 1);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != NO_ERROR) {
		OutputDebugString(L"Client: WSA Startup Error");
		cout << "WSA Startup Error" << endl;
		return;
	}

	//UDP broadcast
	sockClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockClient == INVALID_SOCKET) {
		wsprintf(buffer, L"client socket failed with error %d\n", WSAGetLastError());
		OutputDebugString(L"WSA Startup Error");
		return;
	}

	addrClient.sin_family = AF_INET;
	addrClient.sin_port = htons(PORT);
	addrClient.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr("192.168.0.199") 

	// bind to any address and certain port
	//note the scope restriction otherwise being considered as std::bind, 
	// see http://stackoverflow.com/questions/6551492/how-to-distinguish-between-bind-in-sys-sockets-h-and-stdbind
	::bind(sockClient, (SOCKADDR *)& addrClient, sizeof(addrClient));

	wsprintf(buffer, L"Client Socket: %d, %d\n", sockClient, WSAGetLastError());
	OutputDebugString(buffer);

	//cout << "Socket Binded to " << HOST << endl;
	/**/
	wchar_t* message = new wchar_t[50];
	u_long nMode = 1; // 1: NON-BLOCKING
	int e = ioctlsocket(sockClient, FIONBIO, &nMode);
	if (e == SOCKET_ERROR)
	{
		OutputDebugString(L"ioctlsocket  SOCKET_ERRORSOCKET_ERRORSOCKET_ERRORSOCKET_ERRORSOCKET_ERROR ");
	}
	
	SetStatusMessage(L"initialzed socket client", 1000, true);
}

void CDepthBasics::socket_init_server(){
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wchar_t buffer[40];

	wVersionRequested = MAKEWORD(1, 1);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != NO_ERROR) {
		OutputDebugString(L"Server: WSA Startup Error");
		cout << "WSA Startup Error" << endl;
		return;
	}

	//UDP broadcast
	sockSrv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockSrv == INVALID_SOCKET) {
		wsprintf(buffer, L"Server: socket failed with error %d\n", WSAGetLastError());
		OutputDebugString(L"WSA Startup Error");
		return;
	}

	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(PORT);
	addrServer.sin_addr.s_addr = INADDR_BROADCAST;

	bool bOpt = true;
	setsockopt(sockSrv, SOL_SOCKET, SO_BROADCAST, (char*)&bOpt, sizeof(bOpt));
	wsprintf(buffer, L"Server Socket: %d %d\n", sockSrv, WSAGetLastError());
	OutputDebugString(buffer);

	SetStatusMessage(L"initialzed socket server", 1000, true);
}

void CDepthBasics::send_signal(char* signal){
	int length = sizeof(signal);
	wchar_t buffer[50];
	wchar_t buf_w[SOCK_BUF_LEN];
	size_t len = strlen(signal);
	mbstowcs_s(&len, buf_w, signal, length);
	sendto(sockSrv, signal, length, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	wsprintf(buffer, L"Sending %s, length: %d, error: %d", buf_w, length, WSAGetLastError());
	SetStatusMessage(buffer, 1000, true);
}

void CDepthBasics::ReadSocket()
{
	// init socket
	socket_init_client(); //listen
	socket_init_server(); //sync sender
	wchar_t* message = new wchar_t[50];	
	wchar_t buf_w[30];
	char *buf = new char[SOCK_BUF_LEN];
	memset(buf, 0, SOCK_BUF_LEN);
	int slen = sizeof(SOCKADDR_IN);
	int recv_len;

	while (1) {
		// slave: listening on channel
		if (!m_bStartSync){
			recv_len = recvfrom(sockClient, buf, SOCK_BUF_LEN, 0, (struct sockaddr *) &addrClient, &slen);	
			if (recv_len != SOCKET_ERROR) {
				buf[recv_len] = '\0';
				// switch on writing
				if (strcmp(buf, "on") == 0) {
					m_bWriteDepthFile = true;
					SetDlgItemText(m_hWnd, IDC_BUTTON_SYNC, L"Stop");
				}
				// switch off writing
				if (strcmp(buf, "off") == 0) {
					m_bWriteDepthFile = false;
					SetDlgItemText(m_hWnd, IDC_BUTTON_SYNC, L"Synchronize");
				}
				// capture only one
				if (strcmp(buf, "cap") == 0) {
					if (!m_Master){
						m_bWriteRGBD=true;
					}
				}
			}
			
			// convert buf to wchar_t			
			size_t length = strlen(buf);
			mbstowcs_s(&length, buf_w, buf, length);
			wsprintf(message, L"Socket %d, recv %d, error code: %d, buffer:%s\n", sockClient, recv_len, WSAGetLastError(), buf_w);
			OutputDebugString(message);
			//SetStatusMessage(message, 1000, true);
		}
			
		Sleep(1000);
	}
}

/// <summary>
/// Main processing function
/// </summary>
void CDepthBasics::Update()
{
	if (!m_pMultiSourceFrameReader)
    {
        return;
    }

	IMultiSourceFrame* pMultiSourceFrame = NULL;
	IDepthFrame* pDepthFrame = NULL;
	IColorFrame* pColorFrame = NULL;

	HRESULT hr = m_pMultiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

	if (SUCCEEDED(hr))
	{
		IDepthFrameReference* pDepthFrameReference = NULL;

		hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
		}

		SafeRelease(pDepthFrameReference);
	}

	if (SUCCEEDED(hr))
	{
		IColorFrameReference* pColorFrameReference = NULL;

		hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameReference->AcquireFrame(&pColorFrame);
		}

		SafeRelease(pColorFrameReference);
	}

	if (SUCCEEDED(hr))
	{
		INT64 nDepthTime = 0;
		IFrameDescription* pDepthFrameDescription = NULL;
		int nDepthWidth = 0;
		int nDepthHeight = 0;
		UINT nDepthBufferSize = 0;
		UINT16 *pDepthBuffer = NULL;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxDistance = 0;

		IFrameDescription* pColorFrameDescription = NULL;
		int nColorWidth = 0;
		int nColorHeight = 0;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nColorBufferSize = 0;
		RGBQUAD *pColorBuffer = NULL;

		IFrameDescription* pBodyIndexFrameDescription = NULL;
		int nBodyIndexWidth = 0;
		int nBodyIndexHeight = 0;
		UINT nBodyIndexBufferSize = 0;
		BYTE *pBodyIndexBuffer = NULL;

		// get depth frame data

		hr = pDepthFrame->get_RelativeTime(&nDepthTime);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Width(&nDepthWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Height(&nDepthHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
		}

		if (SUCCEEDED(hr))
		{
			// In order to see the full range of depth (including the less reliable far field depth)
			// we are setting nDepthMaxDistance to the extreme potential depth threshold
			nDepthMaxDistance = USHRT_MAX;

			// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
			//// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
		}

		// get color frame data
		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Width(&nColorWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Height(&nColorHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
		}

		if (SUCCEEDED(hr))
		{
			if (imageFormat == ColorImageFormat_Bgra)
			{
				hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
			}
			else if (m_pColorRGBX)
			{
				pColorBuffer = m_pColorRGBX;
				nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
				hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
			}
			else
			{
				hr = E_FAIL;
			}
		}

		// get body index frame data
		if (SUCCEEDED(hr))
		{
			//ProcessFrame(nDepthTime, pDepthBuffer, nDepthWidth, nDepthHeight,
			//	pColorBuffer, nColorWidth, nColorHeight,
			//	pBodyIndexBuffer, nBodyIndexWidth, nBodyIndexHeight);

			// write the file
			if (m_bWriteRGBD && pColorBuffer && pDepthBuffer){
				char filename[50];
				sprintf(filename, "capture/%d.bin", ++m_frameCounter);
				std::ofstream fo(filename, std::ofstream::binary);
				fo.write((const char *)pDepthBuffer, (nDepthWidth * nDepthHeight)*sizeof(UINT16));
				fo.write((const char *)pColorBuffer, (nColorWidth * nColorHeight)*sizeof(RGBQUAD));
				fo.close();
				m_bWriteRGBD = false;
				SetStatusMessage(L"Written caputered RGBD Images", 1000, true);
			}

			// process
			ProcessDepth(nDepthTime, pDepthBuffer, nDepthWidth, nDepthHeight, nDepthMinReliableDistance, nDepthMaxDistance);
		}

		SafeRelease(pDepthFrameDescription);
		SafeRelease(pColorFrameDescription);
		SafeRelease(pBodyIndexFrameDescription);
	}

	SafeRelease(pDepthFrame);
	SafeRelease(pColorFrame);
	SafeRelease(pMultiSourceFrame);



	/* Handle the depth frame

    IDepthFrame* pDepthFrame = NULL;	

    HRESULT hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);

	if (SUCCEEDED(hr))
	{
		INT64 nTime = 0;
		IFrameDescription* pFrameDescription = NULL;
		int nWidth = 0;
		int nHeight = 0;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxDistance = 0;
		UINT nBufferSize = 0;
		UINT16 *pBuffer = NULL;

		hr = pDepthFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameDescription->get_Width(&nWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameDescription->get_Height(&nHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
		}

		if (SUCCEEDED(hr))
		{
			// In order to see the full range of depth (including the less reliable far field depth)
			// we are setting nDepthMaxDistance to the extreme potential depth threshold
			nDepthMaxDistance = USHRT_MAX;

			// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
			//// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
		}


		// save both image and depth
		//if (0)


		if (SUCCEEDED(hr))
		{
			ProcessDepth(nTime, pBuffer, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxDistance);
		}

        SafeRelease(pFrameDescription);
    }

    SafeRelease(pDepthFrame);
	*/
}

/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CDepthBasics::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CDepthBasics* pThis = NULL;
    
    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CDepthBasics*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CDepthBasics*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CDepthBasics::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {
            // Bind application window handle
            m_hWnd = hWnd;

            // Init Direct2D
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

            // Create and initialize a new Direct2D image renderer (take a look at ImageRenderer.h)
            // We'll use this to draw the data we receive from the Kinect to the screen
            m_pDrawDepth = new ImageRenderer();
            HRESULT hr = m_pDrawDepth->Initialize(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), m_pD2DFactory, cDepthWidth, cDepthHeight, cDepthWidth * sizeof(RGBQUAD)); 
            if (FAILED(hr))
            {
                SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
            }

            // Get and initialize the default Kinect sensor
            //InitializeDefaultSensor();
			InitializeMultiSourceSensor();
        }
        break;

        // If the titlebar X is clicked, destroy app
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            // Quit the main message pump
            PostQuitMessage(0);
            break;

        // Handle button press
        case WM_COMMAND:
            // If it was for the screenshot control and a button clicked event, save a screenshot next frame 
            if (IDC_BUTTON_SCREENSHOT == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
            {
                m_bSaveScreenshot = true;
            }
			// sync button is clicked, start sending signal and writing file
			if (IDC_BUTTON_SYNC == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
			{
				wchar_t itemText[30];
				GetDlgItemText(hWnd, IDC_BUTTON_SYNC, itemText,30);
				if (wcscmp(itemText, L"Synchronize") == 0){
					m_bStartSync = true;
					SetDlgItemText(hWnd, IDC_BUTTON_SYNC, L"Stop");
					m_bWriteDepthFile = true;
					send_signal("on");
				}
				else{
					m_bStartSync = false;
					SetDlgItemText(hWnd, IDC_BUTTON_SYNC, L"Synchronize");
					// send to peer devices to stop
					send_signal("off");
					m_bWriteDepthFile = false;
				}				
			}
			// capture button is pressed
			if (IDC_BUTTON_CAPTURE == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
			{				
				m_Master = true;
				m_bWriteRGBD = true;
				send_signal("cap");
				OutputDebugString(L"Setting writeRGBD\n");
			}
            break;
    }

    return FALSE;
}

/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CDepthBasics::InitializeDefaultSensor()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_pKinectSensor)
    {
        // Initialize the Kinect and get the depth reader
        IDepthFrameSource* pDepthFrameSource = NULL;

        hr = m_pKinectSensor->Open();
		// depth frame reader
        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
        }

		SafeRelease(pDepthFrameSource);

		/* color frame reader
		IColorFrameSource* pColorFrameSource = NULL;
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
		}		
		SafeRelease(pColorFrameSource);
		*/
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
        SetStatusMessage(L"No ready Kinect found!", 10000, true);
        return E_FAIL;
    }

    return hr;
}

HRESULT CDepthBasics::InitializeMultiSourceSensor(){
	HRESULT hr;

	hr = GetDefaultKinectSensor(&m_pKinectSensor);
	if (FAILED(hr))
	{
		return hr;
	}
	
	if (m_pKinectSensor)
	{
		// Initialize the Kinect and get the frame reader

		hr = m_pKinectSensor->Open();

		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->OpenMultiSourceFrameReader(
				FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color | FrameSourceTypes::FrameSourceTypes_BodyIndex,
				&m_pMultiSourceFrameReader);
		}
	}

	if (!m_pKinectSensor || FAILED(hr))
	{
		SetStatusMessage(L"No ready Kinect found!", 10000, true);
		return E_FAIL;
	}

	return hr;
}

/*
void CDepthBasics::WriteMatToFile(cv::Mat& m, const char* filename)
{
    ofstream fout(filename);

    if(!fout)
    {
        cout<<"File Not Opened"<<endl;  return;
    }

    for(int i=0; i<m.rows; i++)
    {
        for(int j=0; j<m.cols; j++)
        {
            fout<<m.at<float>(i,j)<<"\t";
        }
        fout<<endl;
    }

    fout.close();
}*/

/// <summary>
/// Handle new depth data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// <param name="nMinDepth">minimum reliable depth</param>
/// <param name="nMaxDepth">maximum reliable depth</param>
/// </summary>
void CDepthBasics::ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight, USHORT nMinDepth, USHORT nMaxDepth)
{
    if (m_hWnd)
    {
        if (!m_nStartTime)
        {
            m_nStartTime = nTime;
        }

        double fps = 0.0;

        LARGE_INTEGER qpcNow = {0};
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nLastCounter)
                {
                    m_nFramesSinceUpdate++;
                    fps = m_fFreq * m_nFramesSinceUpdate / double(qpcNow.QuadPart - m_nLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[64];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" FPS = %0.2f    Time = %I64d", fps, (nTime - m_nStartTime));

        if (SetStatusMessage(szStatusMessage, 1000, false))
        {
            m_nLastCounter = qpcNow.QuadPart;
            m_nFramesSinceUpdate = 0;
        }

		//writing to file
		/*
		cv::Mat DepthFrame(nHeight, nWidth, CV_16UC1, const_cast<unsigned short *>(pBuffer));
		cv::Mat DepthFrame32FC1(nHeight, nWidth, CV_32FC1);
		DepthFrame.convertTo(DepthFrame32FC1, CV_32FC1, 1.0 / (double)nMaxDepth);
		//cv::imshow("DepthFrame", DepthFrame32FC1);
		
		WriteMatToFile(DepthFrame32FC1, filename);
		*/
    }

    // Make sure we've received valid data
    if (m_pDepthRGBX && pBuffer && (nWidth == cDepthWidth) && (nHeight == cDepthHeight))
    {
        RGBQUAD* pRGBX = m_pDepthRGBX;

        // end pixel is start + width*height - 1
        const UINT16* pBufferEnd = pBuffer + (nWidth * nHeight);

		// write to file
		if (m_bWriteDepthFile){
			char filename[50];
			sprintf(filename, "data/%d.bin", ++frameCounter);
			std::ofstream o1(filename, std::ofstream::binary);
			o1.write((const char *)pBuffer, (nWidth * nHeight)*sizeof(UINT16));
			o1.close();
		}

        while (pBuffer < pBufferEnd)
        {
            USHORT depth = *pBuffer;

            // To convert to a byte, we're discarding the most-significant
            // rather than least-significant bits.
            // We're preserving detail, although the intensity will "wrap."
            // Values outside the reliable depth range are mapped to 0 (black).

            // Note: Using conditionals in this loop could degrade performance.
            // Consider using a lookup table instead when writing production code.
            BYTE intensity = static_cast<BYTE>((depth >= nMinDepth) && (depth <= nMaxDepth) ? (depth % 256) : 0);

            pRGBX->rgbRed   = intensity;
            pRGBX->rgbGreen = intensity;
            pRGBX->rgbBlue  = intensity;
			
            ++pRGBX;
            ++pBuffer;
        }


        // Draw the data with Direct2D
        m_pDrawDepth->Draw(reinterpret_cast<BYTE*>(m_pDepthRGBX), cDepthWidth * cDepthHeight * sizeof(RGBQUAD));

        if (m_bSaveScreenshot)
        {
            WCHAR szScreenshotPath[MAX_PATH];

            // Retrieve the path to My Photos
            GetScreenshotFileName(szScreenshotPath, _countof(szScreenshotPath));

            // Write out the bitmap to disk
            HRESULT hr = SaveBitmapToFile(reinterpret_cast<BYTE*>(m_pDepthRGBX), nWidth, nHeight, sizeof(RGBQUAD) * 8, szScreenshotPath);

            WCHAR szStatusMessage[64 + MAX_PATH];
            if (SUCCEEDED(hr))
            {
                // Set the status bar to show where the screenshot was saved
                StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L"Screenshot saved to %s", szScreenshotPath);
            }
            else
            {
                StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L"Failed to write screenshot to %s", szScreenshotPath);
            }

            SetStatusMessage(szStatusMessage, 5000, true);

            // toggle off so we don't save a screenshot again next frame
            m_bSaveScreenshot = false;
        }
    }
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
/// <param name="showTimeMsec">time in milliseconds to ignore future status messages</param>
/// <param name="bForce">force status update</param>
bool CDepthBasics::SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce)
{
    INT64 now = GetTickCount64();

    if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
    {
        SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
        m_nNextStatusTime = now + nShowTimeMsec;

        return true;
    }

    return false;
}

/// <summary>
/// Get the name of the file where screenshot will be stored.
/// </summary>
/// <param name="lpszFilePath">string buffer that will receive screenshot file name.</param>
/// <param name="nFilePathSize">number of characters in lpszFilePath string buffer.</param>
/// <returns>
/// S_OK on success, otherwise failure code.
/// </returns>
HRESULT CDepthBasics::GetScreenshotFileName(_Out_writes_z_(nFilePathSize) LPWSTR lpszFilePath, UINT nFilePathSize)
{
    WCHAR* pszKnownPath = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &pszKnownPath);

    if (SUCCEEDED(hr))
    {
        // Get the time
        WCHAR szTimeString[MAX_PATH];
        GetTimeFormatEx(NULL, 0, NULL, L"hh'-'mm'-'ss", szTimeString, _countof(szTimeString));

        // File name will be KinectScreenshotDepth-HH-MM-SS.bmp
        StringCchPrintfW(lpszFilePath, nFilePathSize, L"%s\\KinectScreenshot-Depth-%s.bmp", pszKnownPath, szTimeString);
    }

    if (pszKnownPath)
    {
        CoTaskMemFree(pszKnownPath);
    }

    return hr;
}

/// <summary>
/// Save passed in image data to disk as a bitmap
/// </summary>
/// <param name="pBitmapBits">image data to save</param>
/// <param name="lWidth">width (in pixels) of input image data</param>
/// <param name="lHeight">height (in pixels) of input image data</param>
/// <param name="wBitsPerPixel">bits per pixel of image data</param>
/// <param name="lpszFilePath">full file path to output bitmap to</param>
/// <returns>indicates success or failure</returns>
HRESULT CDepthBasics::SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath)
{
    DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

    BITMAPINFOHEADER bmpInfoHeader = {0};

    bmpInfoHeader.biSize        = sizeof(BITMAPINFOHEADER);  // Size of the header
    bmpInfoHeader.biBitCount    = wBitsPerPixel;             // Bit count
    bmpInfoHeader.biCompression = BI_RGB;                    // Standard RGB, no compression
    bmpInfoHeader.biWidth       = lWidth;                    // Width in pixels
    bmpInfoHeader.biHeight      = -lHeight;                  // Height in pixels, negative indicates it's stored right-side-up
    bmpInfoHeader.biPlanes      = 1;                         // Default
    bmpInfoHeader.biSizeImage   = dwByteCount;               // Image size in bytes

    BITMAPFILEHEADER bfh = {0};

    bfh.bfType    = 0x4D42;                                           // 'M''B', indicates bitmap
    bfh.bfOffBits = bmpInfoHeader.biSize + sizeof(BITMAPFILEHEADER);  // Offset to the start of pixel data
    bfh.bfSize    = bfh.bfOffBits + bmpInfoHeader.biSizeImage;        // Size of image + headers

    // Create the file on disk to write to
    HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // Return if error opening file
    if (NULL == hFile) 
    {
        return E_ACCESSDENIED;
    }

    DWORD dwBytesWritten = 0;
    
    // Write the bitmap file header
    if (!WriteFile(hFile, &bfh, sizeof(bfh), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }
    
    // Write the bitmap info header
    if (!WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }
    
    // Write the RGB Data
    if (!WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }    

    // Close the file
    CloseHandle(hFile);
    return S_OK;
}
