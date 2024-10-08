#define PYCOREAUDIO_MODULE
#include <Python.h>
#include <CoreAudio/CoreAudio.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <numeric>
#include <string>
#include <iostream>

#ifndef __cplusplus
    #error "C++ required"
#endif

#define PyBool_FromBool(b) PyBool_FromLong((b) ? 1 : 0)
extern const char* MOD_DOCSTR;

namespace properties {
    //Volume control
    const AudioObjectPropertyAddress volume = {
        kAudioDevicePropertyVolumeScalar, //mSelector
        kAudioDevicePropertyScopeOutput, //mScope
        0 //mElement
    };
    //Mute control
    const AudioObjectPropertyAddress mute = { 
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        0
    };
    //Default output device
    const AudioObjectPropertyAddress defaultOutputDevice = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain //kAudioObjectPropertyElementMaster - deprecated since Monterey
    };
    //Devices count
    const AudioObjectPropertyAddress count = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    }; 
    //Device name
    const AudioObjectPropertyAddress name = {
        kAudioDevicePropertyDeviceNameCFString,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    
    //Device manufacturer
    const AudioObjectPropertyAddress manufacturer = {
        kAudioDevicePropertyDeviceManufacturerCFString,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    //Device input streams
    const AudioObjectPropertyAddress instreams = {
        kAudioDevicePropertyStreams,
        kAudioDevicePropertyScopeInput,
        0
    };
    //Device output streams
    const AudioObjectPropertyAddress outstreams = {
        kAudioDevicePropertyStreams,
        kAudioDevicePropertyScopeOutput,
        0
    };
    //Device UID
    const AudioObjectPropertyAddress uid = {
        kAudioDevicePropertyDeviceUID,
        kAudioDevicePropertyScopeOutput,
        0
    };
};

/* -----------------------------Globals----------------------------------- */
AudioDeviceID defaultOutputDeviceID = 0;                //ID of the default output device
std::vector<int> validChannelsForDefaultDevice;         //List of valid channels for the default output device
bool initialized = false;                               //Just to know wheather we got the default device ID
/* ----------------------------------------------------------------------- */


/* ---------------------------C++ Interface------------------------------- */

/**
 * Get a list of valid channels for the default output device.
 * If deviceID is NULL, it will be set to the default output device.
 *
 * @param deviceID - output device
 * @param maxFailures - number of errors after which the scan is stopped
 * @result - list (vector) of valid channels
 */
std::vector<int> getValidChannels(AudioDeviceID *deviceID = NULL, int maxFailures = 3){
    std::vector<int> validChannels;
    if(deviceID == NULL) deviceID = &defaultOutputDeviceID;

    //During the check we'll be trying to see if the channel has a
    //volume level property
    AudioObjectPropertyAddress propertyAddress = properties::volume;

    int channel = 0, errors = 0;
    while(errors < maxFailures){
        //Cycle trough channels until the last [maxFailures] channels
        //are invalid
        propertyAddress.mElement = channel;
        if(AudioObjectHasProperty(*deviceID, &propertyAddress)){
            //Channel is valid, add it to the list
            validChannels.push_back(channel);
        } else errors++;
        channel++;
    }
    return validChannels;
}

bool init(){
    UInt32 dataSize = sizeof(AudioDeviceID);
    //Find the default output device
    OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                 &properties::defaultOutputDevice,
                                                 0, NULL,
                                                 &dataSize, &defaultOutputDeviceID);
    //if(result != kAudioHardwareNoError) return false;
    if(result != kAudioHardwareNoError) {
    printf("Error getting default output device: %d\n", result);
    return false;
}
    //Get a list of valid channels
    validChannelsForDefaultDevice = getValidChannels(&defaultOutputDeviceID);
    //if(validChannelsForDefaultDevice.size() == 0) return false;   防止设备是多输出设备时出错
    initialized = true;
    return initialized;
}

/**
 * Deinitialize the library.
 */
void deinit(){
    defaultOutputDeviceID = 0;
    validChannelsForDefaultDevice.clear();
    initialized = false;
}

/**
 * Set a property of the default output device.
 * This function is universal, thus it can be used to
 * set volume level or set the mute state.
 * 
 * This is an internal function, which will not be
 * accessible from Python.
 *
 * @param data - buffer to read from (value to set)
 * @param propertyAddr - address of the property
 * @param channels - list of valid channels
 * @result - wheather the set failed or succeeded
 */
