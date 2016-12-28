// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2015 Qianyi Zhou <Qianyi.Zhou@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include <Core/Core.h>
#include <IO/IO.h>

#include <limits>

void PrintHelp()
{
	printf("Usage:\n");
	printf("    > ConvertPointCloud source_file target_file [options]\n");
	printf("    > ConvertPointCloud source_directory target_directory [options]\n");
	printf("      Read point cloud from source file and convert it to target file.\n");
	printf("\n");
	printf("Options (listed in the order of execution priority):\n");
	printf("    --help, -h                : Print help information.\n");
	printf("    --verbose n               : Set verbose level (0-4).\n");
	printf("    --clip_x_min x0           : Clip points with x coordinate < x0.\n");
	printf("    --clip_x_max x1           : Clip points with x coordinate > x1.\n");
	printf("    --clip_y_min y0           : Clip points with y coordinate < y0.\n");
	printf("    --clip_y_max y1           : Clip points with y coordinate > y1.\n");
	printf("    --clip_z_min z0           : Clip points with z coordinate < z0.\n");
	printf("    --clip_z_max z1           : Clip points with z coordinate > z1.\n");
	printf("    --voxel_sample voxel_size : Downsample the point cloud with a voxel.\n");
	printf("    --estimate_normals radius : Estimate normals using a search neighborhood of\n");
	printf("                                radius. The normals are oriented w.r.t. the\n");
	printf("                                original normals of the pointcloud if they\n");
	printf("                                exist. Otherwise, they are oriented towards -Z\n");
	printf("                                direction.\n");
	printf("    --orient_normals [x,y,z]  : Orient the normals w.r.t the direction [x,y,z].\n");
}

void convert(int argc, char **argv, const std::string &file_in,
		const std::string &file_out)
{
	using namespace three;
	using namespace three::filesystem;
	auto pointcloud_ptr = CreatePointCloudFromFile(file_in.c_str());
	size_t point_num_in = pointcloud_ptr->points_.size();
	bool processed = false;

	// clip
	if (ProgramOptionExistsAny(argc, argv, {"--clip_x_min", "--clip_x_max",
			"--clip_y_min", "--clip_y_max", "--clip_z_min", "--clip_z_max"})) {
		Eigen::Vector3d min_bound, max_bound;
		min_bound(0) = GetProgramOptionAsDouble(argc, argv, "--clip_x_min",
				std::numeric_limits<double>::lowest());
		min_bound(1) = GetProgramOptionAsDouble(argc, argv, "--clip_y_min",
				std::numeric_limits<double>::lowest());
		min_bound(2) = GetProgramOptionAsDouble(argc, argv, "--clip_z_min",
				std::numeric_limits<double>::lowest());
		max_bound(0) = GetProgramOptionAsDouble(argc, argv, "--clip_x_max",
				std::numeric_limits<double>::max());
		max_bound(1) = GetProgramOptionAsDouble(argc, argv, "--clip_y_max",
				std::numeric_limits<double>::max());
		max_bound(2) = GetProgramOptionAsDouble(argc, argv, "--clip_z_max",
				std::numeric_limits<double>::max());
		auto clip_ptr = std::make_shared<PointCloud>();
		CropPointCloud(*pointcloud_ptr, min_bound, max_bound, *clip_ptr);
		pointcloud_ptr = clip_ptr;
		processed = true;
	}
	
	// voxel_downsample
	double voxel_size = GetProgramOptionAsDouble(argc, argv, "--voxel_sample",
			0.0);
	if (voxel_size > 0.0) {
		PrintDebug("Downsample point cloud with voxel size %.4f.\n", voxel_size);
		auto downsample_ptr = std::make_shared<PointCloud>();
		VoxelDownSample(*pointcloud_ptr, voxel_size, *downsample_ptr);
		pointcloud_ptr = downsample_ptr;
		processed = true;
	}

	// estimate_normals
	double radius = GetProgramOptionAsDouble(argc, argv, "--estimate_normals",
			0.0);
	if (radius > 0.0) {
		PrintDebug("Estimate normals with search radius %.4f.\n", radius);
		if (pointcloud_ptr->HasNormals()) {
			EstimateNormals(*pointcloud_ptr, KDTreeSearchParamRadius(radius));
		} else {
			EstimateNormals(*pointcloud_ptr, Eigen::Vector3d(0, 0, -1),
					KDTreeSearchParamRadius(radius));
		}
		processed = true;
	}

	// orient_normals
	Eigen::VectorXd direction = GetProgramOptionAsEigenVectorXd(argc, argv,
			"--orient_normals");
	if (direction.size() == 3 && pointcloud_ptr->HasNormals()) {
		PrintDebug("Orient normals to [%.2f, %.2f, %.2f].\n", direction(0),
				direction(1), direction(2));
		Eigen::Vector3d dir(direction);
		for (auto &normal : pointcloud_ptr->normals_) {
			if (normal.dot(dir) < 0.0) {
				normal *= -1.0;
			}
		}
	}

	size_t point_num_out = pointcloud_ptr->points_.size();
	if (processed) {
		PrintInfo("Processed point cloud from %d points to %d points.\n", 
				(int)point_num_in, (int)point_num_out);
	}
	WritePointCloud(file_out.c_str(), *pointcloud_ptr, false, true);
}

int main(int argc, char **argv)
{
	using namespace three;
	using namespace three::filesystem;

	if (argc < 3 || ProgramOptionExists(argc, argv, "--help") ||
			ProgramOptionExists(argc, argv, "-h")) {
		PrintHelp();
		return 0;
	}
	
	int verbose = GetProgramOptionAsInt(argc, argv, "--verbose", 2);
	SetVerbosityLevel((VerbosityLevel)verbose);

	if (FileExists(argv[1])) {
		convert(argc, argv, argv[1], argv[2]);
	} else if (DirectoryExists(argv[1])) {
		MakeDirectoryHierarchy(argv[2]);
		std::vector<std::string> filenames;
		ListFilesInDirectory(argv[1], filenames);
		for (const auto &fn : filenames) {
			convert(argc, argv, fn, GetRegularizedDirectoryName(argv[2]) + 
					GetFileNameWithoutDirectory(fn));
		}
	} else {
		PrintError("File or directory does not exist.\n");
	}

	return 1;
}