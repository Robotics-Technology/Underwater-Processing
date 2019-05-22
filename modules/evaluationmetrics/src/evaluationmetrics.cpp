/*********************************************************************/
/* Project: uw-img-proc									             */
/* Module: 	evaluationmetrics							             */
/* File: 	evaluationmetrics.cpp							         */
/* Created:	09/12/2018				                                 */
/* Description:
	C++ Module of image quality metrics for image processing evaluation
 /********************************************************************/

 /********************************************************************/
 /* Created by:                                                      */
 /* Geraldine Barreto (@geraldinebc)                                 */
 /* Based on: Vision parameters measured using the mathematical model*/ 
 /*			  given by Xie and Wang and the frequency domain image	 */
 /*			  sharpness measure given by De and Masilamani.			 */
 /********************************************************************/

/// Include auxiliary utility libraries
#include "../include/evaluationmetrics.h"

float entropy(cv::Mat img) {
	cv::Mat hist, normhist, prob, logP;
	getHistogram(&img, &hist);
	normalize(hist, normhist, 0, 1, NORM_MINMAX);				// Normalized histogram
	prob = normhist / sum(normhist).val[0];						// Probability
	prob += 0.000000001;										// Added 0.000000001 to avoid errors calculating the logarithm
	log(prob, logP);											// Natural logarithm of the probability
	float entropy = -1.0*sum(prob.mul(logP/log(2))).val[0];		// Computes the entropy according to the Shannon Index	
	return (entropy);
}

float averageEntropy(cv::Mat R, cv::Mat G, cv::Mat B) {			// Entropy considering the RGB components
	float AE = sqrt(pow(entropy(R) , 2) + pow(entropy(G) , 2) + pow(entropy(B) , 2)) / sqrt(3);
	return (AE);
}

float averageContrast(cv::Mat R, cv::Mat G, cv::Mat B) {
	cv::Mat GradR, GradG, GradB;
	cv::Mat C = Mat(R.rows-2, R.cols-2, CV_8U);

	int kernel_size = 3, scale = 1, delta = 0, ddepth = CV_32F;
	Laplacian(R, GradR, ddepth, kernel_size, scale, delta, BORDER_DEFAULT);	// RGB components gradients
	Laplacian(G, GradG, ddepth, kernel_size, scale, delta, BORDER_DEFAULT);
	Laplacian(B, GradB, ddepth, kernel_size, scale, delta, BORDER_DEFAULT);
															 
	pow(GradR, 2, GradR), pow(GradG, 2, GradG), pow(GradB, 2, GradB);		// Grad^2

	for (int i = 0; i < R.rows-2; i++)	{									// C = sqrt(Sum(Grad))/sqrt(3)
		for (int j = 0; j < R.cols-2; j++)	{
			C.at<uchar>(i, j) = sqrt(GradR.at<float>(i, j) + GradG.at<float>(i, j) + GradB.at<float>(i, j)) / sqrt(3);
		}
	}
	float AC = sum(C)[0] / ((R.rows - 1)*(R.cols - 1));						// Normalized sum of gradients
	return(AC);
}

float averageLuminance(cv::Mat L) {
	float AL = sum(L)[0] / (L.total());										// Normalized sum of the luminance component
	return(AL);
}

float getNNF(float AL) {
	float OL = 127.5;
	float NNF = (OL - abs(AL-OL)) / OL;										// Normalized Neighborhood Function
	return(NNF);
}

float getCAF(float AE, float AC, float NNF) {
	float CAF = AE + pow(AC,1/4) + pow(NNF,3);								// Comprehensive Assessment Function
	return(CAF);
}

float getMSE(cv::Mat src, cv::Mat dst) {
	cv::Mat diff;
	absdiff(src, dst, diff);
	diff.convertTo(diff, CV_32F);
	diff = diff.mul(diff);
	float mse = sum(diff)[0] / src.total();									// Mean Square Error
	return (mse);
}

