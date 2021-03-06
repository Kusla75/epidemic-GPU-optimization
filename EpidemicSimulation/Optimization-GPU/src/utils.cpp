#include "utils.h"

// Global variables ----------------------------------------------

const std::string deviceType = "GPU";
const std::string resultsPathGPU = "../../Results/Optimization-GPU/";
const int numLocations = NUM_HOMES + NUM_WORKPLACES + POPULAR_PLACES;
const int maxLocationSize = 1500; // max number of people on any loaction

int locationsHost[numLocations * maxLocationSize]; // this array will be copied to GPU
Person people[NUM_PEOPLE];
std::vector<int> locations[numLocations];

compute::device gpu = compute::system::default_device();
compute::context context(gpu);
compute::command_queue queue(context, gpu, CL_QUEUE_PROFILING_ENABLE);

// ---------------------------------------------------------------

compute::buffer* CreateIntBuffer(int value) {

	compute::buffer* buff = new compute::buffer(context, sizeof(int));
	queue.enqueue_write_buffer(*buff, 0, sizeof(int), &value);
	queue.finish();

	return buff;
}

void UpdateIntBuffer(compute::buffer* buff, int value) {
	queue.enqueue_write_buffer(*buff, 0, sizeof(int), &value);
	queue.finish();
}

void BufferDayInfo(std::string& outputHistory, int simulationTime, compute::buffer* numInfectedBuff) {

	int numInfected, numRecovered, numDead, maxInfected;
	std::string output;

	queue.enqueue_read_buffer(*numInfectedBuff, 0, sizeof(int), &numInfected);
	queue.finish();

	output = "  Day " + std::to_string(simulationTime / DAY_DURATION) + " - active cases: " + std::to_string(numInfected) + "\n";
	std::cout << output;
	outputHistory += output;
}

void BufferSimulationEndInfo(std::string& outputHistory, compute::buffer* numInfectedBuff, compute::buffer* numRecoveredBuff,
	compute::buffer* numDeadBuff, compute::buffer* maxInfectedBuff, int executionTime){

	int numInfected, numRecovered, numDead, maxInfected;
	double value;
	std::string output;

	queue.enqueue_read_buffer(*maxInfectedBuff, 0, sizeof(int), &maxInfected);
	queue.enqueue_read_buffer(*numInfectedBuff, 0, sizeof(int), &numInfected);
	queue.enqueue_read_buffer(*numRecoveredBuff, 0, sizeof(int), &numRecovered);
	queue.enqueue_read_buffer(*numDeadBuff, 0, sizeof(int), &numDead);
	queue.finish();

	value = static_cast<double>(100 * maxInfected) / NUM_PEOPLE;
	output += "\nMax infected: " + std::to_string(maxInfected) + " - " + std::to_string(value) + "% of population\n";

	value = static_cast<double>(100 * numInfected) / NUM_PEOPLE;
	output += "\nInfected: " + std::to_string(numInfected) + " - " + std::to_string(value) + "% of population\n";

	value = static_cast<double>(100 * numRecovered) / NUM_PEOPLE;
	output += "Recovered: " + std::to_string(numRecovered) + " - " + std::to_string(value) + "% of population\n";

	value = static_cast<double>(100 * numDead) / NUM_PEOPLE;
	output += "Dead: " + std::to_string(numDead) + " - " + std::to_string(value) + "% of population\n";

	output += "\nTime: " + std::to_string(executionTime) + " ms\n";

	std::cout << output << std::endl;

	outputHistory += output;
}

