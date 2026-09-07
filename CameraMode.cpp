#include "ControllerInput.h"

// ========== Camera Mode / UDP External Input ==========

bool ControllerMapper::startCameraSenderProcess() {
    if (cameraSenderRunning) return true;

    char modulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) == 0) {
        return false;
    }

    std::string exePath(modulePath);
    size_t slashPos = exePath.find_last_of("\\/");
    std::string exeDir = (slashPos == std::string::npos) ? std::string(".") : exePath.substr(0, slashPos);

    std::string scriptPath = exeDir + "\\camera_sender.py";
    DWORD scriptAttr = GetFileAttributesA(scriptPath.c_str());
    if (scriptAttr == INVALID_FILE_ATTRIBUTES) {
        // Fallback: try current working directory
        scriptPath = "camera_sender.py";
        scriptAttr = GetFileAttributesA(scriptPath.c_str());
        if (scriptAttr == INVALID_FILE_ATTRIBUTES) {
            logError("camera_sender.py not found.");
            return false;
        }
    }

    // Candidate launch commands, in order.
    std::vector<std::string> candidates;

    char pyEnv[512] = {};
    DWORD pyLen = GetEnvironmentVariableA("PYTHON_EXE", pyEnv, sizeof(pyEnv));
    if (pyLen > 0 && pyLen < sizeof(pyEnv)) {
        candidates.push_back(std::string("\"") + pyEnv + "\" \"" + scriptPath + "\" --preview --auto-download-model");
    }

    char foundPython[MAX_PATH] = {};
    if (SearchPathA(nullptr, "python.exe", nullptr, MAX_PATH, foundPython, nullptr) > 0) {
        candidates.push_back(std::string("\"") + foundPython + "\" \"" + scriptPath + "\" --preview --auto-download-model");
    }

    if (SearchPathA(nullptr, "py.exe", nullptr, MAX_PATH, foundPython, nullptr) > 0) {
        candidates.push_back(std::string("\"") + foundPython + "\" -3 \"" + scriptPath + "\" --preview --auto-download-model");
    }

    candidates.push_back(std::string("\"C:\\Program Files\\Python310\\python.exe\" \"") + scriptPath + "\" --preview --auto-download-model");

    STARTUPINFOA si = {};
    si.cb = sizeof(si);

    for (const std::string& cmd : candidates) {
        PROCESS_INFORMATION pi = {};
        std::vector<char> cmdline(cmd.begin(), cmd.end());
        cmdline.push_back('\0');

        BOOL ok = CreateProcessA(
            nullptr,
            cmdline.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            exeDir.c_str(),
            &si,
            &pi
        );

        if (ok) {
            cameraSenderProcess = pi;
            cameraSenderRunning = true;
            logInfo("Auto-started camera_sender.py");
            return true;
        }
    }

    return false;
}

void ControllerMapper::stopCameraSenderProcess() {
    if (!cameraSenderRunning) return;

    if (cameraSenderProcess.hProcess) {
        DWORD waitResult = WaitForSingleObject(cameraSenderProcess.hProcess, 0);
        if (waitResult == WAIT_TIMEOUT) {
            // Best-effort shutdown for child process we started.
            TerminateProcess(cameraSenderProcess.hProcess, 0);
            WaitForSingleObject(cameraSenderProcess.hProcess, 1000);
        }
        CloseHandle(cameraSenderProcess.hProcess);
        cameraSenderProcess.hProcess = nullptr;
    }

    if (cameraSenderProcess.hThread) {
        CloseHandle(cameraSenderProcess.hThread);
        cameraSenderProcess.hThread = nullptr;
    }

    cameraSenderRunning = false;
}

void ControllerMapper::startUDPListener(int port) {
    if (udpRunning.load()) return;
    udpRunning.store(true);
    udpThread = std::thread([this, port]() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            logError("WSAStartup failed for UDP listener");
            udpRunning.store(false);
            return;
        }

        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            logError("Failed to create UDP socket");
            WSACleanup();
            udpRunning.store(false);
            return;
        }

        sockaddr_in localAddr = {};
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // listen on loopback only
        localAddr.sin_port = htons((unsigned short)port);

        if (bind(sock, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
            logError("Failed to bind UDP socket (is port in use?)");
            closesocket(sock);
            WSACleanup();
            udpRunning.store(false);
            return;
        }

        // Non-blocking recv using select with timeout so we can exit cleanly
        char buf[256];
        while (udpRunning.load()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100ms
            int sel = select(0, &fds, nullptr, nullptr, &tv);
            if (sel > 0 && FD_ISSET(sock, &fds)) {
                sockaddr_in remoteAddr = {};
                int addrLen = sizeof(remoteAddr);
                int len = recvfrom(sock, buf, (int)sizeof(buf) - 1, 0, (sockaddr*)&remoteAddr, &addrLen);
                if (len > 0) {
                    buf[len] = '\0';
                    double lx = 0.0, ly = 0.0, rx = 0.0, ry = 0.0;
                    int lp = 0, rp = 0;
                    // Expect CSV: lx,ly,lp,rx,ry,rp
                    int matched = sscanf_s(buf, "%lf,%lf,%d,%lf,%lf,%d", &lx, &ly, &lp, &rx, &ry, &rp);
                    if (matched == 6) {
                        std::lock_guard<std::mutex> lock(udpMutex);
                        // Clamp normalized values to [0..1] to keep camera input sane.
                        if (lx < 0.0) lx = 0.0; else if (lx > 1.0) lx = 1.0;
                        if (ly < 0.0) ly = 0.0; else if (ly > 1.0) ly = 1.0;
                        if (rx < 0.0) rx = 0.0; else if (rx > 1.0) rx = 1.0;
                        if (ry < 0.0) ry = 0.0; else if (ry > 1.0) ry = 1.0;

                        // Apply EMA smoothing similar to camera-side stabilization.
                        if (!externalSmoothingInitialized) {
                            externalLeftX = lx;
                            externalLeftY = ly;
                            externalRightX = rx;
                            externalRightY = ry;
                            externalSmoothingInitialized = true;
                        } else {
                            const double a = externalSmoothingAlpha;
                            externalLeftX = (a * lx) + ((1.0 - a) * externalLeftX);
                            externalLeftY = (a * ly) + ((1.0 - a) * externalLeftY);
                            externalRightX = (a * rx) + ((1.0 - a) * externalRightX);
                            externalRightY = (a * ry) + ((1.0 - a) * externalRightY);
                        }

                        externalLeftPressed = (lp != 0);
                        externalRightPressed = (rp != 0);
                        externalLastPacketMs = GetTickCount64();
                        useExternalInput = true;
                    }
                }
            }
        }

        closesocket(sock);
        WSACleanup();
    });
}

void ControllerMapper::stopUDPListener() {
    if (!udpRunning.load()) return;
    udpRunning.store(false);
    if (udpThread.joinable()) udpThread.join();
}

// Camera mode handler: forward to touch handler using only primary flags
void ControllerMapper::handleCameraControl(bool l1, bool r1, double leftX, double leftY, double rightX, double rightY) {
    // No trigger or stick-press emulation; pass false for l2,r2,l3,r3
    handleTouchControl(l1, r1, false, false, false, false, leftX, leftY, rightX, rightY);
}
