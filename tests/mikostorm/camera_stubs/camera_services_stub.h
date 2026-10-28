#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>
using S32=int;
using camera_joint_list_t=std::vector<std::pair<std::string,S32>>;
inline camera_joint_list_t cameraJoints{{"None",-1}};
inline camera_joint_list_t getCameraJointList() { return cameraJoints; }
class LLSD
{
    int mValue;
public:
    LLSD(int value=0):mValue(value){}
    int asInteger() const { return mValue; }
};
class LLUICtrl
{
public:
    LLSD value;
    LLSD getValue() const { return value; }
};
class LLComboBox:public LLUICtrl
{
public:
    camera_joint_list_t rows;
    int selectionWrites=0;
    std::function<void(LLUICtrl*,const LLSD&)> callback;
    void setCommitCallback(std::function<void(LLUICtrl*,const LLSD&)> fn) { callback=fn; }
    void clearRows() { rows.clear(); }
    void add(const std::string& label,int id) { rows.emplace_back(label,id); }
    void setValue(int id) { value=LLSD(id); ++selectionWrites; }
    void choose(int id) { value=LLSD(id); callback(this,value); }
};
struct CameraSettings
{
    int selected=-1;
    int getS32(const std::string&) const { return selected; }
    void setS32(const std::string&,int id) { selected=id; }
};
inline CameraSettings gSavedSettings;
class LLTimer
{
    double mStart=now;
public:
    inline static double now=0;
    float getElapsedTimeF32() const { return static_cast<float>(now-mStart); }
    void reset() { mStart=now; }
};
