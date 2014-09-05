// Undeprecate CRT functions
#ifndef _CRT_SECURE_NO_DEPRECATE 
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "Viewer.h"



#if (ONI_PLATFORM == ONI_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "..\OniSampleUtilities.h"

#define GL_WIN_SIZE_X	640
#define GL_WIN_SIZE_Y	480
#define TEXTURE_SIZE	512

#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

SampleViewer* SampleViewer::ms_self = NULL;

void SampleViewer::GLUTIdle()
{
	glutPostRedisplay();
}
void SampleViewer::GLUTDisplay()
{
	SampleViewer::ms_self->Display();
}
void SampleViewer::GLUTKeyboard(unsigned char key, int x, int y)
{
	SampleViewer::ms_self->OnKey(key, x, y);
}


SampleViewer::SampleViewer(const char* strSampleName, openni::Device& device, openni::VideoStream& depth, openni::VideoStream& color) :
m_device(device), m_depthStream(depth), m_colorStream(color), m_streams(NULL), m_eViewState(DEFAULT_DISPLAY_MODE), m_pTexMap(NULL), m_grabListener(NULL), m_grabDetector(NULL)

{
	ms_self = this;
	strncpy(m_strSampleName, strSampleName, ONI_MAX_STR);
}
SampleViewer::~SampleViewer()
{
	delete[] m_pTexMap;

	ms_self = NULL;

	if (m_streams != NULL)
	{
		delete []m_streams;
	}
	
	if(m_grabDetector!=NULL)
	{
		PSLabs::ReleaseGrabDetector(m_grabDetector);
	}

	delete m_grabListener;
}

//Initializes new GrabDetector object and events
openni::Status SampleViewer::InitGrabDetector( void )
{
	//Create object
	m_grabDetector = PSLabs::CreateGrabDetector(m_device);
	if(m_grabDetector == NULL || m_grabDetector->GetLastEvent(NULL) != openni::STATUS_OK)
	{
		printf("Error - cannot initialize grab detector: status %d \n", m_grabDetector->GetLastEvent(NULL));
		return openni::STATUS_ERROR;
	}

	//Initialize GrabDetector
	m_grabListener = new GrabEventListener(this);
	m_grabDetector->AddListener(m_grabListener);

	return openni::STATUS_OK;
}

