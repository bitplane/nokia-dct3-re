"""Verify native NSE-1 ROM4 compatibility execution of the own NSE-2 verifier."""
import hashlib
import re

from tools.nse2_boot_check import STATE, verify_nv, verify_record, verify_upload

NOMINAL_FRAME = '7da9bef12e0bf163dfdf8770e215957b58b4911baa9ba987c05ddf8fadfeae8a'


def verify_nominal_voltage(text, pixels, *, physical_menu=False, empty_socket_frame=True):
    caps = [line for line in text.splitlines() if 'nse2_startup_message_cap:' in line]
    # This gate proves the initial <3 s readiness chain, not completeness
    # of the same mailbox's later keypad/application traffic.
    early_or_unknown_cap = any((match := re.search(r' t=([0-9.]+)', line)) is None
                              or float(match[1]) <= 3 for line in caps)
    if 'nse2_average_count_cap:' in text or early_or_unknown_cap:
        raise ValueError('NSE-2 nominal evidence capped')
    rows = re.findall(r'nse2_average_count: remaining=([0-9a-f]{2}) first=([0-9a-f]{4}) '
                      r'second=([0-9a-f]{4}) pc=002645d0 t=([0-9.]+) observer_writes=0', text)
    if [(int(a, 16), int(b, 16), int(c, 16)) for a, b, c, _ in rows[:10]] != [
            (remaining, 554 * 1500 // 232, 554 * 1500 // 232) for remaining in range(9, -1, -1)]:
        raise ValueError('NSE-2 valid nominal samples did not drain the averaging counter')
    times = [float(row[3]) for row in rows[:10]]
    if times != sorted(set(times)) or not 0 < times[0] < times[-1] < 3:
        raise ValueError('NSE-2 nominal averaging order differs')
    if not re.search(r'ccont_input: adc_select=2 raw=22a ctrl=28 t=', text):
        raise ValueError('NSE-2 nominal raw ADC2 input unobserved')
    reports = re.findall(r'nse2_startup_message: address=0010c662 value=0014 pc=0025c584 '
                         r't=([0-9.]+) observer_writes=0', text)
    unmask = re.findall(r'nse2_key_mask: value=20 pc=0025c8c0 t=([0-9.]+) observer_writes=0', text)
    if (len(reports) != 1 or len(unmask) != 1
            or not times[-1] < float(reports[0]) < float(unmask[0]) < 3):
        raise ValueError('NSE-2 organic report14 and keyboard unmask missing or unordered')
    states = re.findall(r'nse2_ui_state: t=([0-9.]+) state_10c664=0004 '
                        r'flags_10c5c9=0f phase_10ffac=03 observer_writes=0', text)
    if len(states) < 7 or float(states[-1]) < 8:
        raise ValueError('NSE-2 completed startup state missing')
    result = dict(nominal_adc2=554, averaging_completed=True, startup_report14_received=True,
                  startup_message_coverage_seconds=3, later_mailbox_trace_capped=bool(caps),
                  keyboard_unmasked=True, measured_board_calibration=False)
    if physical_menu:
        contacts = re.findall(r'nse2_menu_contact: index=([12]) down=([01]) t=([0-9.]+) physical_only=1', text)
        if [(a, b) for a, b, _ in contacts] != [('1', '1'), ('1', '0'), ('2', '1'), ('2', '0')]:
            raise ValueError('NSE-2 nominal physical Menu contacts missing')
        decoded = re.findall(r'nse2_key_decode: raw=11 key=19 pc=00299044 t=([0-9.]+) observer_writes=0', text)
        for i in (0, 2):
            start, end = float(contacts[i][2]), float(contacts[i + 1][2])
            if not start < end or not any(start <= float(time) <= end for time in decoded):
                raise ValueError('NSE-2 physical Menu contact did not reach own decoder')
        result['physical_key_delivery'] = True
    elif empty_socket_frame and hashlib.sha256(pixels).hexdigest() != NOMINAL_FRAME:
        raise ValueError('NSE-2 reviewed Insert SIM card frame differs')
    elif empty_socket_frame:
        result['graphical_boot'] = True
    return result


def verify_menu_probe(text):
    if 'nse2_startup_message_cap:' in text:
        raise ValueError('NSE-2 startup message evidence capped')
    if not re.search(r'nse2_report14_sample: valid=0 first=19d6 second=19d6 remaining=0a '
                     r'pc=002645b2 t=[0-9.]+ observer_writes=0', text):
        raise ValueError('NSE-2 open-pin invalid sample pair unobserved')
    contacts = re.findall(r'nse2_menu_contact: index=([12]) down=([01]) t=([0-9.]+) physical_only=1', text)
    if [(row[0], row[1]) for row in contacts] != [('1', '1'), ('1', '0'), ('2', '1'), ('2', '0')]:
        raise ValueError('NSE-2 physical Menu contact sequence missing')
    for row, expected in zip(contacts, (3, 3.2, 5, 5.2)):
        if not expected <= float(row[2]) < expected + 0.04:
            raise ValueError('NSE-2 physical Menu contact timing differs')
    columns = re.findall(r'kbgpio: columns=(1f|1d)->(1f|1d) changed=00 row=e0 dir=1f mask=3f t=([0-9.]+)', text)
    columns = [row for row in columns if float(row[2]) >= 3]
    if [(row[0], row[1]) for row in columns] != [('1f', '1d'), ('1d', '1f')] * 2:
        raise ValueError('NSE-2 masked physical column transitions missing')
    if any(abs(float(column[2]) - float(contact[2])) > 0.04
           for column, contact in zip(columns, contacts)):
        raise ValueError('NSE-2 column transitions do not track the contacts')
    masks = re.findall(r'nse2_key_mask: value=([0-9a-f]{2}) pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0', text)
    if not masks or masks[-1][:2] != ('3f', '00299066') or float(masks[-1][2]) >= 3:
        raise ValueError('NSE-2 firmware-owned keyboard mask changed')
    states = re.findall(r'nse2_ui_state: t=([0-9.]+) state_10c664=0010 flags_10c5c9=0e phase_10ffac=06 observer_writes=0', text)
    times = [float(value) for value in states]
    if len(times) < 7 or times[-1] < 8 or times != sorted(set(times)):
        raise ValueError('NSE-2 incomplete startup predicate missing')
    reports = re.findall(r'nse2_startup_message: address=0010c662 value=([0-9a-f]{4}) pc=[0-9a-f]{8} t=[0-9.]+ observer_writes=0', text)
    if not {'0015', '0016', '0017'} <= set(reports) or '0014' in reports:
        raise ValueError('NSE-2 observed startup report set changed')
    if 'nse2_key_decode:' in text:
        raise ValueError('NSE-2 probe now reaches decoding; re-evaluate the frontier')
    return dict(physical_contacts_observed=True, gpio_columns_respond=True,
                columns_masked_by_firmware=True, physical_key_delivery=False,
                startup_state='0x10', report_bits='0x0e', phase=6,
                unobserved_startup_report='0x14')


def verify_native(text, payload, pixels, size, *, record_checksum=False, minimum_time=8):
    if text.count('Soft reset\n') != 1 or 'dsp_hle:' in text:
        raise ValueError('NSE-2 native reset/HLE boundary differs')
    staged = verify_upload(text, payload, native_response=True)
    images = re.findall(r'nse2_native_verifier_image: words=186 body=([0-9a-f]+) observer_writes=0', text)
    if len(images) != 1 or images[0] != payload[:372].hex():
        raise ValueError('NSE-2 executed verifier program image differs from own payload')
    identities = re.findall(r'nse2_native_identity: n=\d+ data=0004 mask=ffff pc=0f0f '
                            r't=([0-9.]+) observer_writes=0', text)
    responses = re.findall(r'nse2_startup_poll: n=1 address=00010004 data=0004 mask=ffff '
                           r'pc=002781fa t=([0-9.]+) observer_writes=0', text)
    if len(identities) != 1 or len(responses) != 1 or not 0 < float(identities[0]) < float(responses[0]):
        raise ValueError('NSE-2 own native verifier identity/MCU response missing or unordered')
    ready = re.findall(r'nse2_state_byte: address=0010b9f4 value=01 pc=002787e2 '
                       r't=([0-9.]+) observer_writes=0', text)
    if len(ready) != 1 or not float(responses[0]) < float(ready[0]) < 1:
        raise ValueError('NSE-2 organic native readiness state missing')
    if re.search(r'nse2_state_byte: address=0010b9f[45].*pc=002782(?:26|d6|d8)', text):
        raise ValueError('NSE-2 native loader took the silent-profile failure tail')
    samples = re.findall(r'nse2_native_state: t=([0-9.]+) pc=([0-9a-f]{4}) '
                         r'illegal=([01]) idle=([01]) observer_writes=0', text)
    if len(samples) < 8 or float(samples[-1][0]) < minimum_time or any(row[2] != '0' for row in samples):
        raise ValueError('NSE-2 native execution horizon missing or illegal')
    times = [float(row[0]) for row in samples]
    if times != sorted(set(times)):
        raise ValueError('NSE-2 native sample order differs')
    mcu = STATE.findall(text)
    if (len(mcu) < 8 or float(mcu[-1][0]) < minimum_time
            or any(not (0x200040 <= int(row[1], 16) < 0x390000
                        and 0x100000 <= int(row[3], 16) < 0x110000) for row in mcu)):
        raise ValueError('NSE-2 own MCU execution horizon missing')
    mcu_times = [float(row[0]) for row in mcu]
    if mcu_times != sorted(set(mcu_times)):
        raise ValueError('NSE-2 MCU sample order differs')
    if record_checksum:
        verify_nv(text, checksum_only=True)
        verify_record(text)
    else:
        verify_nv(text)
    if size != (84, 48) or len(pixels) != 84 * 48:
        raise ValueError('NSE-2 frame geometry differs')
    return dict(native_verifier_executed=True, startup_identity=4,
                rounded_upload_words=staged['rounded_upload_words'],
                organic_ready_state=True, native_samples=len(samples),
                fitted_mask_proved=False, usable_boot=False, physical_key_delivery=False,
                graphical_boot=False, frame_blank=all(value == 255 for value in pixels))
