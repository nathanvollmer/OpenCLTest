//Following https://anteru.net/2012/11/03/2009/

#include <utility>
#include <CL\cl.hpp>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <iterator>

std::string GetPlatformName(cl_platform_id id)
{
	size_t size = 0;
	clGetPlatformInfo(id, CL_PLATFORM_NAME, 0, nullptr, &size);

	std::string result;
	result.resize(size);
	clGetPlatformInfo(id, CL_PLATFORM_NAME, size,
		const_cast<char*> (result.data()), nullptr);

	return result;
}

std::string GetDeviceName(cl_device_id id)
{
	size_t size = 0;
	clGetDeviceInfo(id, CL_DEVICE_NAME, 0, nullptr, &size);

	std::string result;
	result.resize(size);
	clGetDeviceInfo(id, CL_DEVICE_NAME, size,
		const_cast<char*> (result.data()), nullptr);

	return result;
}

void CheckError(cl_int error)
{
	if (error != CL_SUCCESS) {
		std::cerr << "OpenCL call failed with error " << error << std::endl;
		std::exit(1);
	}
}

std::string LoadKernel(const char* name)
{
	std::ifstream in(name);
	std::string result(
		(std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
	return result;
}

cl_program CreateProgram(const std::string& source, cl_context context)
{
	size_t lengths[1] = { source.size() };
	const char* sources[1] = { source.data() };
	
	cl_int error = 0;
	cl_program program = clCreateProgramWithSource(context, 1, sources, lengths, &error);
	CheckError(error);
	
	return program;
}


int main()
{
	//--------------------------------- GETTING PLATFORM IDS --------------------------------------
	// http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/clGetPlatformIDs.html
#pragma region Getting_Platform_IDS
	cl_uint platformIdCount = 0;
	clGetPlatformIDs(0, nullptr, &platformIdCount);

	if (platformIdCount == 0) {
		std::cerr << "No OpenCL platform found" << std::endl;
		return 1;
	}
	else {
		std::cout << "Found " << platformIdCount << " platform(s)" << std::endl;
	}

	std::vector<cl_platform_id> platformIds(platformIdCount);
	clGetPlatformIDs(platformIdCount, platformIds.data(), nullptr);

	for (cl_uint i = 0; i < platformIdCount; ++i) {
		std::cout << "\t (" << (i + 1) << ") : " << GetPlatformName(platformIds[i]) << std::endl;
	}
#pragma endregion Getting_Platform_IDS


	//---------------------------------- GETTING DEVICE IDS ---------------------------------------
	// http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/clGetDeviceIDs.html
#pragma region Getting_Device_IDS
	cl_uint deviceIdCount = 0;
	clGetDeviceIDs(platformIds[platformIds.size() - 1], CL_DEVICE_TYPE_ALL, 0, nullptr,
		&deviceIdCount);

	if (deviceIdCount == 0) {
		std::cerr << "No OpenCL devices found" << std::endl;
		return 1;
	}
	else {
		std::cout << "Found " << deviceIdCount << " device(s) for " + GetPlatformName(platformIds[platformIds.size() - 1]) << std::endl;
	}

	std::vector<cl_device_id> deviceIds(deviceIdCount);
	clGetDeviceIDs(platformIds[platformIds.size() - 1], CL_DEVICE_TYPE_ALL, deviceIdCount,
		deviceIds.data(), nullptr);

	for (cl_uint i = 0; i < deviceIdCount; ++i) {
		std::cout << "\t (" << (i + 1) << ") : " << GetDeviceName(deviceIds[i]) << std::endl;
	}
#pragma endregion Getting_Device_IDS


	//------------------------------------ CREATING CONTEXT ----------------------------------------
	// http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/clCreateContext.html
#pragma region Creating_Context	
	std::cout << "Creating a context: ";
	const cl_context_properties contextProperties[] =
	{
		CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties> (platformIds[platformIds.size() - 1]),
		0, 0
	};

	cl_int error = CL_SUCCESS;
	cl_context context = clCreateContext(contextProperties, deviceIdCount,
		deviceIds.data(), nullptr, nullptr, &error);
	CheckError(error);

	std::cout << "SUCCESS" << std::endl;

#pragma endregion Creating_Context


	//--------------------------------- CREATING COMMAND QUEUE -------------------------------------
	// http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/clCreateCommandQueue.html
#pragma region Creating_Command_Queue
	std::cout << "Creating a command queue: ";

	cl_command_queue queue = clCreateCommandQueue(context, deviceIds[0],
		0, &error);
	CheckError(error);
	std::cout << "SUCCESS" << std::endl;

#pragma endregion Creating_Command_Queue

	//------------------------------------ CREATING PROGRAM ----------------------------------------
#pragma region Creating_Program	
	std::cout << "Creating a program kernel: ";

	cl_program program = CreateProgram(LoadKernel("kernels.cl"), context);

	CheckError(clBuildProgram(program, deviceIdCount, deviceIds.data(), nullptr, nullptr, nullptr));

	//Predication
	cl_kernel predicateKernel = clCreateKernel(program, "Predicate", &error);
	CheckError(error);
	std::cout << "SUCCESS" << std::endl;
	//Scanning
	cl_kernel scanKernel = clCreateKernel(program, "Scan", &error);
	CheckError(error);
	std::cout << "SUCCESS" << std::endl;
	//Compaction
	cl_kernel compactKernel = clCreateKernel(program, "Compact", &error);
	CheckError(error);
	std::cout << "SUCCESS" << std::endl;


	// Prepare some test data
	std::cout << "Preparing test data: ";
	static const size_t inputSize = 1 << 9;
	std::vector<int> input(inputSize), predicate(inputSize), address(inputSize), result(inputSize);
	for (int i = 0; i < inputSize; ++i) {
		input[i] = static_cast<int> (i);
		std::cout << "\n" + std::to_string(input[i]);
	}

	cl_mem inputBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof (int)* (inputSize),
		input.data(), &error);
	cl_mem predicateBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof (int)* (inputSize),
		predicate.data(), &error);
	cl_mem addressBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof (int)* (inputSize),
		address.data(), &error);
	cl_mem resultBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof (int)* (inputSize),
		result.data(), &error);
	CheckError(error);


	//----------------------------------- DEFINING ARGUMENTS ---------------------------------------
	#pragma region Defining_Arguments	
	//Predication
	clSetKernelArg(predicateKernel, 0, sizeof (cl_mem), &inputBuffer);
	clSetKernelArg(predicateKernel, 1, sizeof (cl_mem), &predicateBuffer);

	//Scanning
	clSetKernelArg(scanKernel, 0, sizeof (cl_mem), &predicateBuffer);
	clSetKernelArg(scanKernel, 1, sizeof (cl_mem), &addressBuffer);
	clSetKernelArg(scanKernel, 2, inputSize * sizeof(cl_int), NULL);
	clSetKernelArg(scanKernel, 3, inputSize * sizeof(cl_int), NULL);

	//Compaction
	clSetKernelArg(compactKernel, 0, sizeof (cl_mem), &inputBuffer);
	clSetKernelArg(compactKernel, 1, sizeof (cl_mem), &predicateBuffer);
	clSetKernelArg(compactKernel, 2, sizeof (cl_mem), &addressBuffer);
	clSetKernelArg(compactKernel, 3, sizeof (cl_mem), &resultBuffer);
	#pragma endregion Defining_Arguments	
	

	//----------------------------------- DEFINING DIMENSION ---------------------------------------
	// http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/clEnqueueNDRangeKernel.html
	#pragma region Defining_Dimension
	const size_t globalWorkSize[] = { inputSize, 0, 0 };
	const size_t localWorkSize[] = { inputSize, 0, 0 };
	CheckError(clEnqueueNDRangeKernel(queue, predicateKernel, 1,
		nullptr,
		globalWorkSize,
		localWorkSize,
		0, nullptr, nullptr));
	CheckError(clEnqueueNDRangeKernel(queue, scanKernel, 1,
		nullptr,
		globalWorkSize,
		localWorkSize,
		0, nullptr, nullptr));
	CheckError(clEnqueueNDRangeKernel(queue, compactKernel, 1,
		nullptr,
		globalWorkSize,
		localWorkSize,
		0, nullptr, nullptr));
	#pragma endregion Defining_Dimension


	//----------------------------------- RETRIEVING RESULTS ---------------------------------------
	// http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/clEnqueueReadBuffer.html
	#pragma region Retrieving_Result
	CheckError(clEnqueueReadBuffer(queue, resultBuffer, CL_TRUE, 0,
		inputSize * sizeof (int),
		result.data(),
		0, nullptr, nullptr));
	#pragma endregion Retrieving_Results

	std::cout << "\nResult:" << std::endl;
	
	for (int i = 0; i < inputSize/2; ++i){
		std::cout << std::to_string(result[i]) + "\n";
	}

	clReleaseCommandQueue(queue);
	clReleaseMemObject(addressBuffer);
	clReleaseMemObject(predicateBuffer);
	clReleaseMemObject(inputBuffer);
	clReleaseKernel(scanKernel);
	clReleaseKernel(predicateKernel);
	clReleaseProgram(program);
	clReleaseContext(context);

	std::cout << "\nDone! Press enter to exit.";
	std::getchar();
}