// Standard Library
#include <string>
#include <iostream>
#include <stdio.h>

//openCV Header
#include <opencv2/core/core.hpp> 
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

// Kinect for Windows SDK Header
#include <Kinect.h>
#include <Kinect.VisualGestureBuilder.h>

using namespace std;
using namespace cv;


int main(int argc, char** argv)
{
#pragma region Sensor related code
	// Get default Sensor
	cout << "Try to get default sensor" << endl;
	IKinectSensor* pSensor = nullptr;
	if (GetDefaultKinectSensor(&pSensor) != S_OK)
	{
		cerr << "Get Sensor failed" << endl;
		return -1;
	}

	// Open sensor
	cout << "Try to open sensor" << endl;
	if (pSensor->Open() != S_OK)
	{
		cerr << "Can't open sensor" << endl;
		return -1;
	}
#pragma endregion

#pragma region Body releated code
	// Get body frame source
	cout << "Try to get body source" << endl;
	IBodyFrameSource* pBodyFrameSource = nullptr;
	if (pSensor->get_BodyFrameSource(&pBodyFrameSource) != S_OK)
	{
		cerr << "Can't get body frame source" << endl;
		return -1;
	}

	// Get the number of body
	INT32 iBodyCount = 0;
	if (pBodyFrameSource->get_BodyCount(&iBodyCount) != S_OK)
	{
		cerr << "Can't get body count" << endl;
		return -1;
	}
	cout << " > Can trace " << iBodyCount << " bodies" << endl;

	// Allocate resource for bodies
	IBody** aBody = new IBody*[iBodyCount];
	for (int i = 0; i < iBodyCount; ++i)
		aBody[i] = nullptr;

	// get body frame reader
	cout << "Try to get body frame reader" << endl;
	IBodyFrameReader* pBodyFrameReader = nullptr;
	if (pBodyFrameSource->OpenReader(&pBodyFrameReader) != S_OK)
	{
		cerr << "Can't get body frame reader" << endl;
		return -1;
	}
#pragma endregion

#pragma region Visual Gesture Builder Database
	// Load gesture dataase from File
	wstring sDatabaseFile = L"squat.gbd";	// Modify this file to load other file
	IVisualGestureBuilderDatabase* pGestureDatabase = nullptr;
	wcout << L"Try to load gesture database file " << sDatabaseFile << endl;
	CreateVisualGestureBuilderDatabaseInstanceFromFile(sDatabaseFile.c_str(), &pGestureDatabase);

	// Get the number of gestures in database
	UINT iGestureCount = 0;
	cout << "Try to read gesture list" << endl;
	if (pGestureDatabase->get_AvailableGesturesCount(&iGestureCount) != S_OK)
	{
		cerr << "Can't read the gesture count" << endl;
		return -1;
	}
	if (iGestureCount == 0)
	{
		cerr << "There is no gesture in the database" << endl;
		return -1;
	}

	// get the list of gestures
	IGesture** aGestureList = new IGesture*[iGestureCount];
	if (pGestureDatabase->get_AvailableGestures(iGestureCount, aGestureList) != S_OK)
	{
		cerr << "Can't read the gesture list" << endl;
		return -1;
	}
	else
	{
		// output the gesture list
		cout << "There are " << iGestureCount << " gestures in the database: " << endl;
		GestureType mType;
		const UINT uTextLength = 260; // magic number, if value smaller than 260, can't get name
		wchar_t sName[uTextLength];
		for (UINT i = 0; i < iGestureCount; ++i)
		{
			if (aGestureList[i]->get_GestureType(&mType) == S_OK)
			{
				if (mType == GestureType_Discrete)
					cout << "\t[D] ";
				else if (mType == GestureType_Continuous)
					cout << "\t[C] ";

				if (aGestureList[i]->get_Name(260, sName) == S_OK)
					wcout << sName << endl;

			}
		}
	}
#pragma endregion

#pragma region Gesture frame related code
	// create for each possible body
	IVisualGestureBuilderFrameSource** aGestureSources = new IVisualGestureBuilderFrameSource*[iBodyCount];
	IVisualGestureBuilderFrameReader** aGestureReaders = new IVisualGestureBuilderFrameReader*[iBodyCount];
	cout << iBodyCount << endl;

	for (int i = 0; i < iBodyCount; ++i)
	{
		// frame source
		aGestureSources[i] = nullptr;
		if (CreateVisualGestureBuilderFrameSource(pSensor, i, &aGestureSources[i]) != S_OK)
		{
			cerr << "Can't create IVisualGestureBuilderFrameSource" << endl;
			return -1;
		}

		// set gestures
		if (aGestureSources[i]->AddGestures(iGestureCount, aGestureList) != S_OK)
		{
			cerr << "Add gestures failed" << endl;
			//return -1;
		}

		// frame reader
		aGestureReaders[i] = nullptr;
		if (aGestureSources[i]->OpenReader(&aGestureReaders[i]) != S_OK)
		{
			cerr << "Can't open IVisualGestureBuilderFrameReader" << endl;
			return -1;
		}

	}
#pragma endregion

#pragma region Depth
		//2a. get frame source
		IDepthFrameSource* pFrameSource = nullptr;
		pSensor->get_DepthFrameSource(&pFrameSource);

		//2b. Get frame description
		int iWidth = 0;
		int iHeight = 0;
		IFrameDescription* pFrameDescription = nullptr;
		pFrameSource->get_FrameDescription(&pFrameDescription);
		pFrameDescription->get_Width(&iWidth);
		pFrameDescription->get_Height(&iHeight);
		pFrameDescription->Release();
		pFrameDescription = nullptr;

		// 2c. get some dpeth only meta
		UINT16 uDepthMin = 0, uDepthMax = 0;
		pFrameSource->get_DepthMinReliableDistance(&uDepthMin);
		pFrameSource->get_DepthMaxReliableDistance(&uDepthMax);
		cout << "Reliable Distance: " << uDepthMin << " �V " << uDepthMax << endl;

		// perpare OpenCV
		cv::Mat mDepthImg(iHeight, iWidth, CV_16UC1);
		cv::Mat mImg8bit(iHeight, iWidth, CV_8UC1);
		cv::namedWindow("Depth Map");

        //3a. get frame source
		IDepthFrameReader* pFrameReader = nullptr;
		pFrameSource->OpenReader(&pFrameReader);
#pragma endregion

#pragma region main loop
	// Enter main loop
	int iStep = 0;
	struct {
		bool success =false;
		int count=0;
	}squat, push_up;
	FILE *file;
	while (iStep < 10000)
	{
		// 4a. Get last frame
		IBodyFrame* pBodyFrame = nullptr;
		IDepthFrame* pFrame = nullptr;
		// 4a. Get last frame
		if (pBodyFrameReader->AcquireLatestFrame(&pBodyFrame) == S_OK && pFrameReader->AcquireLatestFrame(&pFrame) == S_OK)
		{
			++iStep;
			//  copy the depth map to image
			pFrame->CopyFrameDataToArray(iWidth * iHeight,
				reinterpret_cast<UINT16*>(mDepthImg.data));

			//  convert from 16bit to 8bit
			mDepthImg.convertTo(mImg8bit, CV_8U, 255.0f / uDepthMax);
			cv::imshow("Depth Map", mImg8bit);

			// 4b. get Body data
			if (pBodyFrame->GetAndRefreshBodyData(iBodyCount, aBody) == S_OK)
			{
				// 4c. for each body
				for (int i = 0; i < iBodyCount; ++i)
				{
					IBody* pBody = aBody[i];

					// check if is tracked
					BOOLEAN bTracked = false;
					if ((pBody->get_IsTracked(&bTracked) == S_OK) && bTracked)
					{
						
						// get tracking ID of body
						UINT64 uTrackingId = 0;
						if (pBody->get_TrackingId(&uTrackingId) == S_OK)
						{
							// get tracking id of gesture
							UINT64 uGestureId = 0;
							if (aGestureSources[i]->get_TrackingId(&uGestureId) == S_OK)
							{
								if (uGestureId != uTrackingId)
								{
									// assign traking ID if the value is changed
									cout << "Gesture Source " << i << " start to track user " << uTrackingId << endl;
									aGestureSources[i]->put_TrackingId(uTrackingId);
								}
							}
						}

						// Get gesture frame for this body
						IVisualGestureBuilderFrame* pGestureFrame = nullptr;
						if (aGestureReaders[i]->CalculateAndAcquireLatestFrame(&pGestureFrame) == S_OK)
						{
							// check if the gesture of this body is tracked
							BOOLEAN bGestureTracked = false;
							if (pGestureFrame->get_IsTrackingIdValid(&bGestureTracked) == S_OK && bGestureTracked)
							{
								GestureType mType;
								const UINT uTextLength = 260;
								wchar_t sName[uTextLength];

								// for each gestures
								for (UINT j = 0; j < iGestureCount; ++j)
								{
									// get gesture information
									aGestureList[j]->get_GestureType(&mType);
									aGestureList[j]->get_Name(uTextLength, sName);
									/*
									if (mType == GestureType_Discrete)
									{
										// get gesture result
										IDiscreteGestureResult* pGestureResult = nullptr;
										if (pGestureFrame->get_DiscreteGestureResult(aGestureList[j], &pGestureResult) == S_OK)
										{
											// check if is detected
											BOOLEAN bDetected = false;
											if (pGestureResult->get_Detected(&bDetected) == S_OK && bDetected)
											{
												float fConfidence = 0.0f;
												pGestureResult->get_Confidence(&fConfidence);

												// output information
												wcout << L"Detected Gesture " << sName << L" @" << fConfidence << endl;
											}
											pGestureResult->Release();
										}
									}*/
									if (mType == GestureType_Continuous)
									{
										// get gesture result
										IContinuousGestureResult* pGestureResult = nullptr;
										if (pGestureFrame->get_ContinuousGestureResult(aGestureList[j], &pGestureResult) == S_OK)
										{
											// get progress
											float fProgress = 0.0f;
											if (pGestureResult->get_Progress(&fProgress) == S_OK)    
											{
												if (squat.success != true)
												{
													if (fProgress > 0.6f)
												    {   
													   // output information
													   wcout << L"Detected Gesture " << sName << L" " << fProgress << endl;
													   cout << "successful!!!" << endl;
													   file = fopen("C:\\Users\\user\\Dropbox\\test.txt", "w");
													   squat.count++;
													   fprintf(file, "squat : %d\n", squat.count);
													   squat.success = true;
												    }

												}
												    
												else if (squat.success == true)
												{
													if (fProgress < 0.5f)
													{
														cout<< "motion end" << endl;
														squat.success = false;
													}
												}

											}
											pGestureResult->Release();
										}
									}
								}
							}
							pGestureFrame->Release();
						}
					}
				}
			}
			else
			{
				cerr << "Can't read body data" << endl;
			}

			// release frame
			pBodyFrame->Release();
			pFrame->Release();
		}
		if (cv::waitKey(30) == VK_ESCAPE)
			break;
	}
#pragma endregion

#pragma region Resource release
	// release gesture data
	for (UINT i = 0; i < iGestureCount; ++i)
		aGestureList[i]->Release();
	delete[] aGestureList;

	// release body data
	for (int i = 0; i < iBodyCount; ++i)
		aBody[i]->Release();
	delete[] aBody;

	// release gesture source and reader
	for (int i = 0; i < iBodyCount; ++i)
	{
		aGestureReaders[i]->Release();
		aGestureSources[i]->Release();
	}
	delete[] aGestureReaders;
	delete[] aGestureSources;

	// release frame reader
	pFrameReader->Release();
	pFrameReader = nullptr;

	// release body frame source and reader
	pBodyFrameReader->Release();
	pBodyFrameSource->Release();
	pFrameSource->Release();

	// Close Sensor
	pSensor->Close();

	// Release Sensor
	pSensor->Release();
	pSensor = nullptr;
#pragma endregion

	return 0;
}