template <typename UniversalDataType>
bool setProperty(UniversalDataType data, AudioObjectPropertyAddress propertyAddr, std::vector<int> channels = validChannelsForDefaultDevice){
    UInt32 dataSize = sizeof(data);

    std::vector<bool> statuses;
    OSStatus result;
    for(std::vector<int>::size_type i = 0; i < channels.size(); i++) {
        propertyAddr.mElement = channels[i];
        result = AudioObjectSetPropertyData(defaultOutputDeviceID,
                                            &propertyAddr,
                                            0, NULL, dataSize, &data);
        statuses.push_back(result == kAudioHardwareNoError);
    }
    bool error = !std::all_of(statuses.begin(), statuses.end(), [](bool v) { return v; });

    return !error;
}

/**
 * Get the value of a property of the default output device.
 * This function is universal, thus it can be used to
 * get the volume level or get the mute state.
 * 
 * This is an internal function, which will not be
 * accessible from Python.
 *
 * @param buffer - buffer to write to (should be a vector)
 * @param propertyAddr - address of the property
 * @param channels - list of valid channels
 * @result - wheather the set failed or succeeded
 */
template <typename UniversalDataType>
bool getProperty(std::vector<UniversalDataType> &buffer, AudioObjectPropertyAddress propertyAddr, std::vector<int> channels = validChannelsForDefaultDevice){
    UniversalDataType data;
    UInt32 dataSize = sizeof(data);

    std::vector<bool> statuses;
    std::vector<UniversalDataType> dataCollection;
    OSStatus result;
    for(std::vector<int>::size_type i = 0; i < channels.size(); i++) {
        propertyAddr.mElement = channels[i];
        result = AudioObjectGetPropertyData(defaultOutputDeviceID,
                                            &propertyAddr,
                                            0, NULL, &dataSize, &data);
        statuses.push_back(result == kAudioHardwareNoError);
        if(result == kAudioHardwareNoError) {
            dataCollection.push_back(data);
        }
    }
    bool error = !std::all_of(statuses.begin(), statuses.end(), [](bool v) { return v; });
    if(error) return false;

    for(std::vector<int>::size_type i = 0; i < dataCollection.size(); i++){
        buffer.push_back(dataCollection[i]);
    }

    return true;
}

/* ----------------------------------------------------------------------- */


/* ---------------------Python Interface Helpers-------------------------- */

/**
 * Convert an std::vector<int> to a Python tuple.
 *
 * @param data - source vector
 * @result - Python tuple
 */
PyObject* intVectorToTuple(const std::vector<int> &data){
    size_t dataSize = data.size();
    PyObject* tuple = PyTuple_New(dataSize);
    
    for(std::vector<int>::size_type i = 0; i < dataSize; i++){
        PyObject *num = PyLong_FromLong( (long)data[i] );
        PyTuple_SET_ITEM(tuple, i, num);
    }
    return tuple;
}

PyObject* cppstring_to_pystr(std::string str){
    return Py_BuildValue("s", str.c_str());
}

PyObject* cstring_to_pystr(const char* str){
    return Py_BuildValue("s", str);
}

/**
 * Set the mute state of the default output device.
 * 
 * @param state - muted/unmuted (1/0)
 * @result - wheather the set failed or succeeded
 */
bool setMute(bool state){
    //Sometimes we have to use channel 0, idk why                                                              v
    return setProperty((UInt32)state, properties::mute) ? true : setProperty((UInt32)state, properties::mute, {0});
}

/**
 * Get the mute state of the default output device.
 * If the output device has multiple channels and they
 * are not all muted/unmuted then the mute state of the
 * last channel is returned.
 *
 * @result - mute state (0/1)
 */
bool getMute(){
    //Warning: Do NOT use bool vector, must be int!
    std::vector<int> muteStates;
    bool error = getProperty(muteStates, properties::mute) ? false : !getProperty(muteStates, properties::mute, {0});
    if(error) return -1;

    bool finalState = true;
    for(int muteState_as_int : muteStates){
        bool muteState = (bool)muteState_as_int;
        finalState = finalState && muteState;
    }

    return finalState;
}

/**
 * Get the volume level of the default output device.
 * If the output device has multiple channels and they
 * are set to a different volume level then the
 * averrage is returned.
 *
 * @result - volume level (0-100%) as int
 */
