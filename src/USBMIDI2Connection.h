#ifndef USB_MIDI2_CONNECTION_H
#define USB_MIDI2_CONNECTION_H

#include "USBConnection.h"

// USBMIDI2Connection — USB Host with MIDI 2.0/UMP negotiation.
//
// Overrides _processConfig() to scan for both Alt 0 (bcdMSC 1.00) and
// Alt 1 (bcdMSC 2.00) in the device's configuration descriptor.
// Prefers Alt 1 when available.
//
// After selecting Alt 1, performs the mandatory UMP Endpoint Discovery
// and Protocol Negotiation sequence (UMP Stream Messages, MT 0x0F):
//   1. Endpoint Discovery Request → receives Endpoint Info
//   2. Stream Configuration Request (MIDI 2.0 Protocol)
//   3. Function Block Discovery Request → receives FB Info
//
// In MIDI 2.0 mode, bulk IN data is dispatched as raw UMP words via
// dispatchUMPData(). In MIDI 1.0 mode, behaviour is identical to the
// base USBConnection.
//
// Hardware: USB-A host port (e.g. T-Display-S3 MIDI Shield).
//
// Usage:
//   USBMIDI2Connection usb;
//   usb.setUMPCallback(onUMP, nullptr);    // MIDI 2.0 native
//   usb.setMidiCallback(onMidi, nullptr);  // MIDI 1.0 fallback
//   usb.begin();

class USBMIDI2Connection : public USBConnection {
public:
    USBMIDI2Connection();

    const char* name() const override { return "USB Host MIDI2"; }

    // True when the connected device negotiated MIDI 2.0 (Alt 1).
    bool isMIDI2() const { return _midi2Active; }

    // True when Protocol Negotiation completed successfully.
    bool isNegotiated() const { return _negotiationState == NegDone; }

    // Send raw UMP words to the device via OUT endpoint.
    // Returns true if the transfer was submitted successfully.
    bool sendUMPMessage(const uint32_t* words, uint8_t count);

    // Retry Protocol Negotiation from scratch (useful when the device
    // was not ready when the Host first sent Endpoint Discovery).
    void retryNegotiation();

    // Device info discovered during negotiation.
    struct EndpointInfo {
        uint8_t  umpVersionMajor;
        uint8_t  umpVersionMinor;
        uint8_t  numFunctionBlocks;
        bool     supportsMIDI2Protocol;
        bool     supportsMIDI1Protocol;
        bool     supportsRxJR;
        bool     supportsTxJR;
        uint8_t  currentProtocol;  // 0x01=MIDI1, 0x02=MIDI2
    };

    struct FunctionBlockInfo {
        uint8_t  index;
        bool     active;
        uint8_t  direction;     // 0=input, 1=output, 2=bidirectional
        uint8_t  firstGroup;
        uint8_t  groupLength;
        uint8_t  midiCISupport;
        uint8_t  isMIDI1;       // 0=not MIDI1, 1=MIDI1 unrestricted, 2=MIDI1 restricted
        uint8_t  maxSysEx8Streams;
    };

    struct GroupTerminalBlock {
        uint8_t  id;
        uint8_t  type;           // 0=bidirectional, 1=input-only, 2=output-only
        uint8_t  firstGroup;
        uint8_t  numGroups;
        uint8_t  protocol;       // 0x01=MIDI1, 0x02=MIDI2, 0x00=unknown
        uint16_t maxInputBW;
        uint16_t maxOutputBW;
    };

    static const uint8_t MAX_FUNCTION_BLOCKS = 8;
    static const uint8_t MAX_GTB = 8;
    static const uint8_t MAX_NAME_LEN = 64;

    const EndpointInfo& getEndpointInfo() const { return _epInfo; }
    const FunctionBlockInfo* getFunctionBlocks() const { return _fbInfo; }
    uint8_t getFunctionBlockCount() const { return _fbCount; }
    const GroupTerminalBlock* getGTBlocks() const { return _gtb; }
    uint8_t getGTBlockCount() const { return _gtbCount; }
    const char* getEndpointName() const { return _epName; }
    const char* getProductInstanceId() const { return _prodId; }

protected:
    void _processConfig(const usb_config_desc_t* config_desc) override;
    void _onDeviceGone() override;

private:
    bool _midi2Active;
    usb_transfer_t* _outTransfer = nullptr;

    // Protocol Negotiation state machine
    enum NegotiationState : uint8_t {
        NegIdle,              // Not started or MIDI 1.0 mode
        NegAwaitEndpointInfo, // Sent Endpoint Discovery, awaiting response
        NegAwaitStreamConfig, // Sent Stream Config Request, awaiting notification
        NegAwaitFBInfo,       // Sent Function Block Discovery, awaiting responses
        NegDone,              // Negotiation complete
    };
    NegotiationState _negotiationState = NegIdle;
    unsigned long _negTimeout = 0;

    EndpointInfo _epInfo = {};
    FunctionBlockInfo _fbInfo[MAX_FUNCTION_BLOCKS] = {};
    uint8_t _fbCount = 0;
    uint8_t _fbExpected = 0;

    GroupTerminalBlock _gtb[MAX_GTB] = {};
    uint8_t _gtbCount = 0;
    uint8_t _claimedIfaceNumber = 0;

    char _epName[MAX_NAME_LEN] = {};
    uint8_t _epNameLen = 0;
    char _prodId[MAX_NAME_LEN] = {};
    uint8_t _prodIdLen = 0;

    struct AltCandidate {
        uint8_t  ifaceNumber;
        uint8_t  altSetting;
        bool     isMIDI2;        // bcdMSC == 0x0200
        uint8_t  epInAddress;    // IN endpoint (0 = none found)
        uint16_t epInMaxPacket;
        uint8_t  epInInterval;
        uint8_t  epOutAddress;   // OUT endpoint (0 = none found)
        uint16_t epOutMaxPacket;
    };

    static bool _findBestAlt(const uint8_t* desc, uint16_t totalLen,
                             AltCandidate& best);
    bool _claimAndSetup(const AltCandidate& cand);

    // Protocol Negotiation helpers
    void _startNegotiation();
    void _processStreamMessage(const uint32_t* words);
    void _sendEndpointDiscovery();
    void _sendStreamConfigRequest(uint8_t protocol);
    void _sendFunctionBlockDiscovery();

    // Group Terminal Block reading (USB class-specific GET_DESCRIPTOR)
    void _readGTBDescriptors();
    static void _onGTBResponse(usb_transfer_t* transfer);
    void _parseGTBResponse(const uint8_t* data, uint16_t len);

    // Text accumulation from multi-packet stream messages
    void _appendStreamText(char* dest, uint8_t& destLen, uint8_t maxLen,
                           const uint32_t* words, uint8_t form);

    // Bulk IN callback for UMP mode — reads raw uint32_t words, no CIN.
    static void _onReceiveUMP(usb_transfer_t* transfer);

    // Bulk OUT completion callback.
    static void _onSendComplete(usb_transfer_t* transfer);
};

#endif // USB_MIDI2_CONNECTION_H