void SetDeviceRandGenerators(compute::vector<MTRand>& randGeneratorsDevice, compute::kernel& InitGeneratorsKernel) {

	std::vector<int> seedsHost;
	compute::vector<int> seedsDevice(NUM_PEOPLE, context);
	for (int i = 0; i < NUM_PEOPLE; ++i) {
		seedsHost.push_back(i * 111);
	}

	compute::copy(seedsHost.begin(), seedsHost.end(), seedsDevice.begin(), queue);
	queue.finish();

	InitGeneratorsKernel.set_arg(0, randGeneratorsDevice);
	InitGeneratorsKernel.set_arg(1, seedsDevice);

	size_t global_dimensions[] = { NUM_PEOPLE, 0, 0 };
	size_t work_dim = 1;
	queue.enqueue_nd_range_kernel(InitGeneratorsKernel, work_dim, NULL, global_dimensions, NULL);
	queue.finish();

}

void InitData(compute::vector<Person>& peopleDevice, compute::vector<int>& numPeopleOnLocationsDevice, compute::buffer& locationsOnDevice) {
	
	std::default_random_engine generator(SEED);
	std::vector<int> numPeopleOnLocations;
	InitiateAgents(people, generator);
	SetAgentsHome(people, locations);
	InfectAgents(people, generator);

	for (int i = 0; i < numLocations; ++i) {
		numPeopleOnLocations.push_back(0);
	}

	for (int i = 0; i < numLocations * maxLocationSize; ++i) {
		locationsHost[i] = -1;
	}

	for (int i = 0; i < numLocations; ++i) {
		for (int j = 0; j < locations[i].size(); ++j) {
			locationsHost[i * maxLocationSize + j] = locations[i][j];
			numPeopleOnLocations[i]++;
		}
	}

	compute::copy(people, people + NUM_PEOPLE, peopleDevice.begin(), queue);
	queue.finish();
	compute::copy(numPeopleOnLocations.begin(), numPeopleOnLocations.end(), numPeopleOnLocationsDevice.begin(), queue);
	queue.finish();

	queue.enqueue_write_buffer(locationsOnDevice, 0, sizeof(int) * maxLocationSize * numLocations, locationsHost);
	queue.finish();
}

