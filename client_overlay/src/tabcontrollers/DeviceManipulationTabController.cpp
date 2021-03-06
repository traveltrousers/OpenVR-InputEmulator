#include "DeviceManipulationTabController.h"
#include <QQuickWindow>
#include <QApplication>
#include "../overlaycontroller.h"
#include <openvr_math.h>
#include <chrono>

// application namespace
namespace inputemulator {


DeviceManipulationTabController::~DeviceManipulationTabController() {
	if (identifyThread.joinable()) {
		identifyThread.join();
	}
}


void DeviceManipulationTabController::initStage1() {
	reloadDeviceManipulationProfiles();
	reloadDeviceManipulationSettings();
}


void DeviceManipulationTabController::initStage2(OverlayController * parent, QQuickWindow * widget) {
	this->parent = parent;
	this->widget = widget;
	try {
		vrInputEmulator.connect();
		for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id) {
			auto deviceClass = vr::VRSystem()->GetTrackedDeviceClass(id);
			if (deviceClass != vr::TrackedDeviceClass_Invalid) {
				if (deviceClass == vr::TrackedDeviceClass_Controller || deviceClass == vr::TrackedDeviceClass_GenericTracker) {
					auto info = std::make_shared<DeviceInfo>();
					info->openvrId = id;
					info->deviceClass = deviceClass;
					char buffer[vr::k_unMaxPropertyStringSize];
					vr::ETrackedPropertyError pError = vr::TrackedProp_Success;
					vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_SerialNumber_String, buffer, vr::k_unMaxPropertyStringSize, &pError);
					if (pError == vr::TrackedProp_Success) {
						info->serial = std::string(buffer);
					} else {
						info->serial = std::string("<unknown serial>");
						LOG(ERROR) << "Could not get serial of device " << id;
					}

					try {
						vrinputemulator::DeviceInfo info2;
						vrInputEmulator.getDeviceInfo(info->openvrId, info2);
						info->deviceMode = info2.deviceMode;
						info->deviceOffsetsEnabled = info2.offsetsEnabled;
						if (info->deviceMode == 2 || info->deviceMode == 3) {
							info->deviceStatus = info2.redirectSuspended ? 1 : 0;
						}
					} catch (std::exception& e) {
						LOG(ERROR) << "Exception caught while getting device info: " << e.what();
					}

					deviceInfos.push_back(info);
					LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
				}
				maxValidDeviceId = id;
			}
		}
		emit deviceCountChanged((unsigned)deviceInfos.size());
	} catch (const std::exception& e) {
		LOG(ERROR) << "Could not connect to driver component: " << e.what();
	}
}


void DeviceManipulationTabController::eventLoopTick(vr::TrackedDevicePose_t* devicePoses) {
	if (settingsUpdateCounter >= 50) {
		settingsUpdateCounter = 0;
		if (parent->isDashboardVisible() || parent->isDesktopMode()) {
			unsigned i = 0;
			for (auto info : deviceInfos) {
				bool hasDeviceInfoChanged = updateDeviceInfo(i);
				unsigned status = devicePoses[info->openvrId].bDeviceIsConnected ? 0 : 1;
				if (info->deviceMode == 0 && info->deviceStatus != status) {
					info->deviceStatus = status;
					hasDeviceInfoChanged = true;
				}
				if (hasDeviceInfoChanged) {
					emit deviceInfoChanged(i);
				}
				++i;
			}
			bool newDeviceAdded = false;
			for (uint32_t id = maxValidDeviceId + 1; id < vr::k_unMaxTrackedDeviceCount; ++id) {
				auto deviceClass = vr::VRSystem()->GetTrackedDeviceClass(id);
				if (deviceClass != vr::TrackedDeviceClass_Invalid) {
					if (deviceClass == vr::TrackedDeviceClass_Controller || deviceClass == vr::TrackedDeviceClass_GenericTracker) {
						auto info = std::make_shared<DeviceInfo>();
						info->openvrId = id;
						info->deviceClass = deviceClass;
						char buffer[vr::k_unMaxPropertyStringSize];
						vr::ETrackedPropertyError pError = vr::TrackedProp_Success;
						vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_SerialNumber_String, buffer, vr::k_unMaxPropertyStringSize, &pError);
						if (pError == vr::TrackedProp_Success) {
							info->serial = std::string(buffer);
						} else {
							info->serial = std::string("<unknown serial>");
							LOG(ERROR) << "Could not get serial of device " << id;
						}

						try {
							vrinputemulator::DeviceInfo info2;
							vrInputEmulator.getDeviceInfo(info->openvrId, info2);
							info->deviceMode = info2.deviceMode;
							info->deviceOffsetsEnabled = info2.offsetsEnabled;
							if (info->deviceMode == 2 || info->deviceMode == 3) {
								info->deviceStatus = info2.redirectSuspended ? 1 : 0;
							}
						} catch (std::exception& e) {
							LOG(ERROR) << "Exception caught while getting device info: " << e.what();
						}

						deviceInfos.push_back(info);
						LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
						newDeviceAdded = true;
					}
					maxValidDeviceId = id;
				}
			}
			if (newDeviceAdded) {
				emit deviceCountChanged((unsigned)deviceInfos.size());
			}
		}
	} else {
		settingsUpdateCounter++;
	}
}