openni::Status SampleViewer::InitNiTE( void )
{
	//Initialize NiTE
	nite::Status niteStat = nite::STATUS_OK;
	nite::Status userStat = nite::STATUS_OK;

	niteStat = m_handTracker.create(&m_device);
	if(niteStat != nite::STATUS_OK)
	{
		printf("Error initializing NiTE hand tracker: %d", (int)niteStat);
		return openni::STATUS_ERROR;
	}

	userStat = m_userTracker.create(&m_device);
	if (userStat != nite::STATUS_OK)
	{
		printf("Error initializing NiTE user tracker: %d", (int)userStat);
		return openni::STATUS_ERROR;
	}

	//Detect Skeleton
	m_userTracker.startSkeletonTracking(nite::SKELETON_CALIBRATING);

	//Detect Click/Wave gestures
	m_handTracker.startGestureDetection(nite::GESTURE_CLICK);
	m_handTracker.startGestureDetection(nite::GESTURE_WAVE);

	return openni::STATUS_OK;

}
openni::Status SampleViewer::Init(int argc, char **argv)
{
	openni::VideoMode depthVideoMode;
	openni::VideoMode colorVideoMode;

	if (m_depthStream.isValid() && m_colorStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		colorVideoMode = m_colorStream.getVideoMode();

		int depthWidth = depthVideoMode.getResolutionX();
		int depthHeight = depthVideoMode.getResolutionY();
		int colorWidth = colorVideoMode.getResolutionX();
		int colorHeight = colorVideoMode.getResolutionY();

		if (depthWidth == colorWidth &&
			depthHeight == colorHeight)
		{
			m_width = depthWidth;
			m_height = depthHeight;
		}
		else
		{
			printf("Error - expect color and depth to be in same resolution: D: %dx%d, C: %dx%d\n",
				depthWidth, depthHeight,
				colorWidth, colorHeight);
			return openni::STATUS_ERROR;
		}
	}
	else if (m_depthStream.isValid())
	{
		depthVideoMode = m_depthStream.getVideoMode();
		m_width = depthVideoMode.getResolutionX();
		m_height = depthVideoMode.getResolutionY();
	}
	else if (m_colorStream.isValid())
	{
		colorVideoMode = m_colorStream.getVideoMode();
		m_width = colorVideoMode.getResolutionX();
		m_height = colorVideoMode.getResolutionY();
	}
	else
	{
		printf("Error - expects at least one of the streams to be valid...\n");
		return openni::STATUS_ERROR;
	}

	m_streams = new openni::VideoStream*[2];
	m_streams[0] = &m_depthStream;
	m_streams[1] = &m_colorStream;

	// Texture map init
	m_nTexMapX = MIN_CHUNKS_SIZE(m_width, TEXTURE_SIZE);
	m_nTexMapY = MIN_CHUNKS_SIZE(m_height, TEXTURE_SIZE);
	m_pTexMap = new openni::RGB888Pixel[m_nTexMapX * m_nTexMapY];

	
	if(InitGrabDetector() != openni::STATUS_OK)
		return openni::STATUS_ERROR;

	if(InitNiTE() != openni::STATUS_OK)
		return openni::STATUS_ERROR;

	m_optimalExposure = false;

	return InitOpenGL(argc, argv);

}
openni::Status SampleViewer::Run()	//Does not return
{
	try{
		glutMainLoop();
	}catch(...)
	{
		printf("Exited GLUT main loop");
	}

	return openni::STATUS_OK;
}

//This function updates the NiTE tracker and gesture detection and fills their results in the given arguments
void SampleViewer::UpdateNiTETrackers(bool* handLost, bool* gestureComplete, bool* handTracked, float* handX, float* handY, float* handZ, bool* userLost, bool* userTracked, float* userX, float* userY, float* userZ)
{
	 *handLost = false;
	 *gestureComplete = false;
	 *handTracked = false;
	 *userLost = false;
	 *userTracked = false;

	//Read hand frame from NiTE
	nite::HandTrackerFrameRef handTrackerFrame;
	nite::UserTrackerFrameRef userTrackerFrame;
	nite::UserTracker userTracker;

	if (m_handTracker.readFrame(&handTrackerFrame) != nite::STATUS_OK)
	{
		printf("readFrame failed\n");
		return;
	}
	
	//Get gestures from NiTE
	const nite::Array<nite::GestureData>& gestures = handTrackerFrame.getGestures();
	
	for (int i = 0; i < gestures.getSize(); ++i)
	{
		if (gestures[i].isComplete())
		{
			const nite::Point3f& position = gestures[i].getCurrentPosition();
			printf("Gesture %d at (%f,%f,%f) Hand Id %d\n", gestures[i].getType(), position.x, position.y, position.z);

			//Start hand tracker
			m_handTracker.startHandTracking(gestures[i].getCurrentPosition(), &m_lastHandID);

			//Update data
			*gestureComplete = true;
		 	*handX = position.x;
			*handY = position.y;
			*handZ = position.z;
		}
	}
	//Track user and update position from NiTE
	const nite::Array<nite::UserData>&	users = userTrackerFrame.getUsers();
	for (int i = 0; i < users.getSize(); ++i)
	{
		const nite::UserData& user = users[i];
		if (!user.isVisible())
		{
			printf("Lost user %d\n", user.getId());
			nite::UserId id = user.getId();
			*userLost = true;
		}
		else
		{
			if (user.isNew())
			{
				printf("Found user %d\n", user.getId());
			}
		
			if (!user.getSkeleton().getState() == nite::SKELETON_TRACKED)
			{
				userTracker.startSkeletonTracking(user.getId());
			}

			printf("%d. (%5.2f, %5.2f, %5.2f)\n", user.getId(),	user.getCenterOfMass().x, user.getCenterOfMass().y, user.getCenterOfMass().z);
			//Update user detector
			*userTracked = true;
			*userX = user.getCenterOfMass().x;
			*userY = user.getCenterOfMass().y;
			*userZ = user.getCenterOfMass().z;
		}
	}

	//Track hand and update position from NiTE
	const nite::Array<nite::HandData>& hands = handTrackerFrame.getHands();
	for (int i = 0; i < hands.getSize(); ++i)
	{
		const nite::HandData& hand = hands[i];
		if (!hand.isTracking())
		{
			printf("Lost hand %d\n", hand.getId());
			nite::HandId id = hand.getId();
			*handLost = true;
		}
		else
		{
			if (hand.isNew())
			{
				printf("Found hand %d\n", hand.getId());
			}
			
		printf("%d. (%5.2f, %5.2f, %5.2f)\n", hand.getId(),hand.getPosition().x, hand.getPosition().y,hand.getPosition().z);
			//Update grab detector
			*handTracked = true;
			*handX = hand.getPosition().x;
			*handY = hand.getPosition().y;
			*handZ = hand.getPosition().z;
		}
	}
}