int getVolume(){
    std::vector<Float32> volumes;
    bool error = !(getProperty(volumes, properties::volume));
    if(error) return -1.0;

    Float32 volumeAvrg = std::accumulate(volumes.begin(), volumes.end(), 0.0f) / volumes.size();
    int x = int(roundf(volumeAvrg * 100.0f));
    return x;
}

/**
 * Set the volume level of the default output device.
 *
 * @param volume_in_percent - volume level (0-100)
 * @result - wheather the set failed or succeeded
 */
bool setVolume(int volume_in_percent){
    Float32 volume = Float32(volume_in_percent) / 100;
    return setProperty(volume, properties::volume);
}

/**
 * Set the volume level of a specified output device.
 *
 * @param deviceID - ID of the output device
 * @param volume_in_percent - volume level (0-100)
 * @result - whether the set failed or succeeded
 */
bool setVolumeForDevice(AudioDeviceID deviceID, int volume_in_percent) {
    // 使用现有的 setProperty 函数来设置音量
    Float32 volume = Float32(volume_in_percent) / 100;
    AudioObjectPropertyAddress propertyAddr = properties::volume;
    propertyAddr.mElement = 0; // 通常设置为通道 0

    OSStatus result = AudioObjectSetPropertyData(deviceID, &propertyAddr, 0, NULL, sizeof(volume), &volume);
    return (result == kAudioHardwareNoError);
}

/**
 * Get the volume level of a specified output device.
 *
 * @param deviceID - ID of the output device
 * @result - volume level (0-100) or -1 on error
 */
int getVolumeForDevice(AudioDeviceID deviceID) {
    Float32 volume;
    UInt32 dataSize = sizeof(volume);
    AudioObjectPropertyAddress propertyAddr = properties::volume;
    propertyAddr.mElement = 0; // 通常设置为通道 0

    OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddr, 0, NULL, &dataSize, &volume);
    if (result != kAudioHardwareNoError) {
        return -1; // 失败
    }

    // 将音量转换为百分比
    return static_cast<int>(roundf(volume * 100.0f));
}

/**
 * Set the mute status of a specified output device.
 *
 * @param deviceID - ID of the output device
 * @param mute - true to mute, false to unmute
 * @result - whether the set failed or succeeded
 */
bool setMuteForDevice(AudioDeviceID deviceID, bool mute) {
    UInt32 muteValue = mute ? 1 : 0; // 将布尔值转换为整数
    AudioObjectPropertyAddress propertyAddr = properties::mute;
    propertyAddr.mElement = 0; // 通常设置为通道 0

    OSStatus result = AudioObjectSetPropertyData(deviceID, &propertyAddr, 0, NULL, sizeof(muteValue), &muteValue);
    return (result == kAudioHardwareNoError);
}

/**
 * Get the mute status of a specified output device.
 *
 * @param deviceID - ID of the output device
 * @result - true if muted, false if not muted, or -1 on error
 */
int getMuteForDevice(AudioDeviceID deviceID) {
    UInt32 muteValue;
    UInt32 dataSize = sizeof(muteValue);
    AudioObjectPropertyAddress propertyAddr = properties::mute;
    propertyAddr.mElement = 0; // 通常设置为通道 0

    OSStatus result = AudioObjectGetPropertyData(deviceID, &propertyAddr, 0, NULL, &dataSize, &muteValue);
    if (result != kAudioHardwareNoError) {
        return -1; // 失败
    }

    return (muteValue == 1) ? 1 : 0; // 返回静音状态
}

int getDeviceCount(){
    UInt32 propSize;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &properties::count, 0, NULL, &propSize);
    int deviceCount = propSize / sizeof(AudioDeviceID);
    return deviceCount;
}

char* CFStringToCString(CFStringRef raw){
    if(raw == NULL){
        return NULL;
    }
    CFIndex length = CFStringGetLength(raw);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    char* buffer = (char*)malloc(maxSize);
    if(CFStringGetCString(raw, buffer, maxSize,
                         kCFStringEncodingUTF8)){
        return buffer;
    }
    free(buffer);
    return NULL;
}

