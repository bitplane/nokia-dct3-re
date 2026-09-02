#!/usr/bin/env python3
"""Attach a standalone loopback telephone endpoint to a running DCT3 MAME."""

from __future__ import annotations

import argparse
import asyncio
from dataclasses import dataclass
import json
import sys
import time
from typing import Any


FRAME_HEX_LENGTH = 66
PROTOCOL_VERSION = 1


@dataclass
class BridgeStats:
    calls: int = 0
    uplink_frames: int = 0
    downlink_frames: int = 0
    bad_frames: int = 0
    sequence_gaps: int = 0


class LoopbackProtocol:
    """Pure state machine for the versioned MAME call/media messages."""

    def __init__(self) -> None:
        self.epoch: int | None = None
        self.request_id: int | None = None
        self.digits = ""
        self.phase = "idle"
        self.next_uplink_sequence: int | None = None
        self.next_downlink_sequence = 0
        self.stats = BridgeStats()
        self.direction = "outgoing"

    @property
    def active(self) -> bool:
        return self.request_id is not None and self.phase != "ended"

    def termination(self, cause: int = 16) -> dict[str, Any] | None:
        if not self.active or self.epoch is None or self.request_id is None:
            return None
        return {
            "type": f"{self.direction}_call_terminate",
            "epoch": self.epoch,
            "request_id": self.request_id,
            "cause": cause,
        }

    def handle(self, message: dict[str, Any]) -> list[dict[str, Any]]:
        kind = message.get("type")
        if kind == "call_adapter_ready":
            if message.get("protocol_version") != PROTOCOL_VERSION:
                return []
            epoch = message.get("epoch")
            if isinstance(epoch, int) and not isinstance(epoch, bool):
                self.epoch = epoch
            return []
        if kind == "outgoing_call":
            return self._request(message)
        if kind == "outgoing_call_state":
            self._state(message)
            return []
        if kind == "outgoing_call_media_uplink":
            downlink = self._media(message)
            return [] if downlink is None else [downlink]
        if kind == "incoming_call_state":
            self._state(message)
            return []
        if kind == "incoming_call_media_uplink":
            downlink = self._media(message)
            return [] if downlink is None else [downlink]
        return []

    def incoming_request(
            self, caller: str, request_id: int = 1
    ) -> dict[str, Any] | None:
        if self.epoch is None:
            return None
        self.direction = "incoming"
        self.request_id = request_id
        self.digits = caller
        self.phase = "requested"
        self.next_uplink_sequence = None
        self.next_downlink_sequence = 0
        self.stats.calls += 1
        return {
            "type": "incoming_call",
            "epoch": self.epoch,
            "request_id": request_id,
            "caller": caller,
        }

    @staticmethod
    def _identity(message: dict[str, Any]) -> tuple[int, int] | None:
        epoch = message.get("epoch")
        request_id = message.get("request_id")
        if (not isinstance(epoch, int) or isinstance(epoch, bool) or
                not isinstance(request_id, int) or
                isinstance(request_id, bool) or request_id <= 0):
            return None
        return epoch, request_id

    def _matches(self, message: dict[str, Any]) -> bool:
        identity = self._identity(message)
        return identity == (self.epoch, self.request_id)

    def _request(self, message: dict[str, Any]) -> list[dict[str, Any]]:
        identity = self._identity(message)
        digits = message.get("digits")
        if identity is None or not isinstance(digits, str):
            return []
        is_new_call = (self.request_id is None or
                       identity[1] != self.request_id or
                       self.phase == "ended")
        self.epoch, self.request_id = identity
        self.digits = digits
        self.phase = "requested"
        self.next_uplink_sequence = None
        self.next_downlink_sequence = 0
        if is_new_call:
            self.stats.calls += 1
        return [{
            "type": "outgoing_call_decision",
            "epoch": self.epoch,
            "request_id": self.request_id,
            "decision": "connect",
        }]

    def _state(self, message: dict[str, Any]) -> None:
        if not self._matches(message):
            return
        phase = message.get("phase")
        if phase not in ("queued", "paging", "alerting", "connected", "ended"):
            return
        self.phase = phase
        if phase == "connected":
            sequence = message.get("media_downlink_sequence")
            if isinstance(sequence, int) and not isinstance(sequence, bool):
                self.next_downlink_sequence = sequence
        elif phase == "ended":
            self.next_uplink_sequence = None

    def _media(self, message: dict[str, Any]) -> dict[str, Any] | None:
        if self.phase != "connected" or not self._matches(message):
            return None
        sequence = message.get("sequence")
        time_us = message.get("emulation_time_us")
        frame = message.get("frame")
        good = message.get("good")
        if (not isinstance(sequence, int) or isinstance(sequence, bool) or
                not isinstance(time_us, int) or isinstance(time_us, bool) or
                not isinstance(frame, str) or len(frame) != FRAME_HEX_LENGTH):
            return None
        try:
            bytes.fromhex(frame)
        except ValueError:
            return None
        if self.next_uplink_sequence is not None and sequence != self.next_uplink_sequence:
            self.stats.sequence_gaps += 1
        self.next_uplink_sequence = sequence + 1
        self.stats.uplink_frames += 1
        if good is not True:
            self.stats.bad_frames += 1
            return None
        downlink = {
            "type": f"{self.direction}_call_media_downlink",
            "epoch": self.epoch,
            "request_id": self.request_id,
            "sequence": self.next_downlink_sequence,
            "source_time_us": time_us,
            "frame": frame,
        }
        self.next_downlink_sequence += 1
        self.stats.downlink_frames += 1
        return downlink


