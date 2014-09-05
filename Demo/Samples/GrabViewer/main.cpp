#include <OpenNI.h>
#include <fstream>
#include "Viewer.h"

int main(int argc, char** argv)
{
	openni::Status rc = openni::STATUS_OK;
	
	openni::Device device;
	openni::VideoStream depth, color;
	const char* deviceURI = openni::ANY_DEVICE;
	if (argc > 1)
	{
		deviceURI = argv[1];
	}

	rc = openni::OpenNI::initialize();

	if (rc != openni::STATUS_OK)
	{
		printf("OpenNI Initialization failed:\n%s\n", openni::OpenNI::getExtendedError());
		return -1;
	}

	if (nite::NiTE::initialize() != nite::STATUS_OK)
	{
		printf("NiTE2 Initialization failed!");
		return -1;
	}

	rc = device.open(deviceURI);
	if (rc != openni::STATUS_OK)
	{
		printf("Device open failed:\n%s\n", openni::OpenNI::getExtendedError());
		openni::OpenNI::shutdown();
		return -1;
	}
	rc = device.setDepthColorSyncEnabled(true);

	openni::VideoMode videoMode;
	int resX = 640, resY = 480, fps = 30;
	
	rc = depth.create(device, openni::SENSOR_DEPTH);
	if (rc == openni::STATUS_OK)
	{
		videoMode = depth.getVideoMode();
		videoMode.setFps(fps);
		videoMode.setResolution(resX, resY);
		rc = depth.setVideoMode(videoMode);
		if (rc != openni::STATUS_OK)
		{
			printf("Couldn't set depth mode:\n%s\n", openni::OpenNI::getExtendedError());
			return -1;
		}
				
		rc = depth.start();
		if (rc != openni::STATUS_OK)
		{
			printf("Couldn't start depth stream:\n%s\n", openni::OpenNI::getExtendedError());
			depth.destroy();
			return -1;
		}
	}
	else
	{
		printf("Couldn't find depth stream:\n%s\n", openni::OpenNI::getExtendedError());
		return -1;
	}


	rc = color.create(device, openni::SENSOR_COLOR);
	if (rc == openni::STATUS_OK)
	{
		videoMode = color.getVideoMode();
		videoMode.setFps(fps);
		videoMode.setResolution(resX, resY);
		rc = color.setVideoMode(videoMode);
		if(rc != openni::STATUS_OK)
		{
			printf("Couldn't set color mode:\n%s\n", openni::OpenNI::getExtendedError());
			return -1;
		}

		rc = color.start();
		if (rc != openni::STATUS_OK)
		{
			printf("Couldn't start color stream:\n%s\n", openni::OpenNI::getExtendedError());
			color.destroy();
			return -1;
		}
	}
	else
	{
		printf("Couldn't find color stream:\n%s\n", openni::OpenNI::getExtendedError());
		return -1;
	}

	//Set stream parameters
	depth.setMirroringEnabled(true);
	color.setMirroringEnabled(true);

	//Set registration. This is very important as we cannot use image stream if they are not registered
	rc = device.setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
	if (rc != openni::STATUS_OK)
	{
		printf("Couldn't set image to depth registration. Disabling image stream.\n%s\n", openni::OpenNI::getExtendedError());
		color.stop();
		color.destroy();
	}
	
	if (!depth.isValid())
	{
		printf("No valid depth stream. Exiting\n");
		openni::OpenNI::shutdown();
		return -1;
	}

	std::ifstream file("Data/grab_gesture.dat");
	if (!file)
	{
		printf("Cannot find \"Data/grab_gesture.dat\"");
		openni::OpenNI::shutdown();
		return -1;
	}
	file.close();
	

	SampleViewer sampleViewer("Simple Viewer", device, depth, color);

	rc = sampleViewer.Init(argc, argv);
	if (rc != openni::STATUS_OK)
	{
		openni::OpenNI::shutdown();
		return -1;
	}
	sampleViewer.Run();
}

