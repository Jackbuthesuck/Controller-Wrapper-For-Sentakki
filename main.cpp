#include "ControllerInput.h"
#include <fstream>
#include <regex>

struct StartupConfig {
	bool autoStart = false;
	int startupMode = 0;
	int cameraInputMode = 0;
	int cameraSource = 0;
	int cameraIndex = -1;
};

static bool readJsonString(const std::string& json, const char* key, std::string& value) {
	const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
	std::smatch match;
	if (!std::regex_search(json, match, pattern)) {
		return false;
	}
	value = match[1].str();
	return true;
}

static bool readJsonBool(const std::string& json, const char* key, bool& value) {
	const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false|0|1)\\b");
	std::smatch match;
	if (!std::regex_search(json, match, pattern)) {
		return false;
	}
	value = match[1].str() == "true" || match[1].str() == "1";
	return true;
}

static bool readJsonInt(const std::string& json, const char* key, int& value) {
	const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(-?[0-9]+)");
	std::smatch match;
	if (!std::regex_search(json, match, pattern)) {
		return false;
	}
	value = std::stoi(match[1].str());
	return true;
}

static bool readJsonIndex(const std::string& json, const char* key, int& value) {
	if (readJsonInt(json, key, value)) {
		return true;
	}

	std::string name;
	if (!readJsonString(json, key, name)) {
		return false;
	}
	if (std::string(key) == "startup_mode") {
		if (name == "camera") value = 1;
		else if (name == "touch") value = 2;
		else if (name == "keyboard") value = 3;
		else if (name == "mouse") value = 4;
		else return false;
	} else if (std::string(key) == "input_mode" || std::string(key) == "camera_input_mode") {
		if (name == "ds4led") value = 1;
		else if (name == "push") value = 2;
		else if (name == "open" || name == "curl") value = 3;
		else return false;
	} else if (std::string(key) == "camera_source") {
		if (name == "webcam") value = 1;
		else if (name == "scrcpy_window") value = 2;
		else if (name == "scrcpy_screen") value = 3;
		else return false;
	} else {
		return false;
	}
	return true;
}

static StartupConfig loadStartupConfig() {
	char modulePath[MAX_PATH] = {};
	std::vector<std::string> configPaths;
	#ifdef _DEBUG
	configPaths.push_back("camera_config.json");
	#endif
	if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) > 0) {
		std::string executablePath(modulePath);
		size_t slashPos = executablePath.find_last_of("\\/");
		if (slashPos != std::string::npos) {
			configPaths.push_back(executablePath.substr(0, slashPos) + "\\camera_config.json");
		}
	}
	#ifndef _DEBUG
	configPaths.push_back("camera_config.json");
	#endif

	std::ifstream configFile;
	for (const std::string& configPath : configPaths) {
		configFile.open(configPath);
		if (configFile) {
			break;
		}
		configFile.clear();
	}
	if (!configFile) return {};

	const std::string json((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>());
	StartupConfig config;
	readJsonBool(json, "auto_start", config.autoStart);
	readJsonIndex(json, "startup_mode", config.startupMode);
	if (!readJsonIndex(json, "input_mode", config.cameraInputMode)) {
		readJsonIndex(json, "camera_input_mode", config.cameraInputMode);
	}
	readJsonIndex(json, "camera_source", config.cameraSource);
	readJsonInt(json, "camera_index", config.cameraIndex);
	return config;
}

static bool applyStartupConfig(const StartupConfig& config, InputMode& selectedMode, CameraInputMode& selectedCameraMode, int& selectedCameraIndex) {
	if (config.startupMode == 1) {
		selectedMode = InputMode::Camera;
		if (config.cameraInputMode == 0) {
			selectedCameraMode = CameraInputMode::DS4Led;
		} else if (config.cameraInputMode == 1) {
			selectedCameraMode = CameraInputMode::DS4Led;
		} else if (config.cameraInputMode == 2) {
			selectedCameraMode = CameraInputMode::Push;
		} else if (config.cameraInputMode == 3) {
			selectedCameraMode = CameraInputMode::Curl;
		} else {
			return false;
		}

		if (config.cameraSource == 0) {
			selectedCameraIndex = -1;
		} else if (config.cameraSource == 1) {
			selectedCameraIndex = config.cameraIndex;
		} else if (config.cameraSource == 2) {
			selectedCameraIndex = -2;
		} else if (config.cameraSource == 3) {
			selectedCameraIndex = -3;
		} else {
			return false;
		}
		return true;
	}
	if (config.startupMode == 2) {
		selectedMode = InputMode::Touch;
		return true;
	}
	if (config.startupMode == 3) {
		selectedMode = InputMode::Keyboard;
		return true;
	}
	if (config.startupMode == 4) {
		selectedMode = InputMode::Mouse;
		return true;
	}
	return false;
}

static bool applyCameraInputMode(int choice, CameraInputMode& selectedCameraMode) {
	if (choice == 1) {
		selectedCameraMode = CameraInputMode::DS4Led;
	} else if (choice == 2) {
		selectedCameraMode = CameraInputMode::Push;
	} else if (choice == 3) {
		selectedCameraMode = CameraInputMode::Curl;
	} else {
		return false;
	}
	return true;
}

