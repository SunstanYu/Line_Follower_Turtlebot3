/** MIT License
Copyright (c) 2017 Sudarshan Raghunathan
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*
*@copyright Copyright 2017 Sudarshan Raghunathan
*@file linedetect.cpp
*@author Sudarshan Raghunathan
*@brief  Class linedetect's function definitions
*/
#include "linedetect.hpp"
#include <cv_bridge/cv_bridge.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>
#include <opencv2/highgui/highgui.hpp>
#include "ros/ros.h"
#include "opencv2/opencv.hpp"
#include "ros/console.h"
#include "line_follower_turtlebot/pos.h"

int lastdir = 3;

void LineDetect::imageCallback(const sensor_msgs::CompressedImagePtr &msg)
{
  cv_bridge::CvImagePtr cv_ptr;
  try
  {
    cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    img = cv_ptr->image;
    cv::waitKey(30);
  }
  catch (cv_bridge::Exception &e)
  {
    ROS_ERROR("Could not convert from '' to 'bgr8'.");
  }
}

void LineDetect::geometry_msgs_sub(const geometry_msgs::PoseStamped point){
  arucodir = point;
  ROS_INFO("point x:%f, y:%f, z:%f",point.pose.position.x,point.pose.position.y,point.pose.position.z);
}

bool rectJudge(cv::Rect rectan, int h)
{
  if (rectan.y <= h)
  {
    return false;
  }
  else
    return true;
}

// bool rectJudge(cv::Rect rectan,int h){
//   if(h-rectan.y>0.15*h){
//       return true;
//   }else return false;
// }
bool noisejudge(cv::Rect rectan)
{
  if (rectan.area() < 500 && rectan.height / rectan.width > 1.5)
  {
    return false;
  }
  else
    return true;
}

bool directionJudge(cv::Rect rectan)
{
  if (rectan.height / rectan.width > 2)
  {
    return true;
  }
  else
    return false;
}

std::string converToString(double d)
{
  std::ostringstream os;
  if (os << d)
  {
    return os.str();
  }
  return "invalid conversion";
}

cv::Mat LineDetect::Gauss(cv::Mat input)
{
  cv::Mat output;
  // Applying Gaussian Filter
  cv::GaussianBlur(input, output, cv::Size(3, 3), 0.1, 0.1);
  return output;
}

