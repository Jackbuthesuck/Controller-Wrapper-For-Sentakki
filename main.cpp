#include "ControllerInput.h"

int main() {
    // Allocate console for debug output
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    
    // Main loop: Show mode selection → Run app → On restart, loop back
    while (true) {
        // ========== Mode Selection Menu ==========
        std::cout << "========================================" << std::endl;
        std::cout << "    CONTROLLER INPUT MAPPER" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        std::cout << "Choose input mode:" << std::endl;
        std::cout << std::endl;
        std::cout << "  [1] Touch Mode (Simulate Windows Touch Input)" << std::endl;
        std::cout << std::endl;
        std::cout << "  [2] Mouse Mode (Control Mouse Cursor)" << std::endl;
        std::cout << std::endl;
        std::cout << "  [3] Keyboard Mode (Control Keyboard Keys)" << std::endl;
        std::cout << std::endl;
        std::cout << "  [4] Camera Mode (Use external CV input via UDP)" << std::endl;
        std::cout << std::endl;
        std::cout << "Select mode (1-4): ";
        
        InputMode selectedMode = InputMode::Touch; // Default
        CameraInputMode selectedCameraMode = CameraInputMode::Push;
        int selectedCameraIndex = -1;
        char choice = _getch();
        std::cout << choice << std::endl << std::endl;
        
        switch (choice) {
            case '1':
                selectedMode = InputMode::Touch;
                std::cout << "Starting in TOUCH mode..." << std::endl;
                break;
            case '2':
                selectedMode = InputMode::Mouse;
                std::cout << "Starting in MOUSE mode..." << std::endl;
                break;
            case '3':
                selectedMode = InputMode::Keyboard;
                std::cout << "Starting in KEYBOARD mode..." << std::endl;
                break;
            case '4': {
                selectedMode = InputMode::Camera;
                std::cout << "Starting in CAMERA mode..." << std::endl;
                std::cout << "Choose camera input:" << std::endl;
                std::cout << "  [1] Push for click" << std::endl;
                std::cout << "  [2] Open hand for click (curl for rest)" << std::endl;
                std::cout << "  [3] Track DS4 LED, use L1/R1 for click" << std::endl;
                std::cout << "Select camera input (1-3): ";
                {
                    char cameraChoice = _getch();
                    std::cout << cameraChoice << std::endl << std::endl;
                    if (cameraChoice == '2') {
                        selectedCameraMode = CameraInputMode::Curl;
                    } else if (cameraChoice == '3') {
                        selectedCameraMode = CameraInputMode::DS4Led;
                    } else if (cameraChoice != '1') {
                        std::cout << "Invalid camera input. Using push for click." << std::endl;
                    }
                }
                std::cout << "Choose camera source:" << std::endl;
                std::cout << "  [1] Webcam / camera device" << std::endl;
                std::cout << "  [2] scrcpy USB window" << std::endl;
                std::cout << "  [3] Entire monitor containing scrcpy" << std::endl;
                std::cout << "Select source (1-3): ";
                char sourceChoice = _getch();
                std::cout << sourceChoice << std::endl << std::endl;
                if (sourceChoice == '2') {
                    selectedCameraIndex = -2;
                    std::cout << "Start scrcpy first with a window title containing 'scrcpy'." << std::endl;
                } else if (sourceChoice == '3') {
                    selectedCameraIndex = -3;
                    std::cout << "Maximize scrcpy on the monitor you choose; the entire monitor will be captured." << std::endl;
                }
                break;
            }
            default:
                std::cout << "Invalid choice. Please select 1, 2, or 3." << std::endl;
                std::cout << std::endl;
                continue; // Go back to mode selection
        }
        std::cout << std::endl;

        try {
            ControllerMapper app(selectedMode, selectedCameraMode, selectedCameraIndex);
            if (!app.initialize()) {
                std::cerr << "[ERROR] Failed to initialize application!" << std::endl;
                continue;
            }
            app.run();
            
            // If we get here, the window was closed (restart requested)
            // Loop back to mode selection menu
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            std::cerr << "Press any key to continue or Ctrl+C to exit..." << std::endl;
            _getch();
            std::cout << std::endl;
        }
        
        // Loop back to show mode selection menu again
    }
    
    FreeConsole();
    return 0;
}