#ifndef _ONI_SAMPLE_VIEWER_H_
#define _ONI_SAMPLE_VIEWER_H_

#include <OpenNI.h>
#include <NiTE.h>
#include "GrabDetector/GrabDetector.h"

#define MAX_DEPTH 10000

enum DisplayModes
{
	DISPLAY_MODE_OVERLAY,
	DISPLAY_MODE_DEPTH,
	DISPLAY_MODE_IMAGE
};

class SampleViewer
{
public:
	SampleViewer(const char* strSampleName, openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color);
	virtual ~SampleViewer();

	virtual openni::Status Init(int argc, char **argv);
	virtual openni::Status Run();	//Does not return

	class GrabEventListener : public PSLabs::IGrabEventListener
	{
	public:
		GrabEventListener(SampleViewer* sampleViewer) : m_sampleViewer(sampleViewer)
		{

		}
		virtual void DLL_CALL ProcessGrabEvent( const EventParams& params )
		{
			m_sampleViewer->ProcessGrabEvent(params.Type);
		}

		SampleViewer* m_sampleViewer;
	};

protected:
	virtual void Display();
	virtual void DisplayPostDraw(){};	// Overload to draw over the screen image

	virtual void OnKey(unsigned char key, int x, int y);

	virtual openni::Status InitOpenGL(int argc, char **argv);
	void InitOpenGLHooks();

	openni::VideoFrameRef		m_depthFrame;
	openni::VideoFrameRef		m_colorFrame;

	openni::Device&					m_device;
	openni::VideoStream&		m_depthStream;
	openni::VideoStream&		m_colorStream;
	openni::VideoStream**		m_streams;
	
private:
	SampleViewer(const SampleViewer&);
	SampleViewer& operator=(SampleViewer&);

	static SampleViewer* ms_self;
	static void GLUTIdle();
	static void GLUTDisplay();
	static void GLUTKeyboard(unsigned char key, int x, int y);

	void DrawHandPoint(float x, float y, float z);
	void DrawHandStatus(PSLabs::IGrabEventListener::GrabEventType status, float handX, float handY, float handZ );
	void ProcessGrabEvent( PSLabs::IGrabEventListener::GrabEventType Type );
	void DrawExposureStatus(void);
	void UpdateAlgorithm(void);
	void UpdateNiTETrackers(bool* handLost, bool* gestureComplete, bool* handTracked, float* handX, float* handY, float* handZ, bool* userLost, bool* userTracked,float* userX, float* userY, float* userZ);
	void DrawDetectorInfo(void);
	openni::Status InitGrabDetector(void);
	openni::Status InitNiTE(void);
	
	float			m_pDepthHist[MAX_DEPTH];
	char			m_strSampleName[ONI_MAX_STR];
	unsigned int		m_nTexMapX;
	unsigned int		m_nTexMapY;
	DisplayModes		m_eViewState;
	openni::RGB888Pixel*	m_pTexMap;
	int			m_width;
	int			m_height;

	PSLabs::IGrabDetector* m_grabDetector;
	GrabEventListener* m_grabListener;

	nite::HandTracker m_handTracker;
	nite::HandId m_lastHandID;
	bool toExit;
	bool m_optimalExposure;

	nite::UserTracker m_userTracker;
	nite::UserId m_lastUserID;
};


#endif // _ONI_SAMPLE_VIEWER_H_
