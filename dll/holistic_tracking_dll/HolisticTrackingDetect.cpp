#include <vector>

#include "HolisticTrackingDetect.h"
#include "GestureRecognition.h"
#include "ArmUpAndDownRecognition.h"

GoogleMediapipeDetect::HolisticTrackingDetect::HolisticTrackingDetect()
{
	m_bIsInit = false;
	m_bIsRelease = false;

	m_Video_InputStreamName = "input_video";

	m_Video_OutputStreamName = "output_video";
	m_PoseLandmarks_OutputStreamName = "pose_landmarks";
	m_LeftHandLandmarks_OutputStreamName = "left_hand_landmarks";
	m_RightHandLandmarks_OutputStreamName = "right_hand_landmarks";
	m_FaceLandmarks_OutputStreamName = "face_landmarks";

	m_pVideoPoller = nullptr;
	m_pPoseLandmarksPoller = nullptr;
	m_pLeftHandLandmarksPoller = nullptr;
	m_pRightHandLandmarksPoller = nullptr;
	m_pFaceLandmarksPoller = nullptr;
}

GoogleMediapipeDetect::HolisticTrackingDetect::~HolisticTrackingDetect()
{
	if (m_bIsInit && !m_bIsRelease)
	{
		Release();
	}
}

int GoogleMediapipeDetect::HolisticTrackingDetect::InitModel(
	const char* model_path,
	bool is_need_video_outputstream,
	bool is_need_pose_outputstream,
	bool is_need_hand_outputstream,
	bool is_need_face_outputstream)
{
	absl::Status run_status = Mediapipe_InitGraph(model_path,is_need_video_outputstream,is_need_pose_outputstream,is_need_hand_outputstream,is_need_face_outputstream);
	if (!run_status.ok())
		return 0;
	m_bIsInit = true;
	return  1;
}

int GoogleMediapipeDetect::HolisticTrackingDetect::DetectImageDirect(int image_width, int image_height, void* image_data, int* detect_result, bool show_result_image)
{
	if (!m_bIsInit)
		return 0;

	absl::Status run_status = Mediapipe_RunMPPGraph_Direct(image_width, image_height, image_data, detect_result,show_result_image);
	if (!run_status.ok()) {
		return 0;
	}
	return 1;
}

int GoogleMediapipeDetect::HolisticTrackingDetect::DetectCamera(bool show_image)
{
	if (!m_bIsInit)
		return 0;
	absl::Status run_status = Mediapipe_RunMPPGraph_Camera(show_image);
	if (!run_status.ok()) {
		return 0;
	}
	return 1;

}

int GoogleMediapipeDetect::HolisticTrackingDetect::Release()
{
	absl::Status run_status = Mediapipe_ReleaseGraph();
	if (!run_status.ok()) {
		return 0;
	}
	m_bIsRelease = true;
	return 1;
}

