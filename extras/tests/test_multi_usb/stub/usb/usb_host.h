#pragma once
#include <cstdint>
#include <cstddef>

// Opaque handle types
typedef void* usb_device_handle_t;
typedef void* usb_host_client_handle_t;

// Error codes
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103

// Interrupt flags
#define ESP_INTR_FLAG_LEVEL1 (1<<1)

// Config structures
struct usb_host_config_t {
    bool skip_phy_setup;
    int intr_flags;
};

struct usb_host_client_config_t {
    bool is_synchronous;
    int max_num_event_msg;
    struct {
        void (*client_event_callback)(const struct usb_host_client_event_msg_t*, void*);
        void* callback_arg;
    } async;
};

// Transfer structure
struct usb_transfer_t {
    usb_device_handle_t device_handle;
    uint8_t bEndpointAddress;
    void (*callback)(usb_transfer_t*);
    void* context;
    int num_bytes;
    int actual_num_bytes;
    int status;
    uint8_t* data_buffer;
};

// Config descriptor
struct usb_config_desc_t {
    const uint8_t* val;
    uint16_t wTotalLength;
};

// Client event message
enum usb_host_client_event_t {
    USB_HOST_CLIENT_EVENT_NEW_DEV,
    USB_HOST_CLIENT_EVENT_DEV_GONE,
};

struct usb_host_client_event_msg_t {
    usb_host_client_event_t event;
    union {
        struct { uint8_t address; } new_dev;
        struct { usb_device_handle_t dev_hdl; } dev_gone;
    };
};

// Stub function declarations (no-op implementations)
inline esp_err_t usb_host_install(const usb_host_config_t*) { return ESP_OK; }
inline esp_err_t usb_host_client_register(const usb_host_client_config_t*, usb_host_client_handle_t*) { return ESP_OK; }
inline esp_err_t usb_host_device_open(usb_host_client_handle_t, uint8_t, usb_device_handle_t*) { return ESP_OK; }
inline esp_err_t usb_host_device_close(usb_host_client_handle_t, usb_device_handle_t) { return ESP_OK; }
inline esp_err_t usb_host_get_active_config_descriptor(usb_device_handle_t, const usb_config_desc_t**) { return ESP_OK; }
inline esp_err_t usb_host_interface_claim(usb_host_client_handle_t, usb_device_handle_t, uint8_t, uint8_t) { return ESP_OK; }
inline esp_err_t usb_host_interface_release(usb_host_client_handle_t, usb_device_handle_t, uint8_t) { return ESP_OK; }
inline esp_err_t usb_host_transfer_alloc(int size, int, usb_transfer_t** out) {
    *out = new usb_transfer_t();
    (*out)->data_buffer = new uint8_t[size]();
    return ESP_OK;
}
inline void usb_host_transfer_free(usb_transfer_t* t) {
    if (t) { delete[] t->data_buffer; delete t; }
}
inline esp_err_t usb_host_transfer_submit(usb_transfer_t*) { return ESP_OK; }
inline esp_err_t usb_host_lib_handle_events(int, uint32_t*) { return ESP_OK; }
inline esp_err_t usb_host_client_handle_events(usb_host_client_handle_t, int) { return ESP_OK; }
inline esp_err_t usb_host_client_deregister(usb_host_client_handle_t) { return ESP_OK; }
inline esp_err_t usb_host_uninstall() { return ESP_OK; }
