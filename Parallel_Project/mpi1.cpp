#undef SEEK_SET
#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <iostream>
#include <stdint.h>
#include <ctime>
#include <sstream>
#include <string>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc.hpp"
using namespace cv;
using namespace std;

const int pic_num = 50;

int main(int argc, char** argv)
{
	MPI::Init(argc, argv);

	int id = MPI::COMM_WORLD.Get_rank();
	int Psz = MPI::COMM_WORLD.Get_size();
	int P_cnt = pic_num / Psz + (pic_num % Psz > 0 ? 1 : 0);

	Mat img_hist_equalized[pic_num + 1];
	uint8_t** pixelPtr;
	pixelPtr = new uint8_t*[pic_num + 1];
	int Number_of_Channels = 0, Cols = 0, Rows = 0;
	stringstream sin;
	string s;
	int sz;

	double start = clock();



	//=== reading the images
	for (int i = 0; i < pic_num; i++)
		sin << i + 1 << endl;
	sin << pic_num << endl;

	for (int i = 0; i < pic_num; i++)
	{
		sin >> s;
		img_hist_equalized[i] = imread("img\\" + s + ".jpg", CV_LOAD_IMAGE_UNCHANGED); //read the image data in the file "MyPic.JPG" and store it in 'img'

		if (img_hist_equalized[i].empty()) //check whether the image is loaded or not
		{
			cout << id << " Error : Image cannot be loaded..!! " << s << endl;
			//system("pause"); //wait for a key press
			return -1;
		}
		pixelPtr[i] = (uint8_t*)img_hist_equalized[i].data;
	}
	//==end reading the images

	sz = img_hist_equalized[pic_num - 1].cols*img_hist_equalized[pic_num - 1].rows*img_hist_equalized[pic_num - 1].channels();
	P_cnt = (sz / Psz) + (sz%Psz > 0 ? 1 : 0);

	//======== back ground subtraction
	int* sums = new int[sz];
	for (int i = 0; i < P_cnt; i++)
		sums[i] = 0;

	int mini = (id*P_cnt), maxi = min((int)sz, ((id + 1)*P_cnt));
	for (int k = 0; k < pic_num; k++)
	for (int i = 0, j = mini; i < sz && j < maxi; i++, j++)
		sums[i] += pixelPtr[k][j];


	for (int i = 0; i < P_cnt; i++)
		sums[i] /= pic_num;
	P_cnt = maxi - mini;

	if (!id)
		MPI::COMM_WORLD.Isend(&sums, P_cnt, MPI::INT, 0, 0);

	//MPI::COMM_WORLD.Barrier();

	if (id == 0)
	{
		for (int i = 1; i < Psz; i++)
		{
			int r = min(P_cnt, sz - (i*P_cnt));
			MPI::COMM_WORLD.Irecv(&sums + (i*P_cnt), r, MPI::INT, i, 0);
		}

		MPI::COMM_WORLD.Barrier();

		//== bG subtraction
		for (int j = 0; j < pic_num; j++)
		for (int i = 0; i < sz; i++)
			pixelPtr[j][i] = (sums[i] > pixelPtr[j][i] ? 0 : (pixelPtr[j][i] - sums[i]));

		double end = clock();
		cout << "Using " << pic_num << " images the time is " << (end - start) / double(CLOCKS_PER_SEC) << endl;

		vector<int> compression_params;
		compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
		compression_params.push_back(90);

		sin << (end - start) / double(CLOCKS_PER_SEC) << endl;
		sin >> s;
		string T;
		sin >> T;
		stringstream fin;
		for (int i = 0; i < pic_num; i++)
			fin << i + 1 << endl;
		string S;
		for (int i = 0; i < pic_num; i++)
		{
			fin >> S;
			bool ok = imwrite("OUTPUT\\" + S + "_res_" + s + "_" + T + "_omp2.jpg", img_hist_equalized[i], compression_params);
			if (!ok)
				cout << "can't write image" << endl;
		}



		//namedWindow("imagenew", CV_WINDOW_AUTOSIZE);
		//imshow("imagenew", img_hist_equalized[pic_num - 1]);
	}

	MPI::Finalize();
	waitKey();
	return 0;
}