absl::Status GoogleMediapipeDetect::HolisticTrackingDetect::Mediapipe_InitGraph(
	const char* model_path,
	bool is_need_video_outputstream,
	bool is_need_pose_outputstream,
	bool is_need_hand_outputstream,
	bool is_need_face_outputstream
)
{
	std::string calculator_graph_config_contents;
	MP_RETURN_IF_ERROR(mediapipe::file::GetContents(model_path, &calculator_graph_config_contents));
	std::cout << "mediapipe::file::GetContents success" << std::endl;

	mediapipe::CalculatorGraphConfig config =
		mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
			calculator_graph_config_contents);

	MP_RETURN_IF_ERROR(m_Graph.Initialize(config));
	std::cout << "m_Graph.Initialize(config) success" << std::endl;

	// 1 视频输出
	if(is_need_video_outputstream)
	{
		auto videoOutputStream = m_Graph.AddOutputStreamPoller(m_Video_OutputStreamName);
		assert(videoOutputStream.ok());
		m_pVideoPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(videoOutputStream.value()));
	}

	// 2 PoseLandmarks输出
	if(is_need_pose_outputstream)
	{
		mediapipe::StatusOrPoller poseLandmarks = m_Graph.AddOutputStreamPoller(m_PoseLandmarks_OutputStreamName);
		assert(poseLandmarks.ok());
		m_pPoseLandmarksPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(poseLandmarks.value()));
	}

	// 3 LeftHandLandmarks输出
	if(is_need_hand_outputstream)
	{
		mediapipe::StatusOrPoller leftHandLandmarks = m_Graph.AddOutputStreamPoller(m_LeftHandLandmarks_OutputStreamName);
		assert(leftHandLandmarks.ok());
		m_pLeftHandLandmarksPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(leftHandLandmarks.value()));
	}

	// 4 RightHandLandmarks输出
	if(is_need_hand_outputstream)
	{
		mediapipe::StatusOrPoller rightHandLandmarks = m_Graph.AddOutputStreamPoller(m_RightHandLandmarks_OutputStreamName);
		assert(rightHandLandmarks.ok());
		m_pRightHandLandmarksPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(rightHandLandmarks.value()));
	}

	// 5 FaceLandmarks输出
	if(is_need_face_outputstream)
	{
		mediapipe::StatusOrPoller faceLandmarks = m_Graph.AddOutputStreamPoller(m_FaceLandmarks_OutputStreamName);
		assert(faceLandmarks.ok());
		m_pFaceLandmarksPoller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(faceLandmarks.value()));
	}

	// 设置图的输入流参数
	// 设置图的输入流队列中最大可排队Packet数
	m_Graph.SetInputStreamMaxQueueSize(m_Video_InputStreamName,1);
	// 设置图的输入流队列添加新包的参数
	m_Graph.SetGraphInputStreamAddMode(mediapipe::CalculatorGraph::GraphInputStreamAddMode::WAIT_TILL_NOT_FULL);

	MP_RETURN_IF_ERROR(m_Graph.StartRun({}));
	std::cout << "----------------Graph StartRun Success---------------------" << std::endl;
	return absl::OkStatus();
}

