-- PCS delivery releases after the ordinary SMS save point on both profiles.
local source = debug.getinfo(1, 'S').source:sub(2)
local pin = os.getenv('NOKIA_DCT3_8890_PIN_ENTRY') == '1'
_G.noki8890_sms_state_save_time = pin and 25 or 20
_G.noki8890_sms_read_start = pin and 27 or 22
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_state_sms.lua')
