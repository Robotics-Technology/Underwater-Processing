/********************************************************************/
/* Project: uw_img_proc									            */
/* Module:  dehazing								                */
/* File: 	dehazing.cpp								            */
/* Created:	05/02/2019				                                */
/* Description:
	C++ Module for image dehazing using Gao's Bright Channel Prior	*/
 /*******************************************************************/

 /*******************************************************************/
 /* Created by:                                                     */
 /* Geraldine Barreto (@geraldinebc)                                */
 /*******************************************************************/

/// Include auxiliary utility libraries
#include "../include/dehazing.h"

#if USE_GPU

#endif

cv::Mat brightChannel(vector<Mat_<uchar>> channels, int size) {								// Generates the Bright Channel Image
	cv::Mat maxRGB = max(max(channels[0], channels[1]), channels[2]);						// Maximum Color Image
	cv::Mat element, bright_chan;
	element = getStructuringElement(MORPH_RECT, Size(size, size), Point(-1, -1));			// Maximum filter
	dilate(maxRGB, bright_chan, element);													// Dilates the maxRGB image
	return bright_chan;
}

cv::Mat maxColDiff(vector<Mat_<uchar>> channels) {											// Generates the Maximum Color Difference Image
	float means[3] = { mean(channels[0])[0], mean(channels[1])[0], mean(channels[2])[0] };
	cv::Mat c = Mat(1, 3, CV_32F, means);
	cv::Mat sorted;
	sortIdx(c, sorted, SORT_EVERY_ROW + SORT_ASCENDING);									// Orders the mean of the channels from low to high

	cv::Mat cmin = channels[sorted.at<int>(0, 0)];
	cv::Mat cmid = channels[sorted.at<int>(0, 1)];
	cv::Mat cmax = channels[sorted.at<int>(0, 2)];
	
	cv::Mat a, b, mcd;
	a = max(cmax - cmin, 0);																// Calculates the maximum values for the MCD image
	b = max(cmid - cmin, 0);
	mcd = 255 - max(a,b);
	return mcd;
}

cv::Mat  rectify(cv::Mat S, cv::Mat bc, cv::Mat mcd) {											// Rectifies the Bright Channel Image
	double lambda;
	minMaxIdx(S, NULL, &lambda);															// Maximum value of the Saturation channel
	lambda = lambda / 255.0;																// Normalization for the next step
	cv::Mat correct;
	addWeighted(bc, lambda, mcd, 1.0 - lambda, 0.0, correct);
	return correct;
}

vector<uchar> lightEstimation(cv::Mat src_gray, int size, cv::Mat bc, vector<Mat_<uchar>> channels) {	// Estimates the atmospheric light
	cv::Mat variance, sorted;
	sqrBoxFilter(src_gray, variance, -1, Size(size, size), Point(-1, -1), true, BORDER_DEFAULT);

	bc = bc.reshape(0, 1);
	sortIdx(bc, sorted, SORT_EVERY_ROW + SORT_ASCENDING);				// Sorts the bright channel pixels from low (dark) to high (bright)

	int x, y, percentage = round(src_gray.total()*0.01);				// 0.01 pixel percentage of the original image
	vector<float> darkest;
	vector<int> X, Y;
	for (int i = 0; i < percentage; i++) {
		x = floor(sorted.at<int>(0, i) / src_gray.cols);				// Conversion from the reshaped array to the original matrix coordinates
		if (x == 0 ) y = sorted.at<int>(0, i);
		else y = sorted.at<int>(0, i) - x * src_gray.cols;
		darkest.push_back(variance.at<float>(x,y));						// Values of the variance in the location of the bright channel 0.01% darkest pixels
		X.push_back(x);													// Matrix coordinates of the values
		Y.push_back(y);
	}

	Point minLoc;
	minMaxLoc(darkest, NULL, NULL, &minLoc, NULL);						// Location of the minimum value of the selected pixels from the variance

	vector<uchar> A;
	for (int i = 0; i < 3; i++) A.push_back(channels[i].at<uchar>(X[minLoc.x], Y[minLoc.x]));

	// PARA VISUALIZAR
	//src_gray.at<char>(X[minLoc.x], Y[minLoc.x]) = 255;
	//namedWindow("A point", WINDOW_KEEPRATIO);
	//imshow("A point", src_gray);

	return A;
}

cv::Mat transmittance(cv::Mat correct, vector<uchar> A) {						// Computes the Transmittance Image
	correct.convertTo(correct, CV_32F);
	cv::Mat t[3], acc(correct.size(), CV_32F, Scalar(0));
	for (int i = 0; i < 3; i++) {
		t[i] = 255.0 * ( (correct - A[i]) / (255.0 - A[i]) );
		accumulate(t[i], acc);
	}
	cv::Mat trans = acc/3;
	trans.convertTo(trans, CV_8U);
	return trans;
}

cv::Mat dehaze(vector<Mat_<float>> channels, vector<uchar> A, cv::Mat trans) {	// Restores the Underwater Image using the Bright Channel Prior
	trans.convertTo(trans, CV_32F, 1.0/255.0);
	channels[0] = 255.0 - ((channels[0] - (A[0] * (1.0 - trans))) / trans);
	channels[1] = 255.0 - ((channels[1] - (A[1] * (1.0 - trans))) / trans);
	channels[2] = (channels[2] - A[2]) / trans + A[2];
	cv::Mat dehazed, dst;
	merge(channels, dehazed);
	dehazed.convertTo(dst, CV_8U);
	return dst;
}