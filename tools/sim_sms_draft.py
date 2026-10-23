"""Shared serialized-card assertion for the physical unsent A draft fixture."""
from tools.make_sim_card_profile import ADN_SIZE, LOCI_SIZE, KC_SIZE, BCCH_SIZE, SMS_RECORD_SIZE, SMS_RECORDS
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH

SMS_OFFSET = ADN_SIZE + LOCI_SIZE + KC_SIZE + BCCH_SIZE
DRAFT = bytes.fromhex('070281f001ff008100000141').ljust(SMS_RECORD_SIZE,b'\xff')


def check_card(card):
    empty = b'\0' + b'\xff' * (SMS_RECORD_SIZE-1)
    if (len(card) != CURRENT_NVRAM_LENGTH or card[:ADN_SIZE] != b'\xff'*ADN_SIZE
            or card[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE] != DRAFT
            or card[SMS_OFFSET+SMS_RECORD_SIZE:SMS_OFFSET+SMS_RECORDS*SMS_RECORD_SIZE] != empty*(SMS_RECORDS-1)):
        raise ValueError('exact unsent A record/empty remainder or ADN differs')


def deleted_card(saved):
    check_card(saved)
    result=bytearray(saved)
    result[SMS_OFFSET]=0
    return bytes(result)


def check_deleted_card(card):
    if len(card)!=CURRENT_NVRAM_LENGTH or card[SMS_OFFSET]!=0:
        raise ValueError('erased A draft must retain a free status byte')
    restored=bytearray(card)
    restored[SMS_OFFSET]=7
    check_card(restored)
