This sample displays the depth/color output of the sensor, overlayed with the grab detector information for the first hand that was detected.
The console displays the events as they occur

Usage:
- Preform 'Click' or 'Wave' gesture to activate hand tracking and grab detection
- Use 1,2,3 digit keys to switch display mode.
- Press 'e' to switch optimal exposure on/off
- Press ’t’ to stop tracking the current hand
- Press ESC to exit.

Points of interest in the code (Viewer.cpp):
1.	UpdateAlgorithm() – Demonstrates the flow of updating the algorithm data for each frame
2.	DrawDetectorInfo() – Demonstrates getting hand position and visually displaying it.
3.	InitGrabDetector() – Demonstrates creation of the detector and events listener.
4.	ProcessGrabEvent() – Demonstrates handling GrabDetector events
5.	OnKey() – Demonstrates closing sequence and setting optimal exposure
