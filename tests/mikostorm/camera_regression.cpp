#include "llcamerajointselector.h"
#include "camera_services_stub.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main()
{
    try
    {
        LLComboBox controls,preferences;
        LLCameraJointSelector first,second;
        first.bind(&controls); second.bind(&preferences);
        require(controls.rows.size()==1 && controls.rows[0].second==-1,"no-avatar None entry");
        cameraJoints={{"None",-1},{"mHead",42},{"mPelvis",7}};
        LLTimer::now=1.1;
        first.refresh(); second.refresh();
        require(controls.rows==cameraJoints && preferences.rows==cameraJoints,"late skeleton numeric IDs");
        controls.choose(42);
        require(gSavedSettings.selected==42,"selection must persist numeric joint ID");
        second.refresh();
        require(preferences.getValue().asInteger()==42,"controls must synchronize");
        first.refresh();
        const int writes=controls.selectionWrites;
        controls.value=LLSD(7); // Simulate a highlighted row while the popup is open.
        first.refresh(); first.refresh();
        require(controls.selectionWrites==writes && controls.getValue().asInteger()==7,"refresh must not reset popup highlight");
        cameraJoints[2]={"newBone",99};
        LLTimer::now=2.2;
        first.refresh();
        require(controls.rows[2].second==99,"same-sized changed skeleton must rebuild");
        gSavedSettings.selected=-1;
        second.refresh();
        require(preferences.getValue().asInteger()==-1,"external setting must synchronize");
        LLCameraJointSelector recreated;
        LLComboBox fresh;
        recreated.bind(&fresh);
        require(fresh.rows==cameraJoints,"recreated floater must initialize independently");
        std::cout << "Camera selector: 8 behavioral checks passed\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