void SingleLocationBySingleThread() {
	
	// ----- Buffers and variables -----------------------------------------------------------------

	compute::vector<Person> peopleDevice(NUM_PEOPLE, context);
	compute::buffer locationsOnDevice(context, sizeof(int) * maxLocationSize * numLocations);
	compute::vector<int> numPeopleOnLocationsDevice(numLocations, context);
	compute::vector<MTRand> randGeneratorsDevice(NUM_PEOPLE, context);
	
	InitData(peopleDevice, numPeopleOnLocationsDevice, locationsOnDevice);

	compute::buffer* maxLocationSizeBuff = CreateIntBuffer(maxLocationSize);

	compute::buffer* NUM_PEOPLE_Buff = CreateIntBuffer(NUM_PEOPLE);
	compute::buffer* NUM_HOMES_Buff = CreateIntBuffer(NUM_HOMES);
	compute::buffer* NUM_WORKPLACES_Buff = CreateIntBuffer(NUM_WORKPLACES);
	compute::buffer* POPULAR_PLACES_Buff = CreateIntBuffer(POPULAR_PLACES);
	compute::buffer* WORK_HOURS_Buff = CreateIntBuffer(WORK_HOURS);
	compute::buffer* LOCATION_DURATION_Buff = CreateIntBuffer(LOCATION_DURATION);

	compute::buffer* INFECTION_PROBABILITY_Buff = CreateIntBuffer(INFECTION_PROBABILITY * 100000);
	compute::buffer* FATALITY_RATE_Buff = CreateIntBuffer(FATALITY_RATE * 100000);
	compute::buffer* INFECTION_DURATION_Buff = CreateIntBuffer(INFECTION_DURATION);
	compute::buffer* IMMUNITY_DURATION_Buff = CreateIntBuffer(IMMUNITY_DURATION);
	compute::buffer* DAY_DURATION_Buff = CreateIntBuffer(DAY_DURATION);

	compute::buffer* numInfectedBuff = CreateIntBuffer(Person::numInfected);
	compute::buffer* numRecoveredBuff = CreateIntBuffer(Person::numRecovered);
	compute::buffer* numDeadBuff = CreateIntBuffer(Person::numDead);
	compute::buffer* maxInfectedBuff = CreateIntBuffer(Person::maxInfected);

	int i = 0, simulationTime = 0, dayDuration = 0;
	std::string outputHistory;
	compute::buffer* dayDurationBuff = CreateIntBuffer(dayDuration);

	// ----- Preparing data and kernels ---------------------------------------------------------------

	compute::program funcProgram = compute::program::create_with_source_file("./cl/func.cl", context);

	try {
		funcProgram.build("-I ./cl/");
	}
	catch (boost::compute::opencl_error& e) {
		std::cout << funcProgram.build_log() << std::endl;
		std::cout << std::endl;
	}

	// Setting kernels

	compute::kernel InitGeneratorsKernel = funcProgram.create_kernel("InitGenerators");
	SetDeviceRandGenerators(randGeneratorsDevice, InitGeneratorsKernel);

	compute::kernel MakeInteractionsKernel = funcProgram.create_kernel("MakeInteractions");
	MakeInteractionsKernel.set_arg(0, sizeof(locationsOnDevice), &locationsOnDevice);
	MakeInteractionsKernel.set_arg(1, sizeof(*maxLocationSizeBuff), maxLocationSizeBuff);
	MakeInteractionsKernel.set_arg(2, numPeopleOnLocationsDevice);
	MakeInteractionsKernel.set_arg(3, peopleDevice);
	MakeInteractionsKernel.set_arg(4, randGeneratorsDevice);
	MakeInteractionsKernel.set_arg(5, sizeof(*INFECTION_PROBABILITY_Buff), INFECTION_PROBABILITY_Buff);
	MakeInteractionsKernel.set_arg(6, sizeof(*FATALITY_RATE_Buff), FATALITY_RATE_Buff);
	MakeInteractionsKernel.set_arg(7, sizeof(*numInfectedBuff), numInfectedBuff);
	MakeInteractionsKernel.set_arg(8, sizeof(*maxInfectedBuff), maxInfectedBuff);

	compute::kernel CheckAgentsStatusKernel = funcProgram.create_kernel("CheckAgentsStatus");
	CheckAgentsStatusKernel.set_arg(0, peopleDevice);
	CheckAgentsStatusKernel.set_arg(1, randGeneratorsDevice);
	CheckAgentsStatusKernel.set_arg(2, sizeof(*INFECTION_DURATION_Buff), INFECTION_DURATION_Buff);
	CheckAgentsStatusKernel.set_arg(3, sizeof(*FATALITY_RATE_Buff), FATALITY_RATE_Buff);
	CheckAgentsStatusKernel.set_arg(4, sizeof(*IMMUNITY_DURATION_Buff), IMMUNITY_DURATION_Buff);
	CheckAgentsStatusKernel.set_arg(5, sizeof(*numInfectedBuff), numInfectedBuff);
	CheckAgentsStatusKernel.set_arg(6, sizeof(*numRecoveredBuff), numRecoveredBuff);
	CheckAgentsStatusKernel.set_arg(7, sizeof(*numDeadBuff), numDeadBuff);
	
	compute::kernel ChangeAgentsLocationKernel = funcProgram.create_kernel("ChangeAgentsLocation");
	ChangeAgentsLocationKernel.set_arg(0, sizeof(locationsOnDevice), &locationsOnDevice);
	ChangeAgentsLocationKernel.set_arg(1, sizeof(*maxLocationSizeBuff), maxLocationSizeBuff);
	ChangeAgentsLocationKernel.set_arg(2, peopleDevice);
	ChangeAgentsLocationKernel.set_arg(3, numPeopleOnLocationsDevice);
	ChangeAgentsLocationKernel.set_arg(4, randGeneratorsDevice);
	ChangeAgentsLocationKernel.set_arg(5, sizeof(*dayDurationBuff), dayDurationBuff);
	ChangeAgentsLocationKernel.set_arg(6, sizeof(*WORK_HOURS_Buff), WORK_HOURS_Buff);
	ChangeAgentsLocationKernel.set_arg(7, sizeof(*LOCATION_DURATION_Buff), LOCATION_DURATION_Buff);
	ChangeAgentsLocationKernel.set_arg(8, sizeof(*NUM_HOMES_Buff), NUM_HOMES_Buff);
	ChangeAgentsLocationKernel.set_arg(9, sizeof(*NUM_WORKPLACES_Buff), NUM_WORKPLACES_Buff);
	ChangeAgentsLocationKernel.set_arg(10, sizeof(*POPULAR_PLACES_Buff), POPULAR_PLACES_Buff);

	compute::kernel MoveAgentsToLocationsKernel = funcProgram.create_kernel("MoveAgentsToLocationsParallel");
	MoveAgentsToLocationsKernel.set_arg(0, sizeof(locationsOnDevice), &locationsOnDevice);
	MoveAgentsToLocationsKernel.set_arg(1, sizeof(*maxLocationSizeBuff), maxLocationSizeBuff);
	MoveAgentsToLocationsKernel.set_arg(2, numPeopleOnLocationsDevice);
	MoveAgentsToLocationsKernel.set_arg(3, peopleDevice);
	MoveAgentsToLocationsKernel.set_arg(4, sizeof(*NUM_PEOPLE_Buff), NUM_PEOPLE_Buff);

	compute::kernel UpdateDayDurationKernel = funcProgram.create_kernel("UpdateDayDuration");
	UpdateDayDurationKernel.set_arg(0, sizeof(*dayDurationBuff), dayDurationBuff);
	UpdateDayDurationKernel.set_arg(1, sizeof(*DAY_DURATION_Buff), DAY_DURATION_Buff);

	// ----- Main -------------------------------------------------------------------------------------

	queue.finish();

	std::cout << "DEVICE TYPE: " << deviceType << "\n" << std::endl;
	std::cout << "Simulation start: \n" << std::endl;
	int executionTime = 0;
	compute::event event;

	while (simulationTime < SIMULATION_DURATION * DAY_DURATION) {
		
		event = queue.enqueue_1d_range_kernel(ChangeAgentsLocationKernel, NULL, numLocations, NULL);
		queue.finish();
		executionTime += event.duration<std::chrono::milliseconds>().count();

		event = queue.enqueue_1d_range_kernel(MoveAgentsToLocationsKernel, NULL, NUM_PEOPLE, NULL);
		queue.finish();
		executionTime += event.duration<std::chrono::milliseconds>().count();
		//queue.enqueue_task(MoveAgentsToLocationsKernel); // execution on single thread

		while (i < NUM_INTERACTIONS) {
			event = queue.enqueue_1d_range_kernel(MakeInteractionsKernel, NULL, numLocations, NULL);
			queue.finish();
			executionTime += event.duration<std::chrono::milliseconds>().count();
			++i;
		}
		i = 0;
		dayDuration += 1;
		queue.enqueue_task(UpdateDayDurationKernel);
		queue.finish();

		if (dayDuration == DAY_DURATION) {
			simulationTime += dayDuration;
			dayDuration = 0;
			queue.enqueue_task(UpdateDayDurationKernel);
			queue.finish();

			event = queue.enqueue_1d_range_kernel(CheckAgentsStatusKernel, NULL, NUM_PEOPLE, NULL);
			queue.finish();
			executionTime += event.duration<std::chrono::milliseconds>().count();
			//BufferDayInfo(outputHistory, simulationTime, numInfectedBuff);
			std::cout << "  Day " << simulationTime / DAY_DURATION << std::endl;
		}
	}

	BufferSimulationEndInfo(outputHistory, numInfectedBuff, numRecoveredBuff, numDeadBuff, maxInfectedBuff, executionTime);

	LogSimulationParameters(outputHistory, GetCurrentDate(), resultsPathGPU, "GPU");
}