void DeviceManipulationTabController::handleEvent(const vr::VREvent_t&) {
	/*switch (vrEvent.eventType) {
		default:
			break;
	}*/
}

unsigned  DeviceManipulationTabController::getDeviceCount() {
	return (unsigned)deviceInfos.size();
}

QString DeviceManipulationTabController::getDeviceSerial(unsigned index) {
	if (index < deviceInfos.size()) {
		return QString::fromStdString(deviceInfos[index]->serial);
	} else {
		return QString("<ERROR>");
	}
}

unsigned DeviceManipulationTabController::getDeviceId(unsigned index) {
	if (index < deviceInfos.size()) {
		return (int)deviceInfos[index]->openvrId;
	} else {
		return vr::k_unTrackedDeviceIndexInvalid;
	}
}

int DeviceManipulationTabController::getDeviceClass(unsigned index) {
	if (index < deviceInfos.size()) {
		return (int)deviceInfos[index]->deviceClass;
	} else {
		return -1;
	}
}

int DeviceManipulationTabController::getDeviceState(unsigned index) {
	if (index < deviceInfos.size()) {
		return (int)deviceInfos[index]->deviceStatus;
	} else {
		return -1;
	}
}

int DeviceManipulationTabController::getDeviceMode(unsigned index) {
	if (index < deviceInfos.size()) {
		return (int)deviceInfos[index]->deviceMode;
	} else {
		return -1;
	}
}

bool DeviceManipulationTabController::deviceOffsetsEnabled(unsigned index) {
	if (index < deviceInfos.size()) {
		return deviceInfos[index]->deviceOffsetsEnabled;
	} else {
		return false;
	}
}

double DeviceManipulationTabController::getWorldFromDriverRotationOffset(unsigned index, unsigned axis) {
	if (index < deviceInfos.size() && axis < 3) {
		return deviceInfos[index]->worldFromDriverRotationOffset.v[axis];
	} else {
		return 0.0;
	}
}

double DeviceManipulationTabController::getWorldFromDriverTranslationOffset(unsigned index, unsigned axis) {
	if (index < deviceInfos.size() && axis < 3) {
		return deviceInfos[index]->worldFromDriverTranslationOffset.v[axis];
	} else {
		return 0.0;
	}
}

double DeviceManipulationTabController::getDriverFromHeadRotationOffset(unsigned index, unsigned axis) {
	if (index < deviceInfos.size() && axis < 3) {
		return deviceInfos[index]->driverFromHeadRotationOffset.v[axis];
	} else {
		return 0.0;
	}
}

double DeviceManipulationTabController::getDriverFromHeadTranslationOffset(unsigned index, unsigned axis) {
	if (index < deviceInfos.size() && axis < 3) {
		return deviceInfos[index]->driverFromHeadTranslationOffset.v[axis];
	} else {
		return 0.0;
	}
}

double DeviceManipulationTabController::getDriverRotationOffset(unsigned index, unsigned axis) {
	if (index < deviceInfos.size() && axis < 3) {
		return deviceInfos[index]->deviceRotationOffset.v[axis];
	} else {
		return 0.0;
	}
}

double DeviceManipulationTabController::getDriverTranslationOffset(unsigned index, unsigned axis) {
	if (index < deviceInfos.size() && axis < 3) {
		return deviceInfos[index]->deviceTranslationOffset.v[axis];
	} else {
		return 0.0;
	}
}

