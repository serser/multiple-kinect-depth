//------------------------------------------------------------------------------
// <copyright file="DepthBasics.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include "resource.h"
#include "ImageRenderer.h"
//#include <opencv2/opencv.hpp>
#include <thread>
#include <Winsock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "WS2_32")
#define HOST "192.168.0.199"
#define PORT 18001
#define SOCK_BUF_LEN 4

class CDepthBasics
{
    static const int        cDepthWidth  = 512;
    static const int        cDepthHeight = 424;

public:
    /// <summary>
    /// Constructor
    /// </summary>
    CDepthBasics();

    /// <summary>
    /// Destructor
    /// </summary>
    ~CDepthBasics();

    /// <summary>
    /// Handles window messages, passes most to the class instance to handle
    /// </summary>
    /// <param name="hWnd">window message is for</param>
    /// <param name="uMsg">message</param>
    /// <param name="wParam">message data</param>
    /// <param name="lParam">additional message data</param>
    /// <returns>result of message processing</returns>
    static LRESULT CALLBACK MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /// <summary>
    /// Handle windows messages for a class instance
    /// </summary>
    /// <param name="hWnd">window message is for</param>
    /// <param name="uMsg">message</param>
    /// <param name="wParam">message data</param>
    /// <param name="lParam">additional message data</param>
    /// <returns>result of message processing</returns>
    LRESULT CALLBACK        DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /// <summary>
    /// Creates the main window and begins processing
    /// </summary>
    /// <param name="hInstance"></param>
    /// <param name="nCmdShow"></param>
    int                     Run(HINSTANCE hInstance, int nCmdShow);

private:
    HWND                    m_hWnd;
    INT64                   m_nStartTime;
    INT64                   m_nLastCounter;
    double                  m_fFreq;
    INT64                   m_nNextStatusTime;
    DWORD                   m_nFramesSinceUpdate;
    bool                    m_bSaveScreenshot;
	bool					m_bStartSync;
	bool					m_bWriteDepthFile;
	bool					m_bWriteRGBD;
	bool					m_Master;

    // sock thread
	HANDLE                  m_sThread;
	DWORD                   m_sthreadId;
	SOCKET					sockClient;
	SOCKADDR_IN				addrClient;
	SOCKET					sockSrv;
	SOCKADDR_IN				addrServer;

	// caputure-frame
	int						m_frameCounter;

    // Current Kinect
    IKinectSensor*          m_pKinectSensor;

    // Depth reader
	IMultiSourceFrameReader*m_pMultiSourceFrameReader;
    IDepthFrameReader*      m_pDepthFrameReader;
	IColorFrameReader*      m_pColorFrameReader;

    // Direct2D
    ImageRenderer*          m_pDrawDepth;
    ID2D1Factory*           m_pD2DFactory;
    RGBQUAD*                m_pDepthRGBX;
	RGBQUAD*                m_pColorRGBX;

    /// <summary>
    /// Main processing function
    /// </summary>
    void                    Update();

    /// <summary>
    /// Initializes the default Kinect sensor
    /// </summary>
    /// <returns>S_OK on success, otherwise failure code</returns>
    HRESULT                 InitializeDefaultSensor();
	HRESULT					InitializeMultiSourceSensor();

    /// <summary>
    /// Handle new depth data
    /// <param name="nTime">timestamp of frame</param>
    /// <param name="pBuffer">pointer to frame data</param>
    /// <param name="nWidth">width (in pixels) of input image data</param>
    /// <param name="nHeight">height (in pixels) of input image data</param>
    /// <param name="nMinDepth">minimum reliable depth</param>
    /// <param name="nMaxDepth">maximum reliable depth</param>
    /// </summary>
    void                    ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nHeight, int nWidth, USHORT nMinDepth, USHORT nMaxDepth);

    /// <summary>
    /// Set the status bar message
    /// </summary>
    /// <param name="szMessage">message to display</param>
    /// <param name="nShowTimeMsec">time in milliseconds for which to ignore future status messages</param>
    /// <param name="bForce">force status update</param>
    bool                    SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce);

    /// <summary>
    /// Get the name of the file where screenshot will be stored.
    /// </summary>
    /// <param name="lpszFilePath">string buffer that will receive screenshot file name.</param>
    /// <param name="nFilePathSize">number of characters in lpszFilePath string buffer.</param>
    /// <returns>
    /// S_OK on success, otherwise failure code.
    /// </returns>
    HRESULT                 GetScreenshotFileName(_Out_writes_z_(nFilePathSize) LPWSTR lpszFilePath, UINT nFilePathSize);

    /// <summary>
    /// Save passed in image data to disk as a bitmap
    /// </summary>
    /// <param name="pBitmapBits">image data to save</param>
    /// <param name="lWidth">width (in pixels) of input image data</param>
    /// <param name="lHeight">height (in pixels) of input image data</param>
    /// <param name="wBitsPerPixel">bits per pixel of image data</param>
    /// <param name="lpszFilePath">full file path to output bitmap to</param>
    /// <returns>indicates success or failure</returns>
    HRESULT                 SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath);
  	//void					WriteMatToFile(cv::Mat& m, const char* filename);
	void					ReadSocket();
	void					socket_init_client();
	void					socket_init_server();
	void					send_signal(char* signal);
	static UINT ThreadFunc(LPVOID param)
	{
		CDepthBasics* This = (CDepthBasics*)param;
		This->ReadSocket(); // call a member function
		return 0;
	}
	VOID StartThreadFunc()
	{
		::CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadFunc,
			(LPVOID)this, 0, 0);
	}
};