def status(protocol: LoopbackProtocol) -> str:
    stats = protocol.stats
    return (
        f"phase={protocol.phase} calls={stats.calls} "
        f"uplink={stats.uplink_frames} downlink={stats.downlink_frames} "
        f"bad={stats.bad_frames} gaps={stats.sequence_gaps}"
    )


async def connected_session(args: argparse.Namespace, websocket: Any,
                            protocol: LoopbackProtocol) -> bool:
    hangup_deadline: float | None = None
    while True:
        timeout = None
        if hangup_deadline is not None:
            timeout = max(0.0, hangup_deadline - time.monotonic())
        try:
            payload = await asyncio.wait_for(websocket.recv(), timeout)
        except asyncio.TimeoutError:
            termination = protocol.termination(args.cause)
            if termination is not None:
                await websocket.send(json.dumps(termination))
                print(f"remote hang-up requested: {status(protocol)}", flush=True)
            hangup_deadline = None
            continue
        message = json.loads(payload)
        if (args.incoming_caller is not None and
                message.get("type") == "call_adapter_ready" and
                protocol.request_id is None):
            protocol.handle(message)
            request = protocol.incoming_request(args.incoming_caller)
            if request is not None:
                await websocket.send(json.dumps(request))
                print(
                    f"incoming call requested from {args.incoming_caller}",
                    flush=True,
                )
            continue
        old_phase = protocol.phase
        for reply in protocol.handle(message):
            await websocket.send(json.dumps(reply))
        if protocol.phase != old_phase:
            print(f"call {protocol.request_id} {protocol.phase}: {status(protocol)}",
                  flush=True)
            if protocol.phase == "connected" and args.hangup_after is not None:
                hangup_deadline = time.monotonic() + args.hangup_after
            if protocol.phase == "ended":
                if protocol.stats.downlink_frames < args.require_frames:
                    raise RuntimeError(
                        "call ended before the required media crossed the "
                        f"bridge ({protocol.stats.downlink_frames} < "
                        f"{args.require_frames})")
                return args.once
        if (protocol.stats.uplink_frames and
                protocol.stats.uplink_frames % args.report_every == 0):
            print(status(protocol), flush=True)


async def run(args: argparse.Namespace) -> None:
    try:
        import websockets
    except ImportError as error:
        raise RuntimeError(
            "the websockets package is required; run `make venv`") from error

    protocol = LoopbackProtocol()
    while True:
        try:
            async with websockets.connect(
                    args.url, max_size=4096, ping_interval=None) as websocket:
                print(f"connected to {args.url}", flush=True)
                if await connected_session(args, websocket, protocol):
                    print(f"call complete: {status(protocol)}", flush=True)
                    return
        except (OSError, websockets.exceptions.ConnectionClosed) as error:
            print(f"bridge disconnected ({error}); retrying", file=sys.stderr,
                  flush=True)
            await asyncio.sleep(args.retry_delay)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Answer and GSM-FR-loop an organic DCT3 MAME call")
    parser.add_argument(
        "--url", default="ws://127.0.0.1:18080/nokia/dct3/calls")
    parser.add_argument("--hangup-after", type=float)
    parser.add_argument("--incoming-caller")
    parser.add_argument("--cause", type=int, default=16)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--retry-delay", type=float, default=0.5)
    parser.add_argument("--report-every", type=int, default=250)
    parser.add_argument("--require-frames", type=int, default=0)
    args = parser.parse_args()
    if args.hangup_after is not None and args.hangup_after <= 0:
        parser.error("--hangup-after must be positive")
    if (args.incoming_caller is not None and
            (not args.incoming_caller.isdigit() or
             not 1 <= len(args.incoming_caller) <= 20)):
        parser.error("--incoming-caller must contain 1..20 decimal digits")
    if not 1 <= args.cause <= 0x7f:
        parser.error("--cause must be in the GSM range 1..127")
    if args.report_every <= 0:
        parser.error("--report-every must be positive")
    if args.require_frames < 0:
        parser.error("--require-frames must not be negative")
    try:
        asyncio.run(run(args))
    except (KeyboardInterrupt, RuntimeError) as error:
        if isinstance(error, RuntimeError):
            print(f"FAIL - {error}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
