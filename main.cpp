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
        std::cout << "Select mode (1-3): ";
        
        InputMode selectedMode = InputMode::Touch; // Default
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
            default:
                std::cout << "Invalid choice. Please select 1, 2, or 3." << std::endl;
                std::cout << std::endl;
                continue; // Go back to mode selection
        }
        std::cout << std::endl;

        try {
            ControllerMapper app(selectedMode);
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