int DeviceManipulationTabController::getMotionCompensationCenterMode() {
	return motionCompensationCenterMode;
}

double DeviceManipulationTabController::getMotionCompensationCenter(int axis) {
	if (axis >= 0 && axis < 3) {
		return motionCompensationCenter.v[axis];
	} else {
		return 0.0;
	}
}

double DeviceManipulationTabController::getMotionCompensationTmpCenter(int axis) {
	if (axis >= 0 && axis < 3) {
		return motionCompensationTmpCenter.v[axis];
	} else {
		return 0.0;
	}
}


#define DEVICEMANIPULATIONSETTINGS_GETTRANSLATIONVECTOR(name) { \
	double valueX = settings->value(#name ## "_x", 0.0).toDouble(); \
	double valueY = settings->value(#name ## "_y", 0.0).toDouble(); \
	double valueZ = settings->value(#name ## "_z", 0.0).toDouble(); \
	entry.name = { valueX, valueY, valueZ }; \
}

#define DEVICEMANIPULATIONSETTINGS_GETROTATIONVECTOR(name) { \
	double valueY = settings->value(#name ## "_yaw", 0.0).toDouble(); \
	double valueP = settings->value(#name ## "_pitch", 0.0).toDouble(); \
	double valueR = settings->value(#name ## "_roll", 0.0).toDouble(); \
	entry.name = { valueY, valueP, valueR }; \
}

void DeviceManipulationTabController::reloadDeviceManipulationSettings() {
	auto settings = OverlayController::appSettings();
	settings->beginGroup("deviceManipulationSettings");
	motionCompensationCenterMode = settings->value("motionCompensationCenterMode", 0).toInt();
	double valueX = settings->value("motionCompensationCenter_x", 0.0).toDouble();
	double valueY = settings->value("motionCompensationCenter_y", 0.0).toDouble();
	double valueZ = settings->value("motionCompensationCenter_z", 0.0).toDouble();
	motionCompensationCenter = { valueX, valueY, valueZ };
	settings->endGroup();
}

void DeviceManipulationTabController::reloadDeviceManipulationProfiles() {
	deviceManipulationProfiles.clear();
	auto settings = OverlayController::appSettings();
	settings->beginGroup("deviceManipulationSettings");
	auto profileCount = settings->beginReadArray("deviceManipulationProfiles");
	for (int i = 0; i < profileCount; i++) {
		settings->setArrayIndex(i);
		deviceManipulationProfiles.emplace_back();
		auto& entry = deviceManipulationProfiles[i];
		entry.profileName = settings->value("profileName").toString().toStdString();
		entry.includesDeviceOffsets = settings->value("includesDeviceOffsets", false).toBool();
		if (entry.includesDeviceOffsets) {
			entry.deviceOffsetsEnabled = settings->value("deviceOffsetsEnabled", false).toBool();
			DEVICEMANIPULATIONSETTINGS_GETTRANSLATIONVECTOR(worldFromDriverTranslationOffset);
			DEVICEMANIPULATIONSETTINGS_GETROTATIONVECTOR(worldFromDriverRotationOffset);
			DEVICEMANIPULATIONSETTINGS_GETTRANSLATIONVECTOR(driverFromHeadTranslationOffset);
			DEVICEMANIPULATIONSETTINGS_GETROTATIONVECTOR(driverFromHeadRotationOffset);
			DEVICEMANIPULATIONSETTINGS_GETTRANSLATIONVECTOR(driverTranslationOffset);
			DEVICEMANIPULATIONSETTINGS_GETROTATIONVECTOR(driverRotationOffset);
		}
		entry.includesMotionCompensationSettings = settings->value("includesMotionCompensationSettings", false).toBool();
		if (entry.includesMotionCompensationSettings) {
			entry.motionCompensationCenterMode = settings->value("motionCompensationCenterMode", 0).toInt();
			DEVICEMANIPULATIONSETTINGS_GETTRANSLATIONVECTOR(motionCompensationCenter);
		}
	}
	settings->endArray();
	settings->endGroup();
}

void DeviceManipulationTabController::saveDeviceManipulationSettings() {
	auto settings = OverlayController::appSettings();
	settings->beginGroup("deviceManipulationSettings");
	settings->setValue("motionCompensationCenterMode", motionCompensationCenterMode);
	settings->setValue("motionCompensationCenter_x", motionCompensationCenter.v[0]);
	settings->setValue("motionCompensationCenter_y", motionCompensationCenter.v[1]);
	settings->setValue("motionCompensationCenter_z", motionCompensationCenter.v[2]);
	settings->endGroup();
	settings->sync();
}


#define DEVICEMANIPULATIONSETTINGS_WRITETRANSLATIONVECTOR(name) { \
	auto& vec = p.name; \
	settings->setValue(#name ## "_x", vec.v[0]); \
	settings->setValue(#name ## "_y", vec.v[1]); \
	settings->setValue(#name ## "_z", vec.v[2]); \
}


#define DEVICEMANIPULATIONSETTINGS_WRITEROTATIONVECTOR(name) { \
	auto& vec = p.name; \
	settings->setValue(#name ## "_yaw", vec.v[0]); \
	settings->setValue(#name ## "_pitch", vec.v[1]); \
	settings->setValue(#name ## "_roll", vec.v[2]); \
}


void DeviceManipulationTabController::saveDeviceManipulationProfiles() {
	auto settings = OverlayController::appSettings();
	settings->beginGroup("deviceManipulationSettings");
	settings->beginWriteArray("deviceManipulationProfiles");
	unsigned i = 0;
	for (auto& p : deviceManipulationProfiles) {
		settings->setArrayIndex(i);
		settings->setValue("profileName", QString::fromStdString(p.profileName));
		settings->setValue("includesDeviceOffsets", p.includesDeviceOffsets);
		if (p.includesDeviceOffsets) {
			settings->setValue("deviceOffsetsEnabled", p.deviceOffsetsEnabled);
			DEVICEMANIPULATIONSETTINGS_WRITETRANSLATIONVECTOR(worldFromDriverTranslationOffset);
			DEVICEMANIPULATIONSETTINGS_WRITEROTATIONVECTOR(worldFromDriverRotationOffset);
			DEVICEMANIPULATIONSETTINGS_WRITETRANSLATIONVECTOR(driverFromHeadTranslationOffset);
			DEVICEMANIPULATIONSETTINGS_WRITEROTATIONVECTOR(driverFromHeadRotationOffset);
			DEVICEMANIPULATIONSETTINGS_WRITETRANSLATIONVECTOR(driverTranslationOffset);
			DEVICEMANIPULATIONSETTINGS_WRITEROTATIONVECTOR(driverRotationOffset);
		}
		settings->setValue("includesMotionCompensationSettings", p.includesMotionCompensationSettings);
		if (p.includesMotionCompensationSettings) {
			settings->setValue("motionCompensationCenterMode", p.motionCompensationCenterMode);
			DEVICEMANIPULATIONSETTINGS_WRITETRANSLATIONVECTOR(motionCompensationCenter);
		}
		i++;
	}
	settings->endArray();
	settings->endGroup();
	settings->sync();
}

unsigned DeviceManipulationTabController::getDeviceManipulationProfileCount() {
	return (unsigned)deviceManipulationProfiles.size();
}

QString DeviceManipulationTabController::getDeviceManipulationProfileName(unsigned index) {
	if (index >= deviceManipulationProfiles.size()) {
		return QString();
	} else {
		return QString::fromStdString(deviceManipulationProfiles[index].profileName);
	}
}

void DeviceManipulationTabController::addDeviceManipulationProfile(QString name, unsigned deviceIndex, bool includesDeviceOffsets, bool includesMotionCompensationSettings) {
	if (deviceIndex >= deviceInfos.size()) {
		return;
	}
	auto device = deviceInfos[deviceIndex];
	DeviceManipulationProfile* profile = nullptr;
	for (auto& p : deviceManipulationProfiles) {
		if (p.profileName.compare(name.toStdString()) == 0) {
			profile = &p;
			break;
		}
	}
	if (!profile) {
		auto i = deviceManipulationProfiles.size();
		deviceManipulationProfiles.emplace_back();
		profile = &deviceManipulationProfiles[i];
	}
	profile->profileName = name.toStdString();
	profile->includesDeviceOffsets = includesDeviceOffsets;
	if (includesDeviceOffsets) {
		profile->deviceOffsetsEnabled = device->deviceOffsetsEnabled;
		profile->worldFromDriverTranslationOffset = device->worldFromDriverTranslationOffset;
		profile->worldFromDriverRotationOffset = device->worldFromDriverRotationOffset;
		profile->driverFromHeadTranslationOffset = device->driverFromHeadTranslationOffset;
		profile->driverFromHeadRotationOffset = device->driverFromHeadRotationOffset;
		profile->driverTranslationOffset = device->deviceTranslationOffset;
		profile->driverRotationOffset = device->deviceRotationOffset;
	}
	profile->includesMotionCompensationSettings = includesMotionCompensationSettings;
	if (includesMotionCompensationSettings) {
		profile->motionCompensationCenterMode = motionCompensationCenterMode;
		profile->motionCompensationCenter = motionCompensationCenter;
	}
	saveDeviceManipulationProfiles();
	OverlayController::appSettings()->sync();
	emit deviceManipulationProfilesChanged();
}

void DeviceManipulationTabController::applyDeviceManipulationProfile(unsigned index, unsigned deviceIndex) {
	if (index < deviceManipulationProfiles.size()) {
		if (deviceIndex >= deviceInfos.size()) {
			return;
		}
		auto device = deviceInfos[deviceIndex];
		auto& profile = deviceManipulationProfiles[index];
		if (profile.includesDeviceOffsets) {
			setWorldFromDriverRotationOffset(deviceIndex, profile.worldFromDriverRotationOffset.v[0], profile.worldFromDriverRotationOffset.v[1], profile.worldFromDriverRotationOffset.v[2], false);
			setWorldFromDriverTranslationOffset(deviceIndex, profile.worldFromDriverTranslationOffset.v[0], profile.worldFromDriverTranslationOffset.v[1], profile.worldFromDriverTranslationOffset.v[2], false);
			setDriverFromHeadRotationOffset(deviceIndex, profile.driverFromHeadRotationOffset.v[0], profile.driverFromHeadRotationOffset.v[1], profile.driverFromHeadRotationOffset.v[2], false);
			setDriverFromHeadTranslationOffset(deviceIndex, profile.driverFromHeadTranslationOffset.v[0], profile.driverFromHeadTranslationOffset.v[1], profile.driverFromHeadTranslationOffset.v[2], false);
			setDriverRotationOffset(deviceIndex, profile.driverRotationOffset.v[0], profile.driverRotationOffset.v[1], profile.driverRotationOffset.v[2], false);
			setDriverTranslationOffset(deviceIndex, profile.driverTranslationOffset.v[0], profile.driverTranslationOffset.v[1], profile.driverTranslationOffset.v[2], false);
			enableDeviceOffsets(deviceIndex, profile.deviceOffsetsEnabled, false);
			updateDeviceInfo(deviceIndex);
			emit deviceInfoChanged(deviceIndex);
		}
		if (profile.includesMotionCompensationSettings) {
			setMotionCompensationCenter(profile.motionCompensationCenterMode, profile.motionCompensationCenter.v[0], profile.motionCompensationCenter.v[1], profile.motionCompensationCenter.v[2]);
		}
	}
}

void DeviceManipulationTabController::deleteDeviceManipulationProfile(unsigned index) {
	if (index < deviceManipulationProfiles.size()) {
		auto pos = deviceManipulationProfiles.begin() + index;
		deviceManipulationProfiles.erase(pos);
		saveDeviceManipulationProfiles();
		OverlayController::appSettings()->sync();
		emit deviceManipulationProfilesChanged();
	}
}

unsigned DeviceManipulationTabController::getRenderModelCount() {
	return (unsigned)vr::VRRenderModels()->GetRenderModelCount();
}

QString DeviceManipulationTabController::getRenderModelName(unsigned index) {
	char buffer[vr::k_unMaxPropertyStringSize];
	vr::VRRenderModels()->GetRenderModelName(index, buffer, vr::k_unMaxPropertyStringSize);
	return buffer;
}


void DeviceManipulationTabController::enableDeviceOffsets(unsigned index, bool enable, bool notify) {
	if (index < deviceInfos.size()) {
		try {
			vrInputEmulator.enableDeviceOffsets(deviceInfos[index]->openvrId, enable);
			deviceInfos[index]->deviceOffsetsEnabled = enable;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Exception caught while setting translation offset: " << e.what();
		}
		if (notify) {
			updateDeviceInfo(index);
			emit deviceInfoChanged(index);
		}
	}
}

void DeviceManipulationTabController::setWorldFromDriverRotationOffset(unsigned index, double yaw, double pitch, double roll, bool notify) {
	if (index < deviceInfos.size()) {
		try {
			vrInputEmulator.setWorldFromDriverRotationOffset(deviceInfos[index]->openvrId, vrmath::quaternionFromYawPitchRoll(yaw * 0.01745329252, pitch * 0.01745329252, roll * 0.01745329252));
			deviceInfos[index]->worldFromDriverRotationOffset.v[0] = yaw;
			deviceInfos[index]->worldFromDriverRotationOffset.v[1] = pitch;
			deviceInfos[index]->worldFromDriverRotationOffset.v[2] = roll;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Exception caught while setting WorldFromDriver rotation offset: " << e.what();
		}
		if (notify) {
			updateDeviceInfo(index);
			emit deviceInfoChanged(index);
		}
	}
}

void DeviceManipulationTabController::setWorldFromDriverTranslationOffset(unsigned index, double x, double y, double z, bool notify) {
	if (index < deviceInfos.size()) {
		try {
			vrInputEmulator.setWorldFromDriverTranslationOffset(deviceInfos[index]->openvrId, { x * 0.01, y * 0.01, z * 0.01 });
			deviceInfos[index]->worldFromDriverTranslationOffset.v[0] = x;
			deviceInfos[index]->worldFromDriverTranslationOffset.v[1] = y;
			deviceInfos[index]->worldFromDriverTranslationOffset.v[2] = z;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Exception caught while setting WorldFromDriver translation offset: " << e.what();
		}
		if (notify) {
			updateDeviceInfo(index);
			emit deviceInfoChanged(index);
		}
	}
}

void DeviceManipulationTabController::setDriverFromHeadRotationOffset(unsigned index, double yaw, double pitch, double roll, bool notify) {
	if (index < deviceInfos.size()) {
		try {
			vrInputEmulator.setDriverFromHeadRotationOffset(deviceInfos[index]->openvrId, vrmath::quaternionFromYawPitchRoll(yaw * 0.01745329252, pitch * 0.01745329252, roll * 0.01745329252));
			deviceInfos[index]->driverFromHeadRotationOffset.v[0] = yaw;
			deviceInfos[index]->driverFromHeadRotationOffset.v[1] = pitch;
			deviceInfos[index]->driverFromHeadRotationOffset.v[2] = roll;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Exception caught while setting DriverFromHead rotation offset: " << e.what();
		}
		if (notify) {
			updateDeviceInfo(index);
			emit deviceInfoChanged(index);
		}
	}
}

void DeviceManipulationTabController::setDriverFromHeadTranslationOffset(unsigned index, double x, double y, double z, bool notify) {
	if (index < deviceInfos.size()) {
		try {
			vrInputEmulator.setDriverFromHeadTranslationOffset(deviceInfos[index]->openvrId, { x * 0.01, y * 0.01, z * 0.01 });
			deviceInfos[index]->driverFromHeadTranslationOffset.v[0] = x;
			deviceInfos[index]->driverFromHeadTranslationOffset.v[1] = y;
			deviceInfos[index]->driverFromHeadTranslationOffset.v[2] = z;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Exception caught while setting WorldFromDriver translation offset: " << e.what();
		}
		if (notify) {
			updateDeviceInfo(index);
			emit deviceInfoChanged(index);
		}
	}
}

void DeviceManipulationTabController::setDriverRotationOffset(unsigned index, double yaw, double pitch, double roll, bool notify) {
	if (index < deviceInfos.size()) {
		try {
			vrInputEmulator.setDriverRotationOffset(deviceInfos[index]->openvrId, vrmath::quaternionFromYawPitchRoll(yaw * 0.01745329252, pitch * 0.01745329252, roll * 0.01745329252));
			deviceInfos[index]->deviceRotationOffset.v[0] = yaw;
			deviceInfos[index]->deviceRotationOffset.v[1] = pitch;
			deviceInfos[index]->deviceRotationOffset.v[2] = roll;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Exception caught while setting Driver rotation offset: " << e.what();
		}
		if (notify) {
			updateDeviceInfo(index);
			emit deviceInfoChanged(index);
		}
	}
}

void DeviceManipulationTabController::setDriverTranslationOffset(unsigned index, double x, double y, double z, bool notify) {
	if (index < deviceInfos.size()) {
		try {
			vrInputEmulator.setDriverTranslationOffset(deviceInfos[index]->openvrId, { x * 0.01, y * 0.01, z * 0.01 });
			deviceInfos[index]->deviceTranslationOffset.v[0] = x;
			deviceInfos[index]->deviceTranslationOffset.v[1] = y;
			deviceInfos[index]->deviceTranslationOffset.v[2] = z;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Exception caught while setting WorldFromDriver translation offset: " << e.what();
		}
		if (notify) {
			updateDeviceInfo(index);
			emit deviceInfoChanged(index);
		}
	}
}

// 0 .. normal, 1 .. disable, 2 .. redirect mode, 3 .. swap mode, 4 ... motion compensation
void DeviceManipulationTabController::setDeviceMode(unsigned index, unsigned mode, unsigned targedIndex, bool notify) {
	try {
		switch (mode) {
		case 0:
			vrInputEmulator.setDeviceNormalMode(deviceInfos[index]->openvrId);
			break;
		case 1:
			vrInputEmulator.setDeviceFakeDisconnectedMode(deviceInfos[index]->openvrId);
			break;
		case 2:
			vrInputEmulator.setDeviceRedictMode(deviceInfos[index]->openvrId, deviceInfos[targedIndex]->openvrId);
			break;
		case 3:
			vrInputEmulator.setDeviceSwapMode(deviceInfos[index]->openvrId, deviceInfos[targedIndex]->openvrId);
			break;
		case 4:
			vrInputEmulator.setDeviceMotionCompensationMode(deviceInfos[index]->openvrId, motionCompensationCenter, motionCompensationCenterMode % 2 == 0 ? false : true);
			break;
		default:
			LOG(ERROR) << "Unkown device mode";
			break;
		}
	} catch (std::exception& e) {
		LOG(ERROR) << "Exception caught while setting device mode: " << e.what();
	}
	if (notify) {
		updateDeviceInfo(index);
		emit deviceInfoChanged(index);
	}
}


bool DeviceManipulationTabController::updateDeviceInfo(unsigned index) {
	bool retval = false;
	if (index < deviceInfos.size()) {
		try {
			vrinputemulator::DeviceInfo info;
			vrInputEmulator.getDeviceInfo(deviceInfos[index]->openvrId, info);
			if (deviceInfos[index]->deviceMode != info.deviceMode) {
				deviceInfos[index]->deviceMode = info.deviceMode;
				retval = true;
			}
			if (deviceInfos[index]->deviceOffsetsEnabled != info.offsetsEnabled) {
				deviceInfos[index]->deviceOffsetsEnabled = info.offsetsEnabled;
				retval = true;
			}
			if (deviceInfos[index]->deviceMode == 2 || deviceInfos[index]->deviceMode == 3) {
				auto status = info.redirectSuspended ? 1 : 0;
				if (deviceInfos[index]->deviceStatus != status) {
					deviceInfos[index]->deviceStatus = status;
					retval = true;
				}
			}
		} catch (std::exception& e) {
			LOG(ERROR) << "Exception caught while getting device info: " << e.what();
		}
	}
	return retval;
}

void DeviceManipulationTabController::triggerHapticPulse(unsigned index) {
	try {
		// When I use a thread everything works in debug modus, but as soon as I switch to release mode I get a segmentation fault
		// Hard to debug since it works in debug modus and therefore I cannot use a debugger :-(
		for (unsigned i = 0; i < 50; ++i) {
			vrInputEmulator.triggerHapticPulse(deviceInfos[index]->openvrId, 0, 2000, true);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		/*if (identifyThread.joinable()) {
			identifyThread.join();
		}
		identifyThread = std::thread([](uint32_t openvrId, vrinputemulator::VRInputEmulator* vrInputEmulator) {
			for (unsigned i = 0; i < 50; ++i) {
				vrInputEmulator->triggerHapticPulse(openvrId, 0, 2000, true);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}, this->deviceInfos[index]->openvrId, &vrInputEmulator);*/
	} catch (std::exception& e) {
		LOG(ERROR) << "Exception caught while triggering haptic pulse: " << e.what();
	}
}

void DeviceManipulationTabController::setMotionCompensationCenter(int mode, double x, double y, double z) {
	motionCompensationCenterMode = mode;
	motionCompensationCenter = { x, y, z };
	vr::HmdVector3d_t centerRaw;
	if (mode < 2) { // standing universe
		auto trans1 = vr::VRSystem()->GetRawZeroPoseToStandingAbsoluteTrackingPose();
		vr::HmdVector3d_t standingPos = { x - trans1.m[0][3], y - trans1.m[1][3], z - trans1.m[2][3] };
		centerRaw = vrmath::matMul33(vrmath::transposeMul33(trans1), standingPos);
	} else {
		centerRaw = motionCompensationCenter;
	}
	vrInputEmulator.setMotionCompensationCenter(centerRaw, motionCompensationCenterMode % 2 == 0 ? false : true);
	saveDeviceManipulationSettings();
	emit motionCompensationSettingsChanged();
}

void DeviceManipulationTabController::retrieveMotionCompensationTmpCenter(int mode, int ref) {
	if (ref == 0 || ref == 1) {
		uint32_t deviceId = vr::k_unTrackedDeviceIndexInvalid;
		if (ref == 0) {
			deviceId = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
		} else {
			deviceId = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
		}
		vr::ETrackingUniverseOrigin universe;
		if (mode == 0 ) {
			universe = vr::TrackingUniverseStanding;
		} else {
			universe = vr::TrackingUniverseRawAndUncalibrated;
		}
		if (deviceId != vr::k_unTrackedDeviceIndexInvalid) {
			vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
			vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(universe, 0.0, poses, vr::k_unMaxTrackedDeviceCount);
			motionCompensationTmpCenter = {
				poses[deviceId].mDeviceToAbsoluteTracking.m[0][3],
				poses[deviceId].mDeviceToAbsoluteTracking.m[2][3],
				poses[deviceId].mDeviceToAbsoluteTracking.m[1][3]
			};
		}

	} else if (ref == 2) {
		auto pos = vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
		motionCompensationTmpCenter = { pos.m[0][3], pos.m[2][3], pos.m[1][3] };
	}
}

void DeviceManipulationTabController::setDeviceRenderModel(unsigned deviceIndex, unsigned renderModelIndex) {
	if (deviceIndex < deviceInfos.size()) {
		if (renderModelIndex == 0) {
			if (deviceInfos[deviceIndex]->renderModelOverlay != vr::k_ulOverlayHandleInvalid) {
				vr::VROverlay()->DestroyOverlay(deviceInfos[deviceIndex]->renderModelOverlay);
				deviceInfos[deviceIndex]->renderModelOverlay = vr::k_ulOverlayHandleInvalid;
			}
		} else {
			vr::VROverlayHandle_t overlayHandle = deviceInfos[deviceIndex]->renderModelOverlay;
			if (overlayHandle == vr::k_ulOverlayHandleInvalid) {
				std::string overlayName = std::string("RenderModelOverlay_") + std::string(deviceInfos[deviceIndex]->serial);
				auto oerror = vr::VROverlay()->CreateOverlay(overlayName.c_str(), overlayName.c_str(), &overlayHandle);
				if (oerror == vr::VROverlayError_None) {
					overlayHandle = deviceInfos[deviceIndex]->renderModelOverlay = overlayHandle;
				} else {
					LOG(ERROR) << "Could not create render model overlay: " << vr::VROverlay()->GetOverlayErrorNameFromEnum(oerror);
				}
			}
			if (overlayHandle != vr::k_ulOverlayHandleInvalid) {
				std::string texturePath = QApplication::applicationDirPath().toStdString() + "\\res\\transparent.png";
				if (QFile::exists(QString::fromStdString(texturePath))) {
					vr::VROverlay()->SetOverlayFromFile(overlayHandle, texturePath.c_str());
					char buffer[vr::k_unMaxPropertyStringSize];
					vr::VRRenderModels()->GetRenderModelName(renderModelIndex - 1, buffer, vr::k_unMaxPropertyStringSize);
					vr::VROverlay()->SetOverlayRenderModel(overlayHandle, buffer, nullptr);
					vr::HmdMatrix34_t trans = {
						1.0f, 0.0f, 0.0f, 0.0f,
						0.0f, 1.0f, 0.0f, 0.0f,
						0.0f, 0.0f, 1.0f, 0.0f
					};
					vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlayHandle, deviceInfos[deviceIndex]->openvrId, &trans);
					vr::VROverlay()->ShowOverlay(overlayHandle);
				} else {
					LOG(ERROR) << "Could not find texture \"" << texturePath << "\"";
				}
			}
		}
	}
}

} // namespace inputemulator
