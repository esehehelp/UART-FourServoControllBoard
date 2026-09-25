"""Offline tests for protocol_utils (no board needed): pytest firmware/test"""
from protocol_utils import (OVERHEAD, RESP_ERROR, build_packet, crc8,
                            parse_packet, split_packets)


def test_crc8_check_value():
    assert crc8(b"123456789") == 0xF4
    assert crc8(b"") == 0


def test_build_packet_layout():
    pkt = build_packet(0x01, 0x00, 0x30, [1, 128])
    assert list(pkt[:6]) == [0xAA, 0x01, 0x00, 16, 0x30, 2]
    assert len(pkt) == OVERHEAD + 2
    assert pkt[-1] == crc8(pkt[:-1])


def test_parse_roundtrip():
    pkt = parse_packet(build_packet(0x02, 0x00, 0x02, [0], ttl=3))
    assert pkt["target"] == 0x02 and pkt["ttl"] == 3 and pkt["cmd"] == 0x02
    assert pkt["data"] == b"\x00"


def test_parse_rejects_bad_crc():
    pkt = build_packet(0x01, 0x00, 0x02, [0])
    pkt[-1] ^= 0xFF
    assert parse_packet(pkt) is None


def test_split_packets_stream():
    a = build_packet(0x00, 0x01, 0x82, bytes(15))
    b = build_packet(0x00, 0x01, RESP_ERROR, [0x01, 0x04])
    stream = b"\x00\x13" + a + b + b[:3]  # garbage, two packets, partial third
    packets, rest = split_packets(stream)
    assert [p["cmd"] for p in packets] == [0x82, RESP_ERROR]
    assert bytes(rest) == bytes(b[:3])
