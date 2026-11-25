#include "DriveMissionRules.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
	using namespace DriveMissionRules;
	assert(ActiveCheckpoint(0,0,3,false,false,false));
	assert(!ActiveCheckpoint(-1,-1,3,false,false,false));
	assert(!ActiveCheckpoint(1,0,3,false,false,false));
	assert(!ActiveCheckpoint(3,3,3,false,false,false));
	assert(!ActiveCheckpoint(0,0,3,true,false,false));
	assert(!ActiveCheckpoint(0,0,3,false,true,false));
	assert(!ActiveCheckpoint(0,0,3,false,false,true));
	assert(ValidStop(2.f,25.f,true));
	assert(!ValidStop(2.01f,0.f,true));
	assert(!ValidStop(0.f,25.01f,true));
	assert(ValidStop(0.f,160.f,false));
	assert(!ValidStop(std::numeric_limits<float>::quiet_NaN(),0.f,true));
	assert(InParkingBay(250.f,75.f));
	assert(!InParkingBay(251.f,0.f));
	assert(!InParkingBay(0.f,76.f));
	float Hold=0.f;
	for(int i=0;i<19;++i) Hold=AccumulateHold(Hold,.1f,true);
	assert(!HoldComplete(Hold,2.f));
	Hold=AccumulateHold(Hold,.1f,true);
	assert(HoldComplete(Hold,2.f));
	Hold=AccumulateHold(Hold,.1f,false);
	assert(Hold==0.f); // Moving or leaving the bay breaks the uninterrupted hold.
	assert(AccumulateHold(0.f,8.f,true)==.25f); // A hitch cannot instantly pass a stop.
	assert(AccumulateHold(0.f,-1.f,true)==0.f);
	assert(Stars(false,0,0.f,0,100.f,100.f)==0);
	assert(Stars(true,0,0.f,0,20.f,100.f)==3);
	assert(Stars(true,0,0.f,0,19.f,100.f)==2);
	assert(Stars(true,1,0.f,0,100.f,100.f)==2);
	assert(Stars(true,0,1.f,0,100.f,100.f)==2);
	assert(Stars(true,0,3.01f,0,100.f,100.f)==1);
	assert(Stars(true,0,0.f,2,100.f,100.f)==2);
	std::cout << "Mission rules: 27 checks passed\n";
}