//This function updates the algorithms after a new frame has been read
void SampleViewer::UpdateAlgorithm(void)
{
	bool handLost = false, gestureComplete = false, handTracked = false;
	float handX, handY, handZ;

	//User tracking additions
	bool userLost = false, userTracked = false;
	float userX, userY, userZ;

	//Update NiTE trackers and get their result
	UpdateNiTETrackers(&handLost, &gestureComplete, &handTracked, &handX, &handY, &handZ, &userLost, &userTracked, &userX, &userY, &userZ);
	
	if(m_grabDetector != NULL)
	{
		//If the hand is lost, we need to reset the grab detector
		if(handLost)
			m_grabDetector->Reset();
		//If a gesture is just complete, or the hand is already being tracked, we have valid coordinates and can set them to the detector
		else if(gestureComplete || handTracked)
			m_grabDetector->SetHandPosition(handX, handY, handZ);

		//Update algorithm with the newly read frames. We prefer both frames, but can work only with one
		if(m_depthFrame.isValid() && m_colorFrame.isValid())
			m_grabDetector->UpdateFrame(m_depthFrame, m_colorFrame);
		else if(m_depthFrame.isValid())
			m_grabDetector->UpdateFrame(m_depthFrame);
	}
}

void SampleViewer::Display()
{
	int changedIndex = 0;
	openni::Status rc = openni::STATUS_OK;
	
	//Read frames
	rc = openni::OpenNI::waitForAnyStream(m_streams, 2, &changedIndex);
	if (rc != openni::STATUS_OK)
	{
		printf("Wait failed\n");
		return;
	}
	m_depthStream.readFrame(&m_depthFrame);
	if(m_colorStream.isValid())
		m_colorStream.readFrame(&m_colorFrame);


	//Update algorithm
	UpdateAlgorithm();


	//Now we draw the data

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, GL_WIN_SIZE_X, GL_WIN_SIZE_Y, 0, -1.0, 1.0);

	if (m_depthFrame.isValid())
	{
		calculateHistogram(m_pDepthHist, MAX_DEPTH, m_depthFrame);
	}

	memset(m_pTexMap, 0, m_nTexMapX*m_nTexMapY*sizeof(openni::RGB888Pixel));

	// check if we need to draw image frame to texture
	if ((m_eViewState == DISPLAY_MODE_OVERLAY ||
		m_eViewState == DISPLAY_MODE_IMAGE) && m_colorFrame.isValid())
	{
		const openni::RGB888Pixel* pImageRow = (const openni::RGB888Pixel*)m_colorFrame.getData();
		openni::RGB888Pixel* pTexRow = m_pTexMap + m_colorFrame.getCropOriginY() * m_nTexMapX;
		int rowSize = m_colorFrame.getStrideInBytes() / sizeof(openni::RGB888Pixel);

		for (int y = 0; y < m_colorFrame.getHeight(); ++y)
		{
			const openni::RGB888Pixel* pImage = pImageRow;
			openni::RGB888Pixel* pTex = pTexRow + m_colorFrame.getCropOriginX();

			for (int x = 0; x < m_colorFrame.getWidth(); ++x, ++pImage, ++pTex)
			{
				*pTex = *pImage;
			}

			pImageRow += rowSize;
			pTexRow += m_nTexMapX;
		}
	}
	
	// check if we need to draw depth frame to texture
	if ((m_eViewState == DISPLAY_MODE_OVERLAY ||
		m_eViewState == DISPLAY_MODE_DEPTH) && m_depthFrame.isValid())
	{
		const openni::DepthPixel* pDepthRow = (const openni::DepthPixel*)m_depthFrame.getData();
		openni::RGB888Pixel* pTexRow = m_pTexMap + m_depthFrame.getCropOriginY() * m_nTexMapX;
		int rowSize = m_depthFrame.getStrideInBytes() / sizeof(openni::DepthPixel);

		for (int y = 0; y < m_depthFrame.getHeight(); ++y)
		{
			const openni::DepthPixel* pDepth = pDepthRow;
			openni::RGB888Pixel* pTex = pTexRow + m_depthFrame.getCropOriginX();

			for (int x = 0; x < m_depthFrame.getWidth(); ++x, ++pDepth, ++pTex)
			{
				if (*pDepth != 0)
				{
					int nHistValue = m_pDepthHist[*pDepth];
					pTex->r = nHistValue;
					pTex->g = nHistValue;
					pTex->b = 0;
				}
			}

			pDepthRow += rowSize;
			pTexRow += m_nTexMapX;
		}
	}

	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_nTexMapX, m_nTexMapY, 0, GL_RGB, GL_UNSIGNED_BYTE, m_pTexMap);

	// Display the OpenGL texture map
	glColor4f(1,1,1,1);

	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	int nXRes = m_width;
	int nYRes = m_height;

	// upper left
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	// upper right
	glTexCoord2f((float)nXRes/(float)m_nTexMapX, 0);
	glVertex2f(GL_WIN_SIZE_X, 0);
	// bottom right
	glTexCoord2f((float)nXRes/(float)m_nTexMapX, (float)nYRes/(float)m_nTexMapY);
	glVertex2f(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	// bottom left
	glTexCoord2f(0, (float)nYRes/(float)m_nTexMapY);
	glVertex2f(0, GL_WIN_SIZE_Y);

	glEnd();
	glDisable(GL_TEXTURE_2D);
	
	DrawDetectorInfo();

	// Swap the OpenGL display buffers
	glutSwapBuffers();
}