std::string getDeviceName(AudioDeviceID device){
    UInt32 propSize = sizeof(CFStringRef);
    CFStringRef result;
    OSStatus error = AudioObjectGetPropertyData(device, &properties::name, 0, NULL, &propSize, &result);
    if(error != noErr){
        return (std::string)"Unknown";
    }
    char* str = CFStringToCString(result);
    if(str == NULL){
        free(str);
        return (std::string)"Unknown";
    }
    std::string name(str);
    free(str);
    return name;
}

std::string getDeviceManufacturer(AudioDeviceID device){
    UInt32 propSize = sizeof(CFStringRef);
    CFStringRef result;
    OSStatus error = AudioObjectGetPropertyData(device, &properties::manufacturer, 0, NULL, &propSize, &result);
    if(error != noErr){
        return (std::string)"Unknown";
    }
    char* str = CFStringToCString(result);
    if(str == NULL){
        free(str);
        return (std::string)"Unknown";
    }
    std::string name(str);
    free(str);
    return name;
}

std::string getDeviceUID(AudioDeviceID device){
    UInt32 propSize = sizeof(CFStringRef);
    CFStringRef result;
    OSStatus error = AudioObjectGetPropertyData(device, &properties::uid, 0, NULL, &propSize, &result);
    if(error != noErr){
        return (std::string)"Unknown";
    }
    char* str = CFStringToCString(result);
    if(str == NULL){
        free(str);
        return (std::string)"Unknown";
    }
    std::string name(str);
    free(str);
    return name;
}

int getDeviceStreamCount(AudioDeviceID device, AudioObjectPropertyAddress io_direction){
    UInt32 dataSize = 0;
    OSStatus error = AudioObjectGetPropertyDataSize(device, &io_direction, 0, NULL, &dataSize);
    if(error != noErr){
        return -1;
    }
    UInt32 streamCount = dataSize / sizeof(AudioStreamID);
    return streamCount;
}

/* ----------------------------------------------------------------------- */


/* ------------------------Python Interface------------------------------- */
static PyObject* PyCoreAudio_init(PyObject* self, PyObject* _){
    if(initialized){
        PyErr_SetString(PyExc_Exception, "Already initialized");
        PyErr_Occurred();
        return NULL;
    }
    return PyBool_FromBool(init());
}

static PyObject* PyCoreAudio_ready(PyObject* self, PyObject* _){
    return PyBool_FromBool(initialized);
}

static PyObject* PyCoreAudio_deinit(PyObject* self, PyObject* _){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    deinit();
    Py_RETURN_NONE;
}

static PyObject* PyCoreAudio_getValidChannels(PyObject* self, PyObject* _){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    std::vector<int> channels = getValidChannels();
    return intVectorToTuple(channels);
}

static PyObject* PyCoreAudio_getDeviceCount(PyObject* self, PyObject* _){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    return PyLong_FromLong((long)getDeviceCount());
}

static PyObject* PyCoreAudio_getDevices(PyObject* self, PyObject* _){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    PyObject* res = NULL;
    OSStatus error;
    const int numDevices = getDeviceCount();
    UInt32 propSize = numDevices * sizeof(AudioDeviceID);
    AudioDeviceID* audioDevices = (AudioDeviceID *)malloc(propSize);

    //Grab the devices and store them in audioDevices
    error = AudioObjectGetPropertyData(kAudioObjectSystemObject, &properties::count, 0, NULL, &propSize, audioDevices);
    if(error != noErr){
        PyErr_SetString(PyExc_Exception, "Error getting devices from system");
        PyErr_Occurred();
        return NULL;
    }

    res = PyTuple_New(numDevices);
    propSize = sizeof(CFStringRef);
    for(int i = 0; i < numDevices; i++){
        //printf("%u\n",audioDevices[i]);
        std::string name = getDeviceName(audioDevices[i]);
        std::string manufacturer = getDeviceManufacturer(audioDevices[i]);
        std::string uid = getDeviceUID(audioDevices[i]);
        int in_streams = getDeviceStreamCount(audioDevices[i], properties::instreams);
        int out_streams = getDeviceStreamCount(audioDevices[i], properties::outstreams);
        bool isMic = in_streams > 0;
        bool isSpeaker = out_streams > 0;
        int devices_id = audioDevices[i];


        PyTuple_SetItem(res, i, Py_BuildValue("(s, s, s, i, i, N, N, i)",
            name.c_str(),
            manufacturer.c_str(),
            uid.c_str(),
            in_streams,
            out_streams,
            PyBool_FromBool(isMic),
            PyBool_FromBool(isSpeaker),
            devices_id
            ));
    }
    free(audioDevices);
    return res;
}