absl::Status GoogleMediapipeDetect::HolisticTrackingDetect::Mediapipe_RunMPPGraph_Direct(
	int image_width,
	int image_height, 
	void* image_data, 
	int* detect_result, 
	bool show_result_image
)
{
	// 等待图处于空闲状态
	//m_Graph.WaitUntilIdle();

	/*----- 1 构造cv::Mat对象 -----*/
	cv::Mat camera_frame(cv::Size(image_width, image_height), CV_8UC3, (uchar*)image_data);
	
	// BGR转RGB
	cv::Mat camera_frame_RGB;
	cv::cvtColor(camera_frame, camera_frame_RGB, cv::COLOR_BGR2RGB);

	// 水平翻转
	cv::flip(camera_frame_RGB, camera_frame_RGB, 1);

	//std::cout << "cv::Mat对象构建完成" << std::endl;

	/*----- 2 将OpenCV Mat转换为ImageFrame -----*/
	auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
		mediapipe::ImageFormat::SRGB, camera_frame_RGB.cols, camera_frame_RGB.rows,
		mediapipe::ImageFrame::kDefaultAlignmentBoundary);
	//camera_frame_RGB.copyTo(mediapipe::formats::MatView(input_frame.get()));
	cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
	camera_frame_RGB.copyTo(input_frame_mat);

	//std::cout << "将OpenCV Mat转换为ImageFrame完成" << std::endl;

	/*----- 3 发送图片到图中推理 -----*/
	// 获取当前距离1970的微秒时间
	size_t frame_timestamp_us =
	 	(double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;

	// std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> tpMicro
    //     = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
	// size_t frame_timestamp_us = tpMicro.time_since_epoch().count();

	MP_RETURN_IF_ERROR(m_Graph.AddPacketToInputStream(
		m_Video_InputStreamName, mediapipe::Adopt(input_frame.release())
		.At(mediapipe::Timestamp(frame_timestamp_us))));

	//std::cout << "发送图片到图中推理完成" << std::endl;

	/*----- 4 得到结果 -----*/
	// 1 视频输出结果帧
	if(m_pVideoPoller != nullptr)
	{
		mediapipe::Packet videoPacket;
		if(m_pVideoPoller->QueueSize() != 0)
		{
			if (!m_pVideoPoller->Next(&videoPacket))
			{
				return absl::InvalidArgumentError("no next packet");
			}

			if(show_result_image)
			{
				// 从视频输出获取mediapipe::ImageFrame结果
				auto& output_frame = videoPacket.Get<mediapipe::ImageFrame>();

				// 转换mediapipe::ImageFrame为cv::Mat
				cv::Mat output_frame_mat = mediapipe::formats::MatView(&output_frame);

				// 显示cv::Mat结果
				cv::cvtColor(output_frame_mat, output_frame_mat, cv::COLOR_RGB2BGR);
				cv::imshow("MediapipeHolistic", output_frame_mat);
			}
		}
	}

	// 2 PoseLandmarks
	int left_arm_result = (int)ArmUpDown::NoResult;
	int right_arm_result = (int)ArmUpDown::NoResult;
	if(m_pPoseLandmarksPoller != nullptr)
	{
		mediapipe::Packet poseeLandmarksPacket;

		if (m_pPoseLandmarksPoller->QueueSize() != 0)
		{
			if (m_pPoseLandmarksPoller->Next(&poseeLandmarksPacket))
			{
				auto& output_landmarks = poseeLandmarksPacket.Get<mediapipe::NormalizedLandmarkList>();
				//std::cout << "PoseLandmarks size:" << output_landmarks.landmark_size() << std::endl;

				std::vector<Point2D> posePoints;
				posePoints.clear();

				for (int i = 0; i < output_landmarks.landmark_size(); ++i)
				{
					Point2D tempPoint2D;
					const mediapipe::NormalizedLandmark landmark = output_landmarks.landmark(i);
					float x = landmark.x() * camera_frame.cols;
					float y = landmark.y() * camera_frame.rows;
					tempPoint2D.x = x;
					tempPoint2D.y = y;

					posePoints.emplace_back(tempPoint2D);
				}

				ArmUpAndDownRecognition armUpAndDownRecognition;
				armUpAndDownRecognition.RecognizeProcess(posePoints,left_arm_result,right_arm_result);

				posePoints.clear();
				posePoints.shrink_to_fit();

				//std::cout << "手臂抬手放手识别结果：" << poseDetectResult << std::endl;
			}
		}
	}
	detect_result[0] = left_arm_result;
	detect_result[1] = right_arm_result;

	// 3 LeftHandLandmarks
	int leftHandDetectResult = Gesture::NoGesture;
	if(m_pLeftHandLandmarksPoller != nullptr)
	{
		mediapipe::Packet leftHandLandmarksPacket;	
		if (m_pLeftHandLandmarksPoller->QueueSize() > 0)
		{
			if (m_pLeftHandLandmarksPoller->Next(&leftHandLandmarksPacket))
			{
				auto& output_landmarks = leftHandLandmarksPacket.Get<mediapipe::NormalizedLandmarkList>();
				//std::cout << "LeftHandLandmarks size:" << output_landmarks.landmark_size() << std::endl;

				std::vector<Point2D> singleGesturePoints;
				singleGesturePoints.clear();

				for (int i = 0; i < output_landmarks.landmark_size(); ++i)
				{
					Point2D tempPoint2D;
					const mediapipe::NormalizedLandmark landmark = output_landmarks.landmark(i);
					float x = landmark.x() * camera_frame.cols;
					float y = landmark.y() * camera_frame.rows;
					tempPoint2D.x = x;
					tempPoint2D.y = y;

					singleGesturePoints.emplace_back(tempPoint2D);
				}

				GestureRecognition gestureRecognition;
				leftHandDetectResult = gestureRecognition.RecognizeProcess(singleGesturePoints);

				singleGesturePoints.clear();
				singleGesturePoints.shrink_to_fit();
				//std::cout << "左手手势识别结果：" << leftHandDetectResult << std::endl;
			}
		}
	}
	detect_result[2] = leftHandDetectResult;


	// 4 RightHandLandmarks
	int rightHandDetectResult = Gesture::NoGesture;
	if(m_pRightHandLandmarksPoller != nullptr)
	{
		mediapipe::Packet rightHandLandmarksPacket;
		
		if (m_pRightHandLandmarksPoller->QueueSize() > 0)
		{
			if (m_pRightHandLandmarksPoller->Next(&rightHandLandmarksPacket))
			{
				auto& output_landmarks = rightHandLandmarksPacket.Get<mediapipe::NormalizedLandmarkList>();
				//std::cout << "RightHandLandmarks size:" << output_landmarks.landmark_size() << std::endl;

				std::vector<Point2D> singleGesturePoints;
				singleGesturePoints.clear();

				for (int i = 0; i < output_landmarks.landmark_size(); ++i)
				{
					Point2D tempPoint2D;
					const mediapipe::NormalizedLandmark landmark = output_landmarks.landmark(i);
					float x = landmark.x() * camera_frame.cols;
					float y = landmark.y() * camera_frame.rows;
					tempPoint2D.x = x;
					tempPoint2D.y = y;

					singleGesturePoints.emplace_back(tempPoint2D);
				}

				GestureRecognition gestureRecognition;
				rightHandDetectResult = gestureRecognition.RecognizeProcess(singleGesturePoints);
				
				singleGesturePoints.clear();
				singleGesturePoints.shrink_to_fit();
				//std::cout << "右手手势识别结果：" << rightHandDetectResult << std::endl;
			}
		}
	}
	detect_result[3] = rightHandDetectResult;

	// 5 FaceLandmarks
	if(m_pFaceLandmarksPoller != nullptr)
	{
		mediapipe::Packet faceLandmarksPacket;
		if (m_pFaceLandmarksPoller->QueueSize() > 0)
		{
			if (m_pFaceLandmarksPoller->Next(&faceLandmarksPacket))
			{
				auto& output_landmarks = faceLandmarksPacket.Get<mediapipe::NormalizedLandmarkList>();
				//std::cout << "FaceLandmarks size:" << output_landmarks.landmark_size() << std::endl;

				for (int i = 0; i < output_landmarks.landmark_size(); ++i)
				{
					const mediapipe::NormalizedLandmark landmark = output_landmarks.landmark(i);
					float x = landmark.x() * camera_frame.cols;
					float y = landmark.y() * camera_frame.rows;
				}
			}
		}
	}

	//m_Graph.WaitUntilIdle();
	//m_Graph.CloseAllPacketSources();

	return absl::OkStatus();
}


