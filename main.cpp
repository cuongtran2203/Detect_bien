#include<opencv2/highgui.hpp>
#include<iostream>
#include<opencv2/imgproc.hpp>
#include <queue>
#include <iterator>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/dnn/all_layers.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
constexpr float CONFIDENCE_THRESHOLD = 0;
constexpr float NMS_THRESHOLD = 0.4;
constexpr int NUM_CLASSES = 80;
using namespace std;
using namespace cv;
// colors for bounding boxes
const cv::Scalar colors[] = {
    {0, 255, 255},
    {255, 255, 0},
    {0, 255, 0},
    {255, 0, 0}
};
const auto NUM_COLORS = sizeof(colors)/sizeof(colors[0]);
int main()
{
    std::vector<std::string> class_names;
    {
        std::ifstream class_file("/home/cuong/Desktop/Liscen_plate/classes.txt");
        if (!class_file)
        {
            std::cerr << "failed to open classes.txt\n";
            return 0;
        }

        std::string line;
        while (std::getline(class_file, line))
            class_names.push_back(line);
    }
    Mat frame;
    std::string image_path = samples::findFile("/home/cuong/Desktop/Liscen_plate/L1_Lpn_20220822090928394.jpg");
    // Mat frame = imread(image_path, IMREAD_COLOR);
    // if(frame.empty())
    // {
    //     std::cout << "Could not read the image: " << image_path << std::endl;
    //     return 1;
    // }
    VideoCapture cap("/home/cuong/Desktop/Liscen_plate/201202_01_Oxford Shoppers_4k_006.mp4");
    if(!cap.isOpened()){
        std::cout << "Could not open the video : " <<endl;
        return -1;
    }
    
    auto net = cv::dnn::readNetFromDarknet("/home/cuong/Desktop/Liscen_plate/weights/yolov4-tiny.cfg", "/home/cuong/Desktop/Liscen_plate/weights/yolov4-tiny.weights");
    // net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    // net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    std::cout<<"Loaded weights successfully..."<<std::endl;
    auto output_names = net.getUnconnectedOutLayersNames();
    cv::Mat  blob;
    std::vector<cv::Mat> detections;
    auto total_start = std::chrono::steady_clock::now();
    while(1){
        cap>>frame;
        if (frame.empty())
        break;
    resize(frame,frame,Size(1280,720),INTER_LINEAR);
    cv::dnn::blobFromImage(frame, blob, 0.00392, cv::Size(608, 608), cv::Scalar(), true, false, CV_32F);
    net.setInput(blob);
    auto dnn_start = std::chrono::steady_clock::now();
    net.forward(detections, output_names);
    auto dnn_end = std::chrono::steady_clock::now();
    std::vector<int> indices[NUM_CLASSES];
    std::vector<cv::Rect> boxes[NUM_CLASSES];
    std::vector<float> scores[NUM_CLASSES];
    for (auto& output : detections)
    {
        const auto num_boxes = output.rows;
        for (int i = 0; i < num_boxes; i++)
        {
            auto x = output.at<float>(i, 0) * frame.cols;
            auto y = output.at<float>(i, 1) * frame.rows;
            auto width = output.at<float>(i, 2) * frame.cols;
            auto height = output.at<float>(i, 3) * frame.rows;
            cv::Rect rect(x - width/2, y - height/2, width, height);
            for (int c = 0; c < NUM_CLASSES; c++)
            {
                auto confidence = *output.ptr<float>(i, 5 + c);
                if (confidence >= CONFIDENCE_THRESHOLD)
                {
                    boxes[c].push_back(rect);
                    scores[c].push_back(confidence);
                }
            }
        }
    }
    for (int c = 0; c < NUM_CLASSES; c++)
        cv::dnn::NMSBoxes(boxes[c], scores[c], 0.0, NMS_THRESHOLD, indices[c]);
    for (int c= 0; c < NUM_CLASSES; c++)
    {
        for (size_t i = 0; i < indices[c].size(); ++i)
        {
            const auto color = colors[c % NUM_COLORS];

            auto idx = indices[c][i];
            const auto& rect = boxes[c][idx];
            cv::rectangle(frame, cv::Point(rect.x, rect.y), cv::Point(rect.x + rect.width, rect.y + rect.height), color, 3);
            std::ostringstream label_ss;
            label_ss << class_names[c] << ": " << std::fixed << std::setprecision(2) << scores[c][idx];
            auto label = label_ss.str();
            std::cout << label << std::endl;
            int baseline;
            auto label_bg_sz = cv::getTextSize(label.c_str(), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, 1, &baseline);
            cv::rectangle(frame, cv::Point(rect.x, rect.y - label_bg_sz.height - baseline - 10), cv::Point(rect.x + label_bg_sz.width, rect.y), color, cv::FILLED);
            cv::putText(frame, label.c_str(), cv::Point(rect.x, rect.y - baseline - 5), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, cv::Scalar(0, 0, 0));
        }
    }
    auto total_end = std::chrono::steady_clock::now();
    float inference_fps =1000.0/std::chrono::duration_cast<std::chrono::milliseconds>(dnn_end - dnn_start).count();
    float total_fps = 1000.0/std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();
    std::ostringstream stats_ss;
    stats_ss << std::fixed << std::setprecision(2);
    stats_ss << "Inference time : " << inference_fps << ", Total time: " << total_fps;
    auto stats = stats_ss.str();
    int baseline;
    auto stats_bg_sz = cv::getTextSize(stats.c_str(), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, 1, &baseline);
    cv::rectangle(frame, cv::Point(0, 0), cv::Point(stats_bg_sz.width, stats_bg_sz.height + 10), cv::Scalar(0, 0, 0), cv::FILLED);
    cv::putText(frame, stats.c_str(), cv::Point(0, stats_bg_sz.height + 5), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, cv::Scalar(255, 255, 255));
    cv::namedWindow("output");
    cv::imshow("output", frame);
    char c=(char)waitKey(25);
    if (c=='q')
    break;
}
cap.release();
destroyAllWindows();
return 0;
}