#ifndef USE_GLES
void glPrintString(void *font, const char *str)
{
	int i,l = (int)strlen(str);

	for(i=0; i<l; i++)
	{   
		glutBitmapCharacter(font,*str++);
	}   
}
#endif

//This function draws the detector information - the hand point/status and exposure status
void SampleViewer::DrawDetectorInfo(void)
{
	if(m_grabDetector == NULL)
		return;

	PSLabs::IGrabEventListener::EventParams grabStatus;
	m_grabDetector->GetLastEvent(&grabStatus);

	float handX,handY,handZ;
	if(m_grabDetector->GetHandPosition(&handX,&handY,&handZ) == openni::STATUS_OK)
	{
		openni::CoordinateConverter::convertWorldToDepth(m_depthStream, handX, handY, handZ, &handX, &handY, &handZ);
		DrawHandPoint(handX,handY,handZ);
		DrawHandStatus(grabStatus.Type, handX,handY,handZ);
	}
	//If hand position is not valid we just write its last status
	else
	{
		DrawHandStatus(grabStatus.Type, 0,36,0);
	}

	DrawExposureStatus();
}

void SampleViewer::DrawHandStatus( PSLabs::IGrabEventListener::GrabEventType status, float handX, float handY, float handZ )
{
	handX *= GL_WIN_SIZE_X / m_width;
	handY *= GL_WIN_SIZE_Y / m_height;

	glColor3f(0.0f, 0.0f, 1.0f);
	glRasterPos2f(handX, handY);

	if(status == PSLabs::IGrabEventListener::GRAB_EVENT)
		glPrintString(GLUT_BITMAP_HELVETICA_18, "GRAB");
	else if(status == PSLabs::IGrabEventListener::RELEASE_EVENT)
		glPrintString(GLUT_BITMAP_HELVETICA_18, "RELEASE");
	else if(status == PSLabs::IGrabEventListener::NO_EVENT)
		glPrintString(GLUT_BITMAP_HELVETICA_18, "NO EVENT");
}

