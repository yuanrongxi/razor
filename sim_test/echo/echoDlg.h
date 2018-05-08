
// echoDlg.h : header file
//

#pragma once
#include "afxwin.h"

#include "video_device.h"
#include "video_view.h"
#include "sim_object.h"

// CechoDlg dialog
class CechoDlg : public CDialogEx
{
// Construction
public:
	CechoDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_ECHO_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


protected:
	LRESULT		OnConnectSucc(WPARAM wparam, LPARAM lparam);
	LRESULT		OnConnectFailed(WPARAM wparam, LPARAM lparam);
	LRESULT		OnTimeout(WPARAM wparam, LPARAM lparam);
	LRESULT		OnDisconnected(WPARAM wparam, LPARAM lparam);
	
	LRESULT		OnStartPlay(WPARAM wparam, LPARAM lparam);
	LRESULT		OnStopPlay(WPARAM wparam, LPARAM lparam);

	LRESULT		OnChangeBitrate(WPARAM wparam, LPARAM lparam);
	LRESULT		OnNetInterrupt(WPARAM wparam, LPARAM lparam);
	LRESULT		OnNetRecover(WPARAM wparam, LPARAM lparam);

	LRESULT		OnStateInfo(WPARAM wparam, LPARAM lparam);

// Implementation
protected:
	HICON m_hIcon;
	VideoViewThread m_view;
	BOOL m_viewing;
	CFVideoRecorder* m_viRecorder;
	CFVideoPlayer* m_viPlayer;

	SimFramework* m_frame;
	BOOL m_connected;
	BOOL m_playing;
	BOOL m_recording;

	void InitVideoDevices();
	void CloseAll();

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_cbxDevice;
	CStatic m_srcVideo;
	afx_msg void OnDestroy();
	afx_msg void OnBnClickedBtnview();
	CString m_strDev;
	CButton m_btnView;
	CStatic m_dstVideo;
	afx_msg void OnBnClickedBtnconnect();
	CString m_strIP;
	int m_iPort;
	int m_iUser;
	CString m_strState;
	CButton m_btnEcho;
	CString m_strInfo;
};