absl::Status GoogleMediapipeDetect::HolisticTrackingDetect::Mediapipe_RunMPPGraph_Camera(bool show_image)
{
	std::string cvWindowName = "MediapipeHolistic";

	// 打开OpenCV摄像头
	cv::VideoCapture capture(0);
	if (!capture.isOpened())
	{
		return absl::InvalidArgumentError("cv camera is not open");
	}

	bool grab_frames = true;
	while (grab_frames) {

		// 从摄像头抓取视频帧
		cv::Mat camera_frame_raw;
		capture >> camera_frame_raw;
		if (camera_frame_raw.empty())
			break;

		cv::Mat camera_frame_RGB;
		cv::cvtColor(camera_frame_raw, camera_frame_RGB, cv::COLOR_BGR2RGB);
		cv::flip(camera_frame_RGB, camera_frame_RGB, 1);

		// 将OpenCV Mat转换为ImageFrame
		auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
			mediapipe::ImageFormat::SRGB, camera_frame_RGB.cols, camera_frame_RGB.rows,
			mediapipe::ImageFrame::kDefaultAlignmentBoundary);

		camera_frame_RGB.copyTo(mediapipe::formats::MatView(input_frame.get()));

		// 发送图片到图中推理
		size_t frame_timestamp_us =
			(double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;

		MP_RETURN_IF_ERROR(m_Graph.AddPacketToInputStream(
			m_Video_InputStreamName, mediapipe::Adopt(input_frame.release())
			.At(mediapipe::Timestamp(frame_timestamp_us))));

		// 获取推理结果
		if(m_pVideoPoller != nullptr)
		{
			mediapipe::Packet videoPacket;
			if(m_pVideoPoller->QueueSize() != 0)
			{
				if (!m_pVideoPoller->Next(&videoPacket))
				{
					return absl::InvalidArgumentError("no next packet");
				}

				if(show_image)
				{
					// 从视频输出获取mediapipe::ImageFrame结果
					auto& output_frame = videoPacket.Get<mediapipe::ImageFrame>();

					// 转换mediapipe::ImageFrame为cv::Mat
					cv::Mat output_frame_mat = mediapipe::formats::MatView(&output_frame);

					// 显示cv::Mat结果
					cv::cvtColor(output_frame_mat, output_frame_mat, cv::COLOR_RGB2BGR);
					cv::imshow("MediapipeHolistic", output_frame_mat);
				}
			}
		}
	}
	if (show_image)
		cv::destroyWindow(cvWindowName);

	return absl::OkStatus();
}

absl::Status GoogleMediapipeDetect::HolisticTrackingDetect::Mediapipe_ReleaseGraph()
{
	MP_RETURN_IF_ERROR(m_Graph.CloseInputStream(m_Video_InputStreamName));

	return m_Graph.WaitUntilDone();
}