static PyObject* PyCoreAudio_getCurrentDevice(PyObject* self, PyObject* _){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    return cppstring_to_pystr(getDeviceName(defaultOutputDeviceID));
}

static PyObject* PyCoreAudio_setMute(PyObject* self, PyObject* arg){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    return PyBool_FromBool(setMute(PyObject_IsTrue(arg)));
}

static PyObject* PyCoreAudio_getMute(PyObject* self, PyObject* _){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    return PyBool_FromBool(getMute());
}

static PyObject* PyCoreAudio_mute(PyObject* self, PyObject* _){
    return PyCoreAudio_setMute(self, Py_True);
}

static PyObject* PyCoreAudio_unmute(PyObject* self, PyObject* _){
    return PyCoreAudio_setMute(self, Py_False);
}

static PyObject* PyCoreAudio_getVolume(PyObject* self, PyObject* _){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    return PyLong_FromLong((long)getVolume());
}

static PyObject* PyCoreAudio_setVolume(PyObject* self, PyObject* arg){
    if(!initialized){
        PyErr_SetString(PyExc_Exception, "Not initialized");
        PyErr_Occurred();
        return NULL;
    }
    int value = PyLong_AsLong(arg);
    if(value >= 0 && value <= 100){
        return PyBool_FromBool(setVolume(value));
    }
    PyErr_SetString(PyExc_Exception, "Value out of range [0;100]");
    PyErr_Occurred();
    return NULL;
}

static PyObject* PyCoreAudio_setVolumeForDevice(PyObject* self, PyObject* args) {
    AudioDeviceID deviceID;
    int volume_in_percent;

    if (!PyArg_ParseTuple(args, "Ii", &deviceID, &volume_in_percent)) {
        return NULL; // 参数解析失败
    }

    if (volume_in_percent < 0 || volume_in_percent > 100) {
        PyErr_SetString(PyExc_Exception, "Value out of range [0;100]");
        return NULL;
    }

    // 调用新创建的 setVolumeForDevice 函数
    return PyBool_FromBool(setVolumeForDevice(deviceID, volume_in_percent));
}

static PyObject* PyCoreAudio_getVolumeForDevice(PyObject* self, PyObject* args) {
    AudioDeviceID deviceID;

    if (!PyArg_ParseTuple(args, "i", &deviceID)) {
        return NULL; // 参数解析失败
    }

    // 调用新创建的 getVolumeForDevice 函数
    int volume = getVolumeForDevice(deviceID);
    if (volume == -1) {
        PyErr_SetString(PyExc_Exception, "Failed to get volume for device");
        return NULL;
    }

    return PyLong_FromLong(volume);
}

static PyObject* PyCoreAudio_setMuteForDevice(PyObject* self, PyObject* args) {
    AudioDeviceID deviceID;
    int mute;

    if (!PyArg_ParseTuple(args, "iI", &deviceID, &mute)) {
        return NULL; // 参数解析失败
    }

    // 调用 setMuteForDevice 函数
    bool success = setMuteForDevice(deviceID, mute != 0);
    return PyBool_FromLong(success);
}

