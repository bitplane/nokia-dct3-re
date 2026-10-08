"""Shared observed DSP/radio rail-off contract, not complete clock-gating proof."""
import re


POWERED_ENDPOINT_ACTIVITY = re.compile(
    r'dspif_transport: (?:RX enqueue|FIQ0 notify|peer RAM W)|'
    r'rom4_(?:timing_port|port_write):|staged_dsp: publication|'
    r'radio_peer: LAPDm|dsp_hle: speech')


def require_endpoint_silence(text, message):
    """Reject known powered-endpoint events within an already delimited window.

    Product checkers own rail ordering, RTC encoding, minimum observation time,
    boot/registration contracts and restored-timeline selection.
    """
    if POWERED_ENDPOINT_ACTIVITY.search(text):
        raise ValueError(message)