static bool selectModeInteractively(InputMode& selectedMode) {
	std::cout << "========================================" << std::endl;
	std::cout << "    CONTROLLER INPUT MAPPER" << std::endl;
	std::cout << "========================================" << std::endl;
	std::cout << std::endl;
	std::cout << "Choose input mode:" << std::endl;
	std::cout << std::endl;
	std::cout << "  [1] Camera Mode (Use external CV input via UDP)" << std::endl;
	std::cout << std::endl;
	std::cout << "  [2] Touch Mode (Simulate Windows Touch Input)" << std::endl;
	std::cout << std::endl;
	std::cout << "  [3] Keyboard Mode (Control Keyboard Keys)" << std::endl;
	std::cout << std::endl;
	std::cout << "  [4] Mouse Mode (Control Mouse Cursor)" << std::endl;
	std::cout << std::endl;
	std::cout << "Select mode (1-4): ";

	char choice = _getch();
	if (choice == 27) {
		std::cout << std::endl << std::endl;
		return false;
	}
	std::cout << choice << std::endl << std::endl;

	if (choice == '1') {
		selectedMode = InputMode::Camera;
		std::cout << "Starting in CAMERA mode..." << std::endl;
	} else if (choice == '2') {
		selectedMode = InputMode::Touch;
		std::cout << "Starting in TOUCH mode..." << std::endl;
	} else if (choice == '3') {
		selectedMode = InputMode::Keyboard;
		std::cout << "Starting in KEYBOARD mode..." << std::endl;
	} else if (choice == '4') {
		selectedMode = InputMode::Mouse;
		std::cout << "Starting in MOUSE mode..." << std::endl;
	} else {
		std::cout << "Invalid choice. Please select 1-4." << std::endl;
		return false;
	}
	return true;
}

static bool selectCameraSetup(const StartupConfig& config, bool useConfig, CameraInputMode& selectedCameraMode, int& selectedCameraIndex) {
	if (useConfig && config.cameraInputMode != 0) {
		if (!applyCameraInputMode(config.cameraInputMode, selectedCameraMode)) {
			return false;
		}
	} else {
		std::cout << "Choose camera input (ESC to go back):" << std::endl;
		std::cout << "  [1] Track DS4 LED, use L1/R1 for click" << std::endl;
		std::cout << "  [2] Push for click" << std::endl;
		std::cout << "  [3] Open hand for click (curl for rest)" << std::endl;
		std::cout << "Select camera input (1-3): ";
		char cameraChoice = _getch();
		if (cameraChoice == 27) {
			std::cout << std::endl << std::endl;
			return false;
		}
		std::cout << cameraChoice << std::endl << std::endl;
		if (!applyCameraInputMode(cameraChoice - '0', selectedCameraMode)) {
			std::cout << "Invalid camera input. Using DS4 LED tracking." << std::endl;
			selectedCameraMode = CameraInputMode::DS4Led;
		}
	}

	if (useConfig && config.cameraSource != 0) {
		if (config.cameraSource == 1) {
			selectedCameraIndex = config.cameraIndex;
		} else if (config.cameraSource == 2) {
			selectedCameraIndex = -2;
		} else if (config.cameraSource == 3) {
			selectedCameraIndex = -3;
		} else {
			return false;
		}
		return true;
	}

	std::cout << "Choose camera source (ESC to go back):" << std::endl;
	std::cout << "  [1] Webcam / camera device" << std::endl;
	std::cout << "  [2] scrcpy USB window" << std::endl;
	std::cout << "  [3] Entire monitor containing scrcpy" << std::endl;
	std::cout << "Select source (1-3): ";
	char sourceChoice = _getch();
	if (sourceChoice == 27) {
		std::cout << std::endl << std::endl;
		return false;
	}
	std::cout << sourceChoice << std::endl << std::endl;
	if (sourceChoice == '1') {
		selectedCameraIndex = useConfig ? config.cameraIndex : -1;
	} else if (sourceChoice == '2') {
		selectedCameraIndex = -2;
		std::cout << "Start scrcpy first with a window title containing 'scrcpy'." << std::endl;
	} else if (sourceChoice == '3') {
		selectedCameraIndex = -3;
		std::cout << "Maximize scrcpy on the monitor you choose; the entire monitor will be captured." << std::endl;
	} else {
		selectedCameraIndex = config.cameraIndex;
	}
	return true;
}

int main() {
	// Keep monitor geometry and injected touch coordinates in physical pixels.
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

	StartupConfig startupConfig = loadStartupConfig();
	while (true) {
		InputMode selectedMode = InputMode::Touch;
		CameraInputMode selectedCameraMode = CameraInputMode::DS4Led;
		int selectedCameraIndex = -1;
		const bool autoModeSelected = startupConfig.autoStart && startupConfig.startupMode != 0;
		if (autoModeSelected) {
			if (!applyStartupConfig(startupConfig, selectedMode, selectedCameraMode, selectedCameraIndex)) {
				std::cerr << "Invalid auto-start settings in camera_config.json; returning to interactive mode." << std::endl;
				startupConfig.autoStart = false;
				continue;
			}
			std::cout << "Auto-starting from camera_config.json..." << std::endl;
		} else if (!selectModeInteractively(selectedMode)) {
			continue;
		}

		if (selectedMode == InputMode::Camera &&
			(!autoModeSelected || startupConfig.cameraInputMode == 0 || startupConfig.cameraSource == 0)) {
			if (!selectCameraSetup(startupConfig, startupConfig.autoStart, selectedCameraMode, selectedCameraIndex)) {
				continue;
			}
		}

		std::cout << std::endl;
		try {
			ControllerMapper app(selectedMode, selectedCameraMode, selectedCameraIndex);
			if (!app.initialize()) {
				std::cerr << "[ERROR] Failed to initialize application!" << std::endl;
				continue;
			}
			app.run();
		} catch (const std::exception& e) {
			std::cerr << "Error: " << e.what() << std::endl;
			std::cerr << "Press any key to continue or Ctrl+C to exit..." << std::endl;
			_getch();
			std::cout << std::endl;
		}
	}

	FreeConsole();
	return 0;
}
