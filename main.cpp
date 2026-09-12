#include "ControllerInput.h"

int main() {
    // Keep monitor geometry and injected touch coordinates in physical pixels.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

    while (true) {
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

        InputMode selectedMode = InputMode::Touch;
        CameraInputMode selectedCameraMode = CameraInputMode::DS4Led;
        int selectedCameraIndex = -1;
        char choice = _getch();
        if (choice == 27) {
            std::cout << std::endl << std::endl;
            continue;
        }
        std::cout << choice << std::endl << std::endl;

        if (choice == '1') {
            selectedMode = InputMode::Camera;
            std::cout << "Starting in CAMERA mode..." << std::endl;

            bool cameraSetupDone = false;
            bool retreatToModeSelection = false;
            while (!cameraSetupDone) {
                std::cout << "Choose camera input (ESC to go back):" << std::endl;
                std::cout << "  [1] Track DS4 LED, use L1/R1 for click" << std::endl;
                std::cout << "  [2] Push for click" << std::endl;
                std::cout << "  [3] Open hand for click (curl for rest)" << std::endl;
                std::cout << "Select camera input (1-3): ";
                char cameraChoice = _getch();
                if (cameraChoice == 27) {
                    std::cout << std::endl << std::endl;
                    retreatToModeSelection = true;
                    break;
                }
                std::cout << cameraChoice << std::endl << std::endl;
                if (cameraChoice == '1') {
                    selectedCameraMode = CameraInputMode::DS4Led;
                } else if (cameraChoice == '2') {
                    selectedCameraMode = CameraInputMode::Push;
                } else if (cameraChoice == '3') {
                    selectedCameraMode = CameraInputMode::Curl;
                } else {
                    std::cout << "Invalid camera input. Using DS4 LED tracking." << std::endl;
                    selectedCameraMode = CameraInputMode::DS4Led;
                }

                std::cout << "Choose camera source (ESC to go back):" << std::endl;
                std::cout << "  [1] Webcam / camera device" << std::endl;
                std::cout << "  [2] scrcpy USB window" << std::endl;
                std::cout << "  [3] Entire monitor containing scrcpy" << std::endl;
                std::cout << "Select source (1-3): ";
                char sourceChoice = _getch();
                if (sourceChoice == 27) {
                    std::cout << std::endl << std::endl;
                    continue;
                }
                std::cout << sourceChoice << std::endl << std::endl;
                selectedCameraIndex = -1;
                if (sourceChoice == '2') {
                    selectedCameraIndex = -2;
                    std::cout << "Start scrcpy first with a window title containing 'scrcpy'." << std::endl;
                } else if (sourceChoice == '3') {
                    selectedCameraIndex = -3;
                    std::cout << "Maximize scrcpy on the monitor you choose; the entire monitor will be captured." << std::endl;
                }
                cameraSetupDone = true;
            }

            if (retreatToModeSelection) {
                continue;
            }
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
            std::cout << std::endl;
            continue;
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