void SampleViewer::DrawExposureStatus(void)
{
	if(m_optimalExposure)
		glColor3f(0.0f, 1.0f, 0.0f);
	else
		glColor3f(1.0f, 0.0f, 0.0f);

	glRasterPos2i(0, 18);
	glPrintString(GLUT_BITMAP_HELVETICA_18, "Optimal Exposure");
}


void SampleViewer::DrawHandPoint(float x, float y, float z)
{
	
	x *= GL_WIN_SIZE_X / m_width;
	y *= GL_WIN_SIZE_Y / m_height;

	glColor3f(0.0f,1.0f,0.0f);
	glPointSize(12);
	
	glBegin(GL_POINTS);
	glVertex2f(x,y);
	glEnd();
}

void SampleViewer::OnKey(unsigned char key, int /*x*/, int /*y*/)
{
	switch (key)
	{
	case 27:
		m_depthStream.stop();
		m_colorStream.stop();
		m_depthStream.destroy();
		m_colorStream.destroy();

		m_handTracker.destroy();

		PSLabs::ReleaseGrabDetector(m_grabDetector); //delete m_grabDetector
		m_grabDetector = NULL;
		
		m_device.close();

		nite::NiTE::shutdown();
		openni::OpenNI::shutdown();
		
//		throw "ExitClicked";
		exit (1);
	case '1':
		m_eViewState = DISPLAY_MODE_OVERLAY;
		break;
	case '2':
		m_eViewState = DISPLAY_MODE_DEPTH;
		break;
	case '3':
		m_eViewState = DISPLAY_MODE_IMAGE;
		break;
	case 'm':
		m_depthStream.setMirroringEnabled(!m_depthStream.getMirroringEnabled());
		m_colorStream.setMirroringEnabled(!m_colorStream.getMirroringEnabled());
		break;

	case 'e':
		m_optimalExposure = !m_optimalExposure;
		m_grabDetector->SetOptimalExposure(m_optimalExposure, m_depthStream._getHandle(), m_colorStream._getHandle());
		break;

	case 't':
		m_handTracker.stopHandTracking(m_lastHandID);
		m_grabDetector->Reset();
		break;

	case 'u':
		m_userTracker.stopSkeletonTracking(m_lastUserID);
		break;
	}

}

openni::Status SampleViewer::InitOpenGL(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	glutCreateWindow (m_strSampleName);
	// 	glutFullScreen();
	glutSetCursor(GLUT_CURSOR_NONE);

	InitOpenGLHooks();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	return openni::STATUS_OK;

}
void SampleViewer::InitOpenGLHooks()
{
	glutKeyboardFunc(GLUTKeyboard);
	glutDisplayFunc(GLUTDisplay);
	glutIdleFunc(GLUTIdle);
}

void SampleViewer::ProcessGrabEvent( PSLabs::IGrabEventListener::GrabEventType Type )
{
	printf("Got ");
	if(Type == PSLabs::IGrabEventListener::GRAB_EVENT)
		printf("Grab");
	else if(Type == PSLabs::IGrabEventListener::RELEASE_EVENT)
		printf("Release");
	else if(Type == PSLabs::IGrabEventListener::NO_EVENT)
		printf("No Event?!");
	printf(" event\n");
}