float getPSNR(float mse) {
	float psnr = 20 * log10(255 / sqrt(mse));								// Peak Signal to Noise Ratio
	return (psnr);
}

float IQMfun(cv::Mat src){
	cv::Mat padded, src_dft, dft_mag;
	int m = getOptimalDFTSize(src.rows);					// Optimal Size to calculate the Discrete Fourier Transform 
	int n = getOptimalDFTSize(src.cols);
	copyMakeBorder(src, padded, 0, m - src.rows, 0, n - src.cols, BORDER_CONSTANT, Scalar::all(0));	// Resize to optimal FFT size
	cv::Mat planes[] = { Mat_<float>(padded), Mat::zeros(padded.size(), CV_32F) };
	merge(planes, 2, src_dft);								// Plane with zeros is added so the the real and complex results will fit in the source matrix
	dft(src_dft, src_dft);									// The Discrete Fourier Transform is divided by the number of the matrix elements
	split(src_dft / src.total(), planes);					// The result is splitted -> planes[0] = Re(DFT(src)), planes[1] = Im(DFT(src))
	magnitude(planes[0], planes[1], dft_mag);				// dft_mag = sqrt(Re(DFT(src))^2+Im(DFT(src))^2)
	double max;
	minMaxIdx(dft_mag, NULL, &max);							// Maximum value of the Fourier transform magnitude
	double thres = max/1000;								// Threshold to calculate the IQM
	float TH = 0;
	for (int i = 0; i < dft_mag.rows; i++){					// Number of pixels in dft_mag whose pixel value > thres
		for (int j = 0; j < dft_mag.cols; j++){
			if (dft_mag.at<float>(i, j) > thres) TH++;
		}
	}
	float IQM = TH / src.total();							// Computes the Image Quality Measure
	return IQM;
}

void getHistogram(cv::Mat *channel, cv::Mat *hist) {		// Computes the intensity distribution histogram
	int histSize = 256;
	float range[] = { 0, 256 };
	const float* histRange = { range };
	calcHist(channel, 1, 0, Mat(), *hist, 1, &histSize, &histRange, true, false);
}

cv::Mat printHist(cv::Mat histogram, Scalar color) {
	// Finding the maximum value of the histogram. It will be used to scale the histogram to fit the image
	int max = 0;
	for (int i = 0; i < 256; i++) {
		if (histogram.at<float>(i, 0) > max) max = histogram.at<float>(i, 0);
	}
	// Histogram Image
	cv::Mat imgHist(1480, 1580, CV_8UC3, cv::Scalar(255, 255, 255));
	cv::Point pt1, pt2;
	pt1.y = 1380;
	for (int i = 0; i < 256; i++) {
		pt1.x = 150 + 5 * i + 1;
		pt2.x = 150 + 5 * i + 3;
		pt2.y = 1380 - 1280 * histogram.at<float>(i, 0) / max;
		cv::rectangle(imgHist, pt1, pt2, color, CV_FILLED);
	}
	// y-axis labels
	cv::rectangle(imgHist, cv::Point(130, 1400), cv::Point(1450, 80), cvScalar(0, 0, 0), 1);
	cv::putText(imgHist, std::to_string(max), cv::Point(10, 100), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(max * 3 / 4), cv::Point(10, 420), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(max / 2), cv::Point(10, 740), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(max / 4), cv::Point(10, 1060), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(0), cv::Point(10, 1380), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	// x-axis labels
	cv::putText(imgHist, std::to_string(0), cv::Point(152 - 7 * 1, 1430), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(63), cv::Point(467 - 7 * 2, 1430), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(127), cv::Point(787 - 7 * 3, 1430), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(191), cv::Point(1107 - 7 * 3, 1430), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);
	cv::putText(imgHist, std::to_string(255), cv::Point(1427 - 7 * 3, 1430), cv::FONT_HERSHEY_PLAIN, 1.5, cvScalar(0, 0, 0), 2.0);

	return imgHist;
}