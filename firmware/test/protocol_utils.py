"""Shared UART-FSCB protocol helpers for the test scripts (#32).

Packet (protocol v3.0, see firmware/docs/PROTOCOL.md):
    [0xAA | Target | Source | TTL | Cmd | Len | Data... | CRC8]

This is the only Python implementation of the packet format; the scripts in
this directory import it instead of carrying their own copy.
"""
import time

HEADER = 0xAA
HOST_ID = 0x00
BROADCAST_ID = 0xFF
DEFAULT_TTL = 16
OVERHEAD = 7  # header, target, source, ttl, cmd, len, crc
MAX_PACKET_LEN = 128
MAX_DATA_LEN = MAX_PACKET_LEN - OVERHEAD

RESP_SENSOR_DATA = 0x82
RESP_CFG_ACK = 0x84
RESP_PD_ACK = 0x86
RESP_CAL_ACK = 0x87
RESP_CAL_DATA = 0x88
RESP_ERROR = 0xEE

# Keep in sync with firmware/src/error_codes.h (#52)
ERROR_NAMES = {
    0x00: "OK",
    0x01: "UNKNOWN_CMD",
    0x02: "BAD_LENGTH",
    0x03: "BAD_CHANNEL",
    0x04: "BAD_VALUE",
    0x10: "CRC",
    0x11: "TIMEOUT",
    0x12: "BUFFER_OVERFLOW",
    0x13: "TTL_EXPIRED",
    0x20: "FLASH_WRITE",
    0x21: "ADC",
    0x22: "PWM",
    0x30: "OVERCURRENT",
    0x31: "UNDERVOLTAGE",
    0x32: "OVERHEAT",
    0x33: "STALL",
    0x40: "CONFIG_INVALID",
    0x41: "CAL_INVALID",
}


def crc8(data):
    """CRC-8, poly 0x07, init 0x00 (check value for b"123456789" is 0xF4)."""
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) if crc & 0x80 else (crc << 1)
            crc &= 0xFF
    return crc


def build_packet(target_id, source_id, cmd, data=b"", ttl=DEFAULT_TTL):
    data = bytes(data)
    if len(data) > MAX_DATA_LEN:
        raise ValueError(f"data too long: {len(data)} > {MAX_DATA_LEN}")
    pkt = bytearray([HEADER, target_id, source_id, ttl, cmd, len(data)]) + data
    pkt.append(crc8(pkt))
    return pkt


def parse_packet(buf):
    """Parse one packet at the start of buf. Returns a dict or None."""
    if len(buf) < OVERHEAD or buf[0] != HEADER:
        return None
    length = buf[5]
    if len(buf) < OVERHEAD + length:
        return None
    if buf[6 + length] != crc8(buf[:6 + length]):
        return None
    return {
        "target": buf[1],
        "source": buf[2],
        "ttl": buf[3],
        "cmd": buf[4],
        "data": bytes(buf[6:6 + length]),
        "size": OVERHEAD + length,
    }


def split_packets(buf):
    """Extract all complete packets from buf.

    Returns (packets, rest): rest is the unconsumed tail (an incomplete
    packet), so a stream reader can keep appending to it.
    """
    packets = []
    buf = bytearray(buf)
    while buf:
        if buf[0] != HEADER:
            del buf[0]
            continue
        if len(buf) < OVERHEAD or len(buf) < OVERHEAD + buf[5]:
            break
        pkt = parse_packet(buf)
        if pkt is None:
            del buf[0]  # bad CRC: resync on the next header
            continue
        packets.append(pkt)
        del buf[:pkt["size"]]
    return packets, buf


def read_packet(ser, want_cmds, timeout=1.0):
    """Read from ser until a packet whose cmd is in want_cmds (or an error
    response) arrives. Returns the packet dict or None on timeout."""
    if isinstance(want_cmds, int):
        want_cmds = (want_cmds,)
    buf = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        chunk = ser.read(max(1, ser.in_waiting))
        if chunk:
            buf += chunk
            packets, buf = split_packets(buf)
            for pkt in packets:
                if pkt["cmd"] in want_cmds or pkt["cmd"] == RESP_ERROR:
                    return pkt
    return None


def error_text(pkt):
    """Human-readable text for a RESP_ERROR packet dict."""
    cmd, code = pkt["data"][0], pkt["data"][1]
    return f"cmd 0x{cmd:02X}: {ERROR_NAMES.get(code, 'unknown')} (0x{code:02X})"
