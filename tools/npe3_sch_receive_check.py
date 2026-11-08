"""Qualify a native laboratory SCH result, not physical RF or camping."""
import re

from tools.gsm_sch_signal import HYPERFRAME, sch_uncoded_bits
from tools.rom6_adc_measure_check import check_rx_publications
from tools.dsp_rom6_candidate_check import PINNED, digest


CRC = re.compile(r'npe3_sch_crc: sequence=(\d+) t=([\d.]+) status=0000 remainder=03ff words=([0-9a-f]{4}),([0-9a-f]{4}),([0-9a-f]{4}) read_owner=004f5d class=0040 observer_writes=0')
OBSERVATION = re.compile(r'npe3_sch_crc: sequence=(\d+) t=([\d.]+) status=([0-9a-f]{4}) remainder=([0-9a-f]{4}) words=([0-9a-f]{4}),([0-9a-f]{4}),([0-9a-f]{4}) read_owner=004f5d class=0040 observer_writes=0')
DUE = re.compile(r'npe3_capture_due: target=([0-9a-f]{4}) counter=([0-9a-f]{4}) t=([\d.]+) physical_claim=0')
# Derived from the audited capture; zero initialization outside B000..FFFF
# is not a claimed hardware reset image.
DROM_SHA256 = '98bab4bfc75e1dc76bfbd9c9b4d46e8c09198a0b7f99e681651cd505499aadaa'


def check_compare_edges(text):
    due = list(DUE.finditer(text))
    if not due or len(due) != text.count('npe3_capture_due:') or any(row[1] != row[2] for row in due):
        raise ValueError('capture candidate missed its declared compare edge')
    return len(due)


def decode_information(words):
    bits = [(words[index // 16] >> (index % 16)) & 1 for index in range(25)]
    def field(positions):
        value = 0
        for index in positions:
            value = value * 2 + bits[index]
        return value
    # Independently indexed GSM04.08 fields, not the source encoder's packing.
    bsic = field((7, 6, 5, 4, 3, 2))
    t1 = field((1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 23))
    t2 = field((22, 21, 20, 19, 18))
    t3prime = field((17, 16, 24))
    if t2 >= 26 or t3prime >= 5:
        raise ValueError('decoded SCH contains reserved timing fields')
    t3 = 10 * t3prime + 1
    frame = 26 * 51 * t1 + 51 * ((t3 - t2) % 26) + t3
    return frame, bsic, bits


def check_program(program, data, resident, drom):
    if (len(program) != 131072 or len(data) != 131072
            or digest(resident) != PINNED['working/rom6_program.be16']
            or digest(drom) != DROM_SHA256
            or program[0x2000 * 2:] != resident[0x2000 * 2:]
            or data[0xb000 * 2:] != drom[0xb000 * 2:]):
        raise ValueError('native SCH resident program or DROM differs from acquired image')


def check(text, source, program, data, resident, drom, *, allow_reacquisition=False):
    check_program(program, data, resident, drom)
    observations=list(OBSERVATION.finditer(text))
    rows = list(CRC.finditer(text))
    if (not rows or len(observations)!=text.count('npe3_sch_crc:') or
            not allow_reacquisition and len(observations)!=1 or
            observations[0].groups()[2:4]!=('0000','03ff') or
            any(int(row[1])!=index for index,row in enumerate(observations,1))):
        raise ValueError('missing complete ordered native SCH CRC observations')
    all_packets=[packet for packet in check_rx_publications(text)['packets']
                 if packet['type']==0x80 and packet['payload'].startswith('4000')]
    if len(all_packets)!=len(observations):
        raise ValueError('native SCH observation/publication counts differ')
    previous=-1
    for row,packet in zip(observations,all_packets):
        _,stamp,status,remainder,*values=row.groups()
        body=bytes.fromhex(packet['payload'])
        if ((status,remainder)!=('0000','03ff') and
                (not allow_reacquisition or status!='0001' or remainder=='03ff') or
                len(body)!=14 or body[2]!=int(status,16) or
                body[10:]!=b''.join(int(value,16).to_bytes(2,'little') for value in values[:2]) or
                not previous<float(stamp)<=float(packet['time'])):
            raise ValueError('native SCH attempt status,words or publication differs')
        previous=float(packet['time'])
    packets=[packet for packet in all_packets if packet['payload'].startswith('400000')]
    if len(packets)!=len(rows):
        raise ValueError('native successful class40 publication count differs')
    previous=-1
    for row,packet in zip(rows,packets):
        stamp,*values=row.groups()[1:]
        words=[int(value,16) for value in values]
        _,bsic,bits=decode_information(words)
        decoded=sum(word<<(16*index) for index,word in enumerate(words))
        expected=sum(bit<<index for index,bit in enumerate(sch_uncoded_bits(bits)[:35]))
        body=bytes.fromhex(packet['payload'])
        if (decoded&((1<<35)-1)!=expected or bsic!=42 or
                len(body)!=14 or body[10:]!=b''.join(word.to_bytes(2,'little') for word in words[:2]) or
                not previous<float(stamp)<=float(packet['time'])):
            raise ValueError('native SCH parity, BSIC, ordering or publication differs')
        previous=float(packet['time'])
    stamp, *values = rows[0].groups()[1:]
    words = [int(value, 16) for value in values]
    frame, bsic, _ = decode_information(words)
    first, last = source['source_first_tick'], source['source_last_tick']
    frames = [at for at in range(first // 60000, last // 60000 + 1)
              if at % HYPERFRAME == frame and first <= at * 60000 and last >= at * 60000 + 7104]
    if len(frames) != 1:
        raise ValueError('decoded frame is not the complete captured source SCH')
    edges = check_compare_edges(text)
    return dict(native_sch_crc_qualified=True, decoded_frame=frame, decoded_bsic=bsic,
                native_sch_results_checked=len(rows), initial_capture_only=True,
                native_sch_failed_attempts_checked=len(observations)-len(rows),
                checked_information_bits=25, checked_parity_bits=10,
                complete_source_burst_observed=True, compare_edges_checked=edges,
                physical_capture_qualified=False, fitted_clock_qualified=False,
                camping_qualified=False, registration_qualified=False)
