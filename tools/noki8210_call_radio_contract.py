"""Own NSM-3 traffic/release words for default and configured laboratory cells."""
import re


def channel_patterns(configured_carrier=False):
    # Coherent ARFCN4 SCH supplies the 12/02 parameters and traffic carrier 4.
    # The older default-cell comparison retains its exact, distinct words.
    traffic, release = (
        ('041202000271012fc10000040000000400000000',
         '041202001117001a600000040000001400000001')
        if configured_carrier else
        ('040002000271012fc10000010000000400000000',
         '040000001117001a600000040000001400000001'))
    return (re.compile(r'TX packet type=02 payload=20 .*data=' + traffic),
            re.compile(r'TX packet type=02 payload=20 .*data=' + release))
