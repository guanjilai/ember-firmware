/* 
 * File:   LayerSettings.cpp
 * Author: Richard Greene
 * 
 * Handles overrides to settings on a per-layer basis, as defined in a CSV file.
 * 
 * Created on May 22, 2015, 2:28 PM
 */

#include <fstream>
#include <sstream>

#include <LayerSettings.h>
#include <Logger.h>
#include <Settings.h>

using std::string;
using std::map;
using std::vector;
using std::ifstream;
using std::stringstream;

LayerSettings::LayerSettings() {
}

LayerSettings::LayerSettings(const LayerSettings& orig) {
}

LayerSettings::~LayerSettings() {
}

/// Load per-layer settings overrides from a CSV file.
bool LayerSettings::Load(string filename)
{
    Clear();
    
    ifstream layerParamsFile(filename.c_str());
    if(!layerParamsFile.is_open()) 
        return false; 
    
    string line;  
    string cell;
    char delim = ','; 
    
    // read the row of headers into a map that tells us which setting 
    // is overridden by each column
    if(std::getline(layerParamsFile, line))
    {  
        stringstream firstLineStream(line);
        
        // skip the first (Layer) column heading        
        int col = -1;

        while(std::getline(firstLineStream, cell, delim))
        {
            string name = Trim(cell);
        
            if(_columns.count(name) < 1)
                _columns[name] = col++;
            else
            {
                LOGGER.HandleError(DuplicateLayerParamsColumn, false, 
                                                                name.c_str());
                Clear();
                return false;
            }
        }
    }
    else
        return false;
    
    // for each row of settings, for a particular layer
    while(std::getline(layerParamsFile, line))
    {
        int layer;
        double value;
        vector<double> rowData;
        int col = 0;
        stringstream lineStream(line);
        
        // get the layer number
        if(std::getline(lineStream, cell, delim) && (Trim(cell).size() > 0))
        {
            layer = atoi(cell.c_str());
            if(layer < 1)
                continue;   // comment or other invalid row
        }
        else
            continue;
        
        // get the settings, using -1 for any missing ones 
        while(std::getline(lineStream, cell, delim))
            rowData.push_back((Trim(cell).size() > 0)  ? atof(cell.c_str()) : 
                                                         -1.0);

        // check for duplicate layer number
        if(_rows.count(layer) < 1)
            _rows[layer] = rowData;
        else
        {
            LOGGER.HandleError(DuplicateLayerParams, false, NULL, layer);
            return false;
        }
    }
    
    return _rows.size() > 0;

}

/// Clear all per-layer settings.
void LayerSettings::Clear()
{
    for (map<int, vector<double> >::iterator it = _rows.begin(); 
                                              it != _rows.end(); ++it)
        it->second.clear();
    
    _rows.clear();
    _columns.clear();
}

/// Trim leading and trailing whitespace from a string.
string LayerSettings::Trim(string input)
{
    const char* whitespace = " \n\r\t";
    
    size_t start = input.find_first_not_of(whitespace);
    size_t end   = input.find_last_not_of (whitespace);

    if((string::npos == start) || ( string::npos == end))
        return "";
    else
        return input.substr(start, end - start + 1);
}

/// Get the raw double value contained in this object for the given layer and 
/// setting name, if any.  Return -1.0 if no such value is contained.
double LayerSettings::GetRawValue(int layer, std::string name)
{
    double value = -1.0;
       
    if(_rows.count(layer) > 0 && _columns.count(name) > 0)
    {
        int col = _columns[name];
        value = _rows[layer][col];
    }
    
    return value;  
}

/// Get the override for an integer setting, if overridden, else the setting 
/// value itself.
int LayerSettings::GetInt(int layer, std::string name)
{
    double value = GetRawValue(layer, name);
    if(value > 0.0)
        return (int) value;
    else
        return SETTINGS.GetInt(name);  
}

/// Get the override for a double setting, if overridden, else the setting 
/// value itself.
double LayerSettings::GetDouble(int layer, std::string name)
{
    double value = GetRawValue(layer, name);
    if(value > 0.0)
        return value;
    else
        return SETTINGS.GetDouble(name);  
}