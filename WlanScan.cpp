#include <iostream>
#include <Windows.h>
#include <Wlanapi.h>
#pragma comment(lib, "Wlanapi.lib")

const DWORD SCAN_TIMEOUT = 10000;

class NotificationHandler {
public:
    NotificationHandler() {
        hScanCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    ~NotificationHandler() {
        CloseHandle(hScanCompleteEvent);
    }

    HANDLE getEventHandle() const {
        return hScanCompleteEvent;
    }

    static VOID WINAPI WlanNotificationCallback(PWLAN_NOTIFICATION_DATA pData, PVOID pContext) {
        if (pData->NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM &&
            pData->NotificationCode == wlan_notification_acm_scan_complete) {
            SetEvent(((NotificationHandler*)pContext)->getEventHandle());
        }
    }

private:
    HANDLE hScanCompleteEvent;
};

class WlanManager {
public:
    WlanManager() : hClient(NULL), pIfList(NULL), pNetList(NULL) {}

    ~WlanManager() {
        if (pIfList) WlanFreeMemory(pIfList);
        if (pNetList) WlanFreeMemory(pNetList);
        if (hClient) WlanCloseHandle(hClient, NULL);
    }

    bool initialize() {
        return WlanOpenHandle(2, NULL, &dwVersion, &hClient) == ERROR_SUCCESS;
    }

    bool enumerateInterfaces() {
        return WlanEnumInterfaces(hClient, NULL, &pIfList) == ERROR_SUCCESS;
    }

    bool registerForNotifications(NotificationHandler& handler) {
        return WlanRegisterNotification(hClient, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, &NotificationHandler::WlanNotificationCallback, &handler, NULL, NULL) == ERROR_SUCCESS;
    }

    bool scan() {
        return WlanScan(hClient, &pIfList->InterfaceInfo[0].InterfaceGuid, NULL, NULL, NULL) == ERROR_SUCCESS;
    }

    bool getAvailableNetworks() {
        return WlanGetAvailableNetworkList(hClient, &pIfList->InterfaceInfo[0].InterfaceGuid, 0, NULL, &pNetList) == ERROR_SUCCESS;
    }

    void printNetworks() {
        for (DWORD i = 0; i < pNetList->dwNumberOfItems; i++) {
            std::wstring ssid(pNetList->Network[i].dot11Ssid.ucSSID,
                pNetList->Network[i].dot11Ssid.ucSSID + pNetList->Network[i].dot11Ssid.uSSIDLength);
            std::wcout << L"Network: " << ssid << std::endl;
        }
    }

private:
    HANDLE hClient;
    DWORD dwVersion;
    PWLAN_INTERFACE_INFO_LIST pIfList;
    PWLAN_AVAILABLE_NETWORK_LIST pNetList;
};

int main() {
    WlanManager wlanManager;
    NotificationHandler notificationHandler;

    if (!wlanManager.initialize()) {
        std::cerr << "Failed to initialize WLAN client." << std::endl;
        return 1;
    }

    if (!wlanManager.enumerateInterfaces()) {
        std::cerr << "Failed to enumerate WLAN interfaces." << std::endl;
        return 1;
    }

    if (!wlanManager.registerForNotifications(notificationHandler)) {
        std::cerr << "Failed to register for notifications." << std::endl;
        return 1;
    }

    if (!wlanManager.scan()) {
        std::cerr << "Failed to scan for networks." << std::endl;
        return 1;
    }

    WaitForSingleObject(notificationHandler.getEventHandle(), SCAN_TIMEOUT);

    if (!wlanManager.getAvailableNetworks()) {
        std::cerr << "Failed to get available networks." << std::endl;
        return 1;
    }

    wlanManager.printNetworks();

    return 0;
}
