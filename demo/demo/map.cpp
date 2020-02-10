
#include <iostream>
#include <signal.h>
#include <stdlib.h>     /* srand, rand */
#include <unistd.h>
#include <mutex>
#include "utils.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "Yolo3Detection.h"
#include "CenternetDetection.h"

#include "evaluation.h"

#include <map>

void convertFilename(std::string &filename,const std::string l_folder, const std::string i_folder, const std::string l_ext,const std::string i_ext)
{
    filename.replace(filename.find(l_folder),l_folder.length(),i_folder);
    filename.replace(filename.find(l_ext),l_ext.length(),i_ext);
}

int main(int argc, char *argv[]) 
{
    // char *net = "resnet101_cnet_FP32.rt";
    char *net = "yolo3.rt";
    if(argc > 1)
        net = argv[1]; 
    char ntype = 'y';
    if(argc > 2)
        ntype = argv[2][0]; 
    //path to txt file with all realpath of images labels
    char *labels_path = "/media/887E650E7E64F67A/val2017/all_labels2017.txt";
    if(argc > 3)
        labels_path = argv[3]; 
    
    bool show = false;
    bool write_dets = false;

    tk::dnn::Yolo3Detection yolo;
    tk::dnn::CenternetDetection cnet;
    

    switch(ntype)
    {
        case 'y':
            yolo.init(net);
            break;
        case 'c':
            cnet.init(net);
            break;
        default:
        FatalError("Network type not allowed (3rd parameter)\n");
    }


    std::ifstream all_labels(labels_path);
    std::string l_filename;
    std::vector<Frame> images;

    std::cout<<"Reading groundtruth and generating detections"<<std::endl;

    if(show)
        cv::namedWindow("detection", cv::WINDOW_NORMAL);

    std::vector<tk::dnn::box> detected_bbox;
    
    int i=0;
    while (std::getline(all_labels, l_filename) && i < 1000) 
    {
        std::cout <<COL_ORANGEB<< "Images done:\t" << i++ << "\n"<<COL_END;

        Frame f;
        f.l_filename = l_filename;
        f.i_filename = l_filename;
        convertFilename(f.i_filename, "labels", "images", ".txt", ".jpg");

        // generate detections
        cv::Mat frame = cv::imread(f.i_filename.c_str(), cv::IMREAD_COLOR);
        int height = frame.rows;
        int width = frame.cols;

        cv::Mat dnn_input;
        if(!frame.data) 
            break;
        dnn_input = frame.clone();

        //inference 
        detected_bbox.clear();
        switch(ntype)
        {
            case 'y':
                yolo.update(dnn_input);
                detected_bbox = yolo.detected;
                break;
            case 'c':
                cnet.update(dnn_input);
                detected_bbox = cnet.detected;
                break;
            default:
                FatalError("Network type not allowed!\n");
        }
        std::ofstream myfile;
        if(write_dets)
            myfile.open ("det/"+f.l_filename.substr(l_filename.find("000")));


        // save detections labels
        for(auto d:detected_bbox)
        {
            //convert detected bb in the same format as label
            //<x_center>/<image_width> <y_center>/<image_width> <width>/<image_width> <height>/<image_width>
            BoundingBox b;
            b.x = (d.x + d.w/2) / width;
            b.y = (d.y + d.h/2) / height;
            b.w = d.w / width;
            b.h = d.h / height;
            b.prob = d.prob;
            b.cl = d.cl;
            f.det.push_back(b);

            if(write_dets)
                myfile << d.cl << " "<< d.prob << " "<< d.x << " "<< d.y << " "<< d.w << " "<< d.h <<"\n";

			if(show)// draw rectangle for detection
                cv::rectangle(frame, cv::Point(d.x, d.y), cv::Point(d.x + d.w, d.y + d.h), cv::Scalar(0, 0, 255), 2);             
        }

        if(write_dets)
            myfile.close();

        // read and save groundtruth labels
        std::ifstream labels(l_filename);
        for(std::string line; std::getline(labels, line); )
        {
            std::istringstream in(line); 
            BoundingBox b;
            in >> b.cl >> b.x >> b.y >> b.w >> b.h;  
            b.prob = 1;
            b.truth_flag = 1;
            f.gt.push_back(b);

            if(show)// draw rectangle for groundtruth
                cv::rectangle(frame, cv::Point((b.x-b.w/2)*width, (b.y-b.h/2)*height), cv::Point((b.x+b.w/2)*width,(b.y+b.h/2)*height), cv::Scalar(0, 255, 0), 2);             
        }    
      
        images.push_back(f);
        
        if(show)
        {
            cv::imshow("detection", frame);
            cv::waitKey(0);
        }
    }

    std::cout<<"Done."<<std::endl;
    
    int classes = 80;
    int map_points = 101;
    int map_levels = 10;
    float map_step = 0.05;
    float IoU_thresh = 0.5;
    float conf_thresh = 0.3;
    bool verbose = false;

    double AP = computeMapNIoULevels(images,classes,IoU_thresh,conf_thresh, map_points, map_step, map_levels, verbose);
    std::cout<<"mAP "<<IoU_thresh<<":"<<IoU_thresh+map_step*(map_levels-1)<<" = "<<AP<<std::endl;

    computeTPFPFN(images,classes,IoU_thresh,conf_thresh);


    return 0;
}