static PyObject* PyCoreAudio_getMuteForDevice(PyObject* self, PyObject* args) {
    AudioDeviceID deviceID;

    if (!PyArg_ParseTuple(args, "i", &deviceID)) {
        return NULL; // 参数解析失败
    }

    // 调用 getMuteForDevice 函数
    int muteStatus = getMuteForDevice(deviceID);
    if (muteStatus == -1) {
        PyErr_SetString(PyExc_Exception, "Failed to get mute status for device");
        return NULL;
    }

    return PyLong_FromLong(muteStatus);
}
static PyMethodDef PyCoreAudioMethods[] = {
    {"init",  PyCoreAudio_init, METH_NOARGS,
        "Initialize the module. Finds and selects the currently selected audio output device.\n"
        "This does not open/lock the audio device, so it will not affect other applications\n"
        "using the same device. You must run this function before using any other one."},
    
    {"ready",  PyCoreAudio_ready, METH_NOARGS,
        "Check if the module is initialized. Returns a boolean."},
    
    {"deinit",  PyCoreAudio_deinit, METH_NOARGS,
        "Deinitialize the module. It is possible to run init() again after this function.\n"
        "Reinitialization should be done if the currently selected audio output device has\n"
        "been changed. This change cannot be detected automatically using this module.\n"
        "If the module is not initialized, an exception will be raised."},
    
    {"getValidChannels", PyCoreAudio_getValidChannels, METH_NOARGS,
        "Get a list of valid channels. Returns a tuple.\n"
        "If the module is not initialized, an exception will be raised."},
    
    {"getMute", PyCoreAudio_getMute, METH_NOARGS,
        "Get mute status of the current audio output device. Returns a boolean.\n"
        "If the module is not initialized, an exception will be raised."},
    
    {"setMute", PyCoreAudio_setMute, METH_O,
        "Set mute status of the current audio output device. Returns a boolean, which represents\n"
        "whether the operation was successful or not.\n"
        "If the module is not initialized, an exception will be raised."},
    
    {"mute", PyCoreAudio_mute, METH_NOARGS,
        "Mute the current audio output device. Alias to setMute(True)."},
    
    {"unmute", PyCoreAudio_unmute, METH_NOARGS,
        "Unmute the current audio output device. Alias to setMute(False)."},
    
    {"getVolume", PyCoreAudio_getVolume, METH_NOARGS,
        "Get the currently set volume level of the current audio output device.\n"
        "Returns an in in range [0; 100], indicating the volume level in percentage.\n"
        "If the module is not initialized, an exception will be raised."},
    
    {"setVolume", PyCoreAudio_setVolume, METH_O,
        "Set volume level of the current audio output device. Takes a single argument - \n"
        "int. The argument must be in interval [0; 100], indicating the volume level in percentage.\n"
        "Returns a boolean, which represents whether the operation was successful or not.\n"
        "If the module is not initialized or the argument is out of range, an exception\n"
        "will be raised."},
    
    {"getDeviceCount", PyCoreAudio_getDeviceCount, METH_NOARGS, 
        "Get the amount of audio input and output devices available on this system.\n"
        "If the module is not initialized, an exception will be raised."},
    
    {"getDevices", PyCoreAudio_getDevices, METH_NOARGS,
        "Get all audio input and output devices available on this system along with\n"
        "some of their properties. Returns a tuple of tuples.\n"
        "The tuples are structured like this:\n"
        "(name, manufacturer, uid, number of input streams, number of output streams,\n"
        "isMicrophone, isSpeaker).\n"
        "Keep in mind that a single device can have both input and output streams.\n"
        "In such a case, the last two properties will be True.\n"
        "If the module fails to get the name of the device, the value \"Unknown\"\n"
        "will be used.\n"
        "If the module is not initialized, an exception will be raised."},
    
    {"getCurrentDevice", PyCoreAudio_getCurrentDevice, METH_NOARGS,
        "Get the name of the current audio output device.\n"
        "If the module is not initialized, an exception will be raised."},
    {"setVolumeForDevice", PyCoreAudio_setVolumeForDevice, METH_VARARGS, "Set volume level of a specified output device."},
    {"getVolumeForDevice", PyCoreAudio_getVolumeForDevice, METH_VARARGS, "Get volume level of a specified output device."},
    {"setMuteForDevice", PyCoreAudio_setMuteForDevice, METH_VARARGS, "Set mute status of a specified output device."},
    {"getMuteForDevice", PyCoreAudio_getMuteForDevice, METH_VARARGS, "Get mute status of a specified output device."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef modpycoreaudio = {
    PyModuleDef_HEAD_INIT,
    "CoreAudio",   /* name of module */
    MOD_DOCSTR,          /* module documentation, may be NULL */
    -1,            /* size of per-interpreter state of the module,
                      or -1 if the module keeps state in global variables. */
    PyCoreAudioMethods
};

/* ----------------------------------------------------------------------- */


PyMODINIT_FUNC PyInit_CoreAudio(){
    return PyModule_Create(&modpycoreaudio);
}

const char* MOD_DOCSTR = \
"This module allows you to make use of CoreAudio's basic functionality.\n"
"This includes getting and setting the mute status as well as the volume\n"
"of an audio output devices. The module can only work with one device,\n"
"which is selected when running init(). To change this device, you must\n"
"change the currently selected default audio output device.\n"
"Make sure to run init() before using any other functions.\n"
"It is also possible to retrieve basic information about all the audio I/O\n"
"devices available on the system.\n\n"
"Module written by br0kenpixel.";