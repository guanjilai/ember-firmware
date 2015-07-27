/* 
 * File:   LayerSettings.h
 * Author: Richard Greene
 * 
 * Handles overrides to settings on a per-layer basis, as defined in a CSV file.
 * 
 * Created on May 22, 2015, 2:28 PM
 */

#ifndef LAYERSETTINGS_H
#define	LAYERSETTINGS_H

#include <vector>
#include <map>
#include <string>

class LayerSettings {
public:
    virtual ~LayerSettings();
    bool Load(const std::string& layerParams);
    int GetInt(int layer, std::string name);
    double GetDouble(int layer, std::string name);
    void Clear();
    
private:
    std::map<std::string, int> _columns;
    std::map<int, std::vector<double> > _rows;
    std::string Trim(std::string);
    double GetRawValue(int layer, std::string name);

};

/// Holds the values of all print settings to use for a single layer 
struct CurrentLayerSettings
{
    // listed here in the order in which they're used
    int PressMicrons;
    int PressMicronsPerSec;
    int PressWaitMS;
    int UnpressMicronsPerSec;
    int ApproachWaitMS;
    double ExposureSec;
    int SeparationRotJerk;
    int SeparationRPM;
    int RotationMilliDegrees;
    int SeparationZJerk;
    int SeparationMicronsPerSec;
    int ZLiftMicrons;
    int ApproachRotJerk;
    int ApproachRPM;
    int ApproachZJerk;
    int ApproachMicronsPerSec;
    int LayerThicknessMicrons;
    
    // these are included to avoid changes while pause & inspect is in progress
    bool CanInspect;
    int InspectionHeightMicrons;
};

#endif	/* LAYERSETTINGS_H */