int LineDetect::colorthresh(cv::Mat input)
{
  // Initializaing variables
  cv::Size s = input.size();
  std::vector<std::vector<cv::Point>> v;
  auto w = s.width;
  auto h = s.height;
  // std::string size = "w: "+std::to_string(w)+ "h: "+std::to_string(h);
  // ROS_INFO("%s",size);
  auto c_x = 0.0;
  // Detect all objects within the HSV range
  cv::cvtColor(input, LineDetect::img_hsv, CV_BGR2HSV);
  // Yellow
  //  LineDetect::LowerYellow = {20, 100, 100};
  //  LineDetect::UpperYellow = {30, 255, 255};
  // White
  //  LineDetect::LowerYellow = {0, 0, 220};
  //  LineDetect::UpperYellow = {180, 30, 255};
  // Black
  LineDetect::LowerYellow = {0, 0, 0};
  LineDetect::UpperYellow = {255, 180, 68};
  cv::inRange(LineDetect::img_hsv, LowerYellow,
              UpperYellow, LineDetect::img_mask);
  img_mask(cv::Rect(0, 0, w, 0.85 * h)) = 0;
  // img_mask(cv::Rect(0, 0, w, 0.65*h)) = 0;

  // Find contours for better visualization
  cv::findContours(LineDetect::img_mask, v, CV_RETR_LIST, CV_CHAIN_APPROX_NONE);
  // If contours exist add a bounding
  // Choosing contours with maximum area
  cv::Rect rect;
  bool notFind = true;
  if (v.size() != 0)
  {
    notFind = false;
    auto area = 0;
    auto idx = 0;
    auto count = 0;
    while (count < v.size())
    {
      if (area < v[count].size())
      {
        idx = count;
        area = v[count].size();
      }
      count++;
    }
    rect = boundingRect(v[idx]);
    cv::Point pt1, pt2, pt3;
    pt1.x = rect.x;
    pt1.y = rect.y;
    pt2.x = rect.x + rect.width;
    pt2.y = rect.y + rect.height;
    pt3.x = pt1.x - 10;
    pt3.y = pt1.y - 10;
    // Drawing the rectangle using points obtained
    rectangle(input, pt1, pt2, CV_RGB(0, 255, 0), 2);
    // Inserting text box

    cv::putText(input, "(" + std::to_string(pt3.x) + "," + std::to_string(pt3.y) + ")", pt3,
                CV_FONT_HERSHEY_COMPLEX, 1, CV_RGB(0, 255, 0));
    // ROS_INFO("pt3: %d,%d",pt3.x,pt3.y);
  }

  // Mask image to limit the future turns affecting the output
  img_mask(cv::Rect(0.7 * w, 0, 0.3 * w, h)) = 0;
  img_mask(cv::Rect(0, 0, 0.3 * w, h)) = 0;
  // Perform centroid detection of line
  cv::Moments M = cv::moments(LineDetect::img_mask);
  if (M.m00 > 0)
  {
    cv::Point p1(M.m10 / M.m00, M.m01 / M.m00);
    cv::circle(LineDetect::img_mask, p1, 5, cv::Scalar(155, 200, 0), -1);
  }
  c_x = M.m10 / M.m00;
  // std::string ss = "cx: "+std::to_string(c_x)+ " "+"mid: "+std::to_string(h/2);
  if (!notFind)
  {
    std::string ss = "area: " + std::to_string(rect.area()) + " " + "ratio: " + std::to_string(rect.height / rect.width);
    ROS_INFO_STREAM(ss);
  }

  std::string cx = converToString(c_x);
  std::string mid = converToString(w / 2);
  cv::putText(input, "c_x: " + cx, cv::Point(w / 2, 20),
              CV_FONT_HERSHEY_COMPLEX, 0.6, CV_RGB(0, 255, 0));
  cv::putText(input, "mid: " + mid, cv::Point(w / 2, 40),
              CV_FONT_HERSHEY_COMPLEX, 0.6, CV_RGB(0, 255, 0));
  if (!notFind)
  {
    cv::putText(input, "area: " + rect.area(), cv::Point(w / 2, 60),
                CV_FONT_HERSHEY_COMPLEX, 0.6, CV_RGB(0, 255, 0));
    cv::putText(input, "ratio: " + rect.height / rect.width, cv::Point(w / 2, 80),
                CV_FONT_HERSHEY_COMPLEX, 0.6, CV_RGB(0, 255, 0));
  }

  // Tolerance to chooise directions
  auto tol_stright = 35;
  auto tol_turn = 50;
  auto count = cv::countNonZero(img_mask);
  // Turn left if centroid is to the left of the image center minus tolerance
  // Turn right if centroid is to the right of the image center plus tolerance
  // Go straight if centroid is near image center && noisejudge(rect)
  if (!notFind)
  {
    if ( !rectJudge(rect, h / 2) )
    {
      LineDetect::dir = lastdir;
    }
    else if (c_x < w / 2 - tol_turn)
    {
      LineDetect::dir = 0;
      lastdir = 0;
    }
    else if (c_x > w / 2 + tol_turn)
    {
      LineDetect::dir = 2;
      lastdir = 2;
    }
    else if (c_x > w / 2 + tol_stright)
    {
      LineDetect::dir = 4;
      lastdir = 4;
    }
    else if (c_x > w / 2 - tol_stright)
    {
      LineDetect::dir = 5;
      lastdir = 5;
    }
    else
    {
      LineDetect::dir = 1;
      lastdir = 1;
    }
  }

  // Search if no line detected
  if (count == 0)
  {
    LineDetect::dir = 3;
    lastdir = 3;
  }
  // Output images viewed by the turtlebot
  cv::namedWindow("Turtlebot View");
  imshow("Turtlebot View", input);
  return LineDetect::dir;
}
