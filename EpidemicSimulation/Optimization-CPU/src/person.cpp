#include "person.h"

int Person::numInfected = 0;
int Person::numRecovered = 0;
int Person::numDead = 0;

int Person::maxInfected = 0;
int Person::newInfected = 0;
int Person::maxNewInfected = 0;

Person::Person() {
	this->homeID = -1;
	this->workID = -1;
	this->currentLocation = -1;
	this->infectionDays = 0;
	this->immunityDays = 0;
	this->wasInfected = false;
	this->distancing = false;
	this->status = S;
}

Person::Person(int homeID, int workID, int currentLocation, int infectionDays, bool immunityDays, 
	bool wasInfected, bool distancing, Status status) {

	this->homeID = homeID;
	this->workID = workID;
	this->currentLocation = currentLocation;
	this->infectionDays = infectionDays;
	this->distancing = distancing;
	this->status = status;
}

void Person::setHomeID(int homeID) { this->homeID = homeID; }

void Person::setWorkID(int workID) { this->workID = workID; }

void Person::setCurrentLocation(int currentLocation) { this->currentLocation = currentLocation; }

void Person::setInfectionDays(int infectionDays) { this->infectionDays = infectionDays; }

void Person::setImmunityDays(int immunityDays) { this->immunityDays = immunityDays; }

void Person::setWasInfected(bool wasInfected) { this->wasInfected = wasInfected; }

void Person::setDistancing(bool distancing) { this->distancing = distancing; }

void Person::setStatus(Status status) { this->status = status; }

void Person::ExtendInfectionDay() { this->infectionDays++; }

void Person::ExtendImmunityDay() { this->immunityDays++; }

bool Person::TryInfect(std::default_random_engine& generator, int infectionProbability) {
	std::uniform_int_distribution<int> distribution(1, 100000);

	int rand = distribution(generator);
	//rngVector.push_back(rand);
	if (rand <= infectionProbability) {
		this->status = I;
		if (!this->wasInfected) {
			this->wasInfected = true;
			numInfected++;
			maxInfected++;
			newInfected++;
		}
		else {
			numInfected++;
			newInfected++;
		}
		return true;
	}
	else {
		return false;
	}
}

bool Person::TryKill(std::default_random_engine& generator, int deathProbability) {
	std::uniform_int_distribution<int> distribution(1, 100000);

	int rand = distribution(generator);
	//rngVector.push_back(rand);
	if (rand <= deathProbability) {
		this->status = D;
		numDead++;
		numInfected--;
		return true;
	}
	else {
		return false;
	}
}

void Person::RecoverAgent() {
	this->status = R;
	this->infectionDays = 0;
	numRecovered++;
	numInfected--;
}

void Person::MakeAgentSusceptible() {
	this->status = S;
	this->immunityDays = 0;
	numRecovered--;
}

void Person::changeMaxNewInfected() {
	if (newInfected > maxNewInfected) {
		maxNewInfected = newInfected;
	}
	newInfected = 0;
}