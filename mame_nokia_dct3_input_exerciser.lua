local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local call_alerting_output =
		machine.devices[":gsm_session"]:output("nokia_gsm_call_alerting")
local call_active_output =
		machine.devices[":gsm_session"]:output("nokia_gsm_call_active")
local call_release_waiting_output =
		machine.devices[":gsm_session"]:output(
				"nokia_gsm_call_release_waiting_handset")
local output_dir = os.getenv("NOKIA_DCT3_SNAPSHOT_DIR") or ".."
local boot_summary_path = os.getenv("NOKIA_DCT3_BOOT_SUMMARY")
local ram_dump_path = os.getenv("NOKIA_DCT3_RAM_DUMP")
local ram_dump_base = tonumber(os.getenv("NOKIA_DCT3_RAM_DUMP_BASE") or "0") or 0
local ram_dump_size = tonumber(os.getenv("NOKIA_DCT3_RAM_DUMP_SIZE") or "65536") or 65536
local ram_dump_at = tonumber(os.getenv("NOKIA_DCT3_RAM_DUMP_AT") or "5") or 5
local ram_dumped = false
local quiet = os.getenv("NOKIA_DCT3_LUA_QUIET") == "1"
local bios = machine.options.entries.bios:value()
local v501 = bios == "501"
local is_3410 = machine.system.name == "noki3410"
local lcd_controller_width = is_3410 and 102 or 84
local lcd_controller_banks = is_3410 and 9 or 6
local lcd_visible_width = is_3410 and 96 or 84
local lcd_visible_height = is_3410 and 65 or 48

local function info(message)
	if not quiet then emu.print_info(message) end
end

local frames = 0
local lcd_data_writes = 0
local lcd_cmd_writes = 0
local nonzero_lcd_data_writes = 0
local lcd_full_dumps = 0
local lcd_vram = {}
local lcd_mode = 0x04
local lcd_control = 0
local lcd_x = 0
local lcd_y = 0
local pending_lcd = {}
local lcd_dirty = false
local active_fields = {}
-- Address-space taps must outlive the script chunk; otherwise their Lua
-- callbacks can be collected while the CPU still holds the native tap.
nokia_dct3_oracle_taps = {}
local taps = nokia_dct3_oracle_taps
local startup_ready_time = nil
local post_key_active = nil
local charger_pulse_at
local irq_overlap_at
local irq_mask_fixture_at
local fiq8_fixture_at
local buzzer_fixture_at
local vibrator_fixture_at
local rtc_fixture_at
local mad2_reset_fixture_at
local mad2_watchdog_fixture_at
local ccont_watchdog_fixture_at
local ccont_mask_fixture_at
local mad2_sleep_fixture_at
local eeprom_fixture_at

local structural = {
	gensio_controls = {}, ccont_commands = {}, startup_modes = {},
	ccont_bytes = 0, ccont_reads = 0, eeprom_starts = 0,
	eeprom_signal_writes = 0, irq_seen = 0, fiq_seen = 0, soft_resets = 0,
	final_irq_status = 0,
	final_fiq_status = 0, state_roundtrip = "not-run",
	final_pc = 0,
	final_current_task = 0, final_startup_mode = 0, final_startup_event = 0,
	final_startup_flags = 0, final_contact_status = 0,
	final_no_sim = 0, final_sim_enable = 0,
	upper_ram_reads = 0, upper_ram_writes = 0, upper_ram_highest = 0,
}

for i = 0, (lcd_controller_width * lcd_controller_banks) - 1 do lcd_vram[i] = 0 end

local function env_number(name, fallback)
	local value = tonumber(os.getenv(name) or "")
	return value or fallback
end

charger_pulse_at = env_number("NOKIA_DCT3_CCONT_CHARGER_PULSE_AT", -1)
local charger_pulse_duration = env_number("NOKIA_DCT3_CCONT_CHARGER_PULSE_DURATION", 0.05)
local charger_initial = env_number("NOKIA_DCT3_CCONT_CHARGER_INITIAL", 0) ~= 0
irq_overlap_at = env_number("NOKIA_DCT3_MAD2_IRQ_OVERLAP_AT", -1)
irq_mask_fixture_at = env_number("NOKIA_DCT3_MAD2_IRQ_MASK_FIXTURE_AT", -1)
fiq8_fixture_at = env_number("NOKIA_DCT3_MAD2_FIQ8_FIXTURE_AT", -1)
buzzer_fixture_at = env_number("NOKIA_DCT3_BUZZER_FIXTURE_AT", -1)
vibrator_fixture_at = env_number("NOKIA_DCT3_VIBRATOR_FIXTURE_AT", -1)
rtc_fixture_at = env_number("NOKIA_DCT3_CCONT_RTC_FIXTURE_AT", -1)
mad2_reset_fixture_at = env_number("NOKIA_DCT3_MAD2_RESET_FIXTURE_AT", -1)
mad2_watchdog_fixture_at = env_number("NOKIA_DCT3_MAD2_WATCHDOG_FIXTURE_AT", -1)
ccont_watchdog_fixture_at = env_number("NOKIA_DCT3_CCONT_WATCHDOG_FIXTURE_AT", -1)
ccont_mask_fixture_at = env_number("NOKIA_DCT3_CCONT_MASK_FIXTURE_AT", -1)
mad2_sleep_fixture_at = env_number("NOKIA_DCT3_MAD2_SLEEP_FIXTURE_AT", -1)
local mad2_sleep_fixture_source = os.getenv("NOKIA_DCT3_MAD2_SLEEP_FIXTURE_SOURCE") or "timer1"
eeprom_fixture_at = env_number("NOKIA_DCT3_EEPROM_FIXTURE_AT", -1)
local eeprom_fixture_mode = os.getenv("NOKIA_DCT3_EEPROM_FIXTURE_MODE") or "write"
local state_roundtrip_at = env_number("NOKIA_DCT3_STATE_ROUNDTRIP_AT", -1)
local state_roundtrip_wait_release =
		os.getenv("NOKIA_DCT3_STATE_ROUNDTRIP_WAIT_RELEASE") == "1"
local state_roundtrip_end_delay = env_number(
		"NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS", -1) / 1000
local state_roundtrip_replay = env_number(
		"NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS", 0) / 1000
local state_roundtrip_keys = {}
for name in string.gmatch(
		os.getenv("NOKIA_DCT3_STATE_ROUNDTRIP_KEYS") or "", "[^,%s]+") do
	state_roundtrip_keys[#state_roundtrip_keys + 1] = name
end
local state_roundtrip_key_delay = env_number(
		"NOKIA_DCT3_STATE_ROUNDTRIP_KEY_DELAY_MS", 250) / 1000
local mbus_rx_fixture = tonumber(os.getenv("NOKIA_DCT3_MBUS_RX_FIXTURE") or "")
local mbus_rx_fixture_at = env_number("NOKIA_DCT3_MBUS_RX_FIXTURE_AT_MS", 300) / 1000

local function emulation_seconds()
	return machine.time:as_double()
end

local function write_ram_dump()
	if not ram_dump_path or ram_dumped then return end
	local dump = io.open(ram_dump_path, "wb")
	if not dump then return end
	local bytes = {}
	for address = ram_dump_base, ram_dump_base + ram_dump_size - 1 do
		bytes[#bytes + 1] = string.char(space:read_u8(address))
		if #bytes == 4096 then
			dump:write(table.concat(bytes))
			bytes = {}
		end
	end
	if #bytes > 0 then dump:write(table.concat(bytes)) end
	dump:close()
	ram_dumped = true
end

local function field_by_mask(tag, mask)
	local port = machine.ioport.ports[":" .. tag] or machine.ioport.ports[tag]
	return port and port:field(mask) or nil
end

local is_five_row_product = machine.system.name == "noki3310" or machine.system.name == "noki3330" or is_3410
local key_fields
if is_3410 then
	key_fields = {
		enter = field_by_mask("COL.4", 0x01), up = field_by_mask("COL.3", 0x10),
		down = field_by_mask("COL.3", 0x01), ["0"] = field_by_mask("COL.2", 0x01),
		["1"] = field_by_mask("COL.2", 0x02), ["2"] = field_by_mask("COL.3", 0x02),
		["3"] = field_by_mask("COL.1", 0x10), ["4"] = field_by_mask("COL.4", 0x04),
		["5"] = field_by_mask("COL.3", 0x04), ["6"] = field_by_mask("COL.2", 0x04),
		["7"] = field_by_mask("COL.4", 0x08), ["8"] = field_by_mask("COL.3", 0x08),
		["9"] = field_by_mask("COL.2", 0x08), del = field_by_mask("COL.1", 0x01),
		c = field_by_mask("COL.1", 0x01), minus = field_by_mask("COL.2", 0x10),
		star = field_by_mask("COL.4", 0x10), send = field_by_mask("COL.4", 0x02),
		["end"] = field_by_mask("COL.1", 0x02), power = field_by_mask("PWR", 0x01),
	}
elseif is_five_row_product then
	key_fields = {
		enter = field_by_mask("COL.3", 0x10), up = field_by_mask("COL.1", 0x01),
		down = field_by_mask("COL.1", 0x02), ["0"] = field_by_mask("COL.2", 0x01),
		["1"] = field_by_mask("COL.2", 0x02), ["2"] = field_by_mask("COL.3", 0x02),
		["3"] = field_by_mask("COL.1", 0x10), ["4"] = field_by_mask("COL.4", 0x04),
		["5"] = field_by_mask("COL.3", 0x04), ["6"] = field_by_mask("COL.2", 0x04),
		["7"] = field_by_mask("COL.4", 0x08), ["8"] = field_by_mask("COL.3", 0x08),
		["9"] = field_by_mask("COL.2", 0x08), del = field_by_mask("COL.4", 0x01),
		c = field_by_mask("COL.4", 0x01), minus = field_by_mask("COL.2", 0x10),
		star = field_by_mask("COL.4", 0x10), power = field_by_mask("PWR", 0x01),
	}
else
	key_fields = {
		enter = field_by_mask("COL.1", 0x02), up = field_by_mask("COL.3", 0x02),
		down = field_by_mask("COL.1", 0x04), ["0"] = field_by_mask("COL.2", 0x04),
		["1"] = field_by_mask("COL.1", 0x08), ["2"] = field_by_mask("COL.1", 0x10),
		["3"] = field_by_mask("COL.2", 0x08), ["4"] = field_by_mask("COL.3", 0x08),
		["5"] = field_by_mask("COL.2", 0x10), ["6"] = field_by_mask("COL.4", 0x04),
		["7"] = field_by_mask("COL.4", 0x08), ["8"] = field_by_mask("COL.3", 0x10),
		["9"] = field_by_mask("COL.4", 0x10), del = field_by_mask("COL.2", 0x02),
		c = field_by_mask("COL.2", 0x02), minus = field_by_mask("COL.3", 0x04),
		star = field_by_mask("COL.4", 0x02), power = field_by_mask("PWR", 0x01),
	}
end
key_fields.navi = key_fields.enter
key_fields.select = key_fields.enter
key_fields.left = key_fields.enter
key_fields.soft1 = key_fields.enter
key_fields.clear = key_fields.c
key_fields.back = key_fields.c
key_fields.right = key_fields.c
key_fields.soft2 = key_fields.c
key_fields.hash = key_fields.minus
local charger_field = field_by_mask("CHARGER", 0x01)
local mbus_rx_field = field_by_mask("MBUS_RX", 0xff)
if charger_initial and charger_field then charger_field:set_value(1) end

local function press(name)
	local field = key_fields[name]
	if field and not active_fields[name] then
		field:set_value(1)
		active_fields[name] = field
		info(string.format("input-press:frame=%d name=%s", frames, name))
	end
end

local function release(name)
	local field = active_fields[name]
	if field then
		field:clear_value()
		active_fields[name] = nil
		info(string.format("input-release:frame=%d name=%s", frames, name))
	end
end

local function bus_byte(offset, data, mask)
	local address = offset & ~3
	if mask == 0xff000000 then return address, (data >> 24) & 0xff end
	if mask == 0x00ff0000 then return address + 1, (data >> 16) & 0xff end
	if mask == 0x0000ff00 then return address + 2, (data >> 8) & 0xff end
	if mask == 0x000000ff then return address + 3, data & 0xff end
	return offset, data & 0xff
end

local gensio_control = 0
local ccont_command_phase = true
local genio_signal = 0
local genio_direction = 0
local eeprom_sda = 1
local eeprom_scl = 0

local function record_mmio(address, value)
	local reg = address & 0xff
	if reg == 0x01 and (value & 0x04) ~= 0 then
		structural.soft_resets = structural.soft_resets + 1
	elseif reg == 0x20 or reg == 0x24 then
		if reg == 0x20 then
			genio_signal = value
			structural.eeprom_signal_writes = structural.eeprom_signal_writes + 1
		else
			genio_direction = value
		end
		local next_sda = (genio_direction & 1) ~= 0 and (genio_signal & 1) or 1
		local next_scl = (genio_signal >> 3) & 1
		if eeprom_sda == 1 and next_sda == 0 and next_scl == 1 then
			structural.eeprom_starts = structural.eeprom_starts + 1
		end
		eeprom_sda, eeprom_scl = next_sda, next_scl
	elseif reg == 0x2d then
		gensio_control = value
		structural.gensio_controls[value] = true
		if (value & 0x04) ~= 0 then ccont_command_phase = true end
	elseif reg == 0x2c and (gensio_control & 0x04) ~= 0 then
		structural.ccont_bytes = structural.ccont_bytes + 1
		if ccont_command_phase then structural.ccont_commands[value] = true end
		ccont_command_phase = not ccont_command_phase
	end
end

local function queue_lcd_dump()
	lcd_full_dumps = lcd_full_dumps + 1
	local zero, ff, other = 0, 0, 0
	local snapshot = {}
	for i = 0, (lcd_controller_width * lcd_controller_banks) - 1 do
		local value = lcd_vram[i]
		snapshot[i] = value
		if value == 0 then zero = zero + 1 elseif value == 0xff then ff = ff + 1 else other = other + 1 end
	end
	pending_lcd[#pending_lcd + 1] = {
		seq = lcd_full_dumps, zero = zero, ff = ff, other = other,
		vram = snapshot, control = lcd_control,
	}
end

taps[#taps + 1] = space:install_write_tap(0x20000, 0x200ff, "nokia_dct3_oracle_mmio", function(offset, data, mask)
	local address, value = bus_byte(offset, data, mask)
	record_mmio(address, value)
	local reg = address & 0xff
	if reg == 0x2e then
		lcd_dirty = true
		lcd_data_writes = lcd_data_writes + 1
		if value ~= 0 then nonzero_lcd_data_writes = nonzero_lcd_data_writes + 1 end
		local old_x, old_y = lcd_x, lcd_y
		lcd_vram[(lcd_y * lcd_controller_width) + lcd_x] = value
		if (lcd_mode & 0x02) ~= 0 then
			lcd_y = lcd_y + 1
			if lcd_y >= lcd_controller_banks then lcd_y = 0; lcd_x = (lcd_x + 1) % lcd_controller_width end
		else
			lcd_x = lcd_x + 1
			if lcd_x >= lcd_controller_width then lcd_x = 0; lcd_y = (lcd_y + 1) % lcd_controller_banks end
		end
		if old_x == lcd_controller_width - 1 and old_y == lcd_controller_banks - 1 and lcd_x == 0 and lcd_y == 0 then queue_lcd_dump() end
	elseif reg == 0x6e then
		lcd_dirty = true
		lcd_cmd_writes = lcd_cmd_writes + 1
		if (lcd_mode & 0x01) ~= 0 then
			if (value & 0xf8) == 0x20 then lcd_mode = value & 0x07 end
		elseif (value & 0x80) ~= 0 then lcd_x = (value & 0x7f) % lcd_controller_width
		elseif (value & 0xf0) == 0x40 then lcd_y = (value & 0x0f) % lcd_controller_banks
		elseif (value & 0xf8) == 0x20 then lcd_mode = value & 0x07
		elseif (value & 0xf8) == 0x08 then lcd_control = ((value & 0x04) >> 1) | (value & 0x01) end
	end
	return data
end)

taps[#taps + 1] = space:install_read_tap(0x2006c, 0x2006f, "nokia_dct3_oracle_ccont_read", function(offset, data, mask)
	local address = bus_byte(offset, data, mask)
	if (address & 0xff) == 0x6c then structural.ccont_reads = structural.ccont_reads + 1 end
	return data
end)

-- The 3210 carries a 128 KiB KM68U1000 SRAM. Keep the wider provisional map
-- observable until its address-line mirroring is modeled, so ordinary boot
-- cannot silently depend on storage that does not exist on the board.
taps[#taps + 1] = space:install_read_tap(0x120000, 0x17ffff, "nokia_dct3_upper_ram_read", function(offset, data, mask)
	structural.upper_ram_reads = structural.upper_ram_reads + 1
	if offset > structural.upper_ram_highest then structural.upper_ram_highest = offset end
end)
taps[#taps + 1] = space:install_write_tap(0x120000, 0x17ffff, "nokia_dct3_upper_ram_write", function(offset, data, mask)
	structural.upper_ram_writes = structural.upper_ram_writes + 1
	if offset > structural.upper_ram_highest then structural.upper_ram_highest = offset end
end)

local function write_lcd_dump()
	while #pending_lcd > 0 do
	local pending = table.remove(pending_lcd, 1)
	local filename = string.format("%s/nokia_dct3_lcdmirror_%04d_f%03d_z%03d_ff%03d_o%03d.pgm",
		output_dir, pending.seq, frames, pending.zero, pending.ff, pending.other)
	local f = io.open(filename, "wb")
	if not f then return end
	f:write(string.format("P5\n%d %d\n255\n", lcd_visible_width, lcd_visible_height))
	for y = 0, lcd_visible_height - 1 do
		local row, bit = y >> 3, y & 7
		for x = 0, lcd_visible_width - 1 do
			local on = (pending.vram[(row * lcd_controller_width) + x] >> bit) & 1
			if (pending.control & 1) ~= 0 then on = 1 - on end
			f:write(string.char(on ~= 0 and 0 or 255))
		end
	end
	f:close()
	end
end

local function hex_set(values, width)
	local result = {}
	for value in pairs(values) do result[#result + 1] = string.format("%0" .. width .. "X", value) end
	table.sort(result)
	return table.concat(result, ",")
end

local function write_boot_summary()
	if not boot_summary_path then return end
	local summary = {
		"schema=1", string.format("frames=%d", frames),
		string.format("lcd_data_writes=%d", lcd_data_writes),
		string.format("lcd_command_writes=%d", lcd_cmd_writes),
		string.format("lcd_nonzero_writes=%d", nonzero_lcd_data_writes),
		string.format("lcd_full_dumps=%d", lcd_full_dumps),
		string.format("soft_resets=%d", structural.soft_resets),
		string.format("irq_seen=%02X", structural.irq_seen), string.format("fiq_seen=%02X", structural.fiq_seen),
		string.format("final_irq_status=%02X", structural.final_irq_status),
		string.format("final_fiq_status=%02X", structural.final_fiq_status),
		string.format("state_roundtrip=%s", structural.state_roundtrip),
		string.format("gensio_controls=%s", hex_set(structural.gensio_controls, 2)),
		string.format("ccont_bytes=%d", structural.ccont_bytes), string.format("ccont_reads=%d", structural.ccont_reads),
		string.format("ccont_commands=%s", hex_set(structural.ccont_commands, 2)),
		string.format("eeprom_starts=%d", structural.eeprom_starts),
		string.format("eeprom_signal_writes=%d", structural.eeprom_signal_writes),
		string.format("startup_modes=%s", hex_set(structural.startup_modes, 4)),
		string.format("final_pc=%08X", structural.final_pc),
		string.format("final_current_task=%02X", structural.final_current_task),
		string.format("final_startup_mode=%04X", structural.final_startup_mode),
		string.format("final_startup_event=%04X", structural.final_startup_event),
		string.format("final_startup_flags=%02X", structural.final_startup_flags),
		string.format("final_contact_status=%04X", structural.final_contact_status),
		string.format("final_no_sim=%02X", structural.final_no_sim),
		string.format("final_sim_enable=%02X", structural.final_sim_enable),
		string.format("upper_ram_reads=%d", structural.upper_ram_reads),
		string.format("upper_ram_writes=%d", structural.upper_ram_writes),
		string.format("upper_ram_highest=%06X", structural.upper_ram_highest),
	}
	local temporary = boot_summary_path .. ".tmp"
	local f = io.open(temporary, "w")
	if f then f:write(table.concat(summary, "\n"), "\n"); f:close(); os.rename(temporary, boot_summary_path) end
end

local post_key = os.getenv("NOKIA_DCT3_POST_READY_KEY")
local post_keys = {}
for name in string.gmatch(os.getenv("NOKIA_DCT3_POST_READY_KEYS") or post_key or "", "[^,%s]+") do
	post_keys[#post_keys + 1] = name
end
local post_delay = env_number("NOKIA_DCT3_POST_READY_KEY_DELAY_MS", 250) / 1000
local post_duration = env_number("NOKIA_DCT3_POST_READY_KEY_DURATION_MS", 50) / 1000
local post_gap = env_number("NOKIA_DCT3_POST_READY_KEY_GAP_MS", 100) / 1000
local post_period = env_number("NOKIA_DCT3_POST_READY_KEY_PERIOD_MS", 0) / 1000
local post_capture_delay = env_number("NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS", -1) / 1000
local post_sequence_driven = #post_keys > 0 and post_period == 0

local function update_post_ready_key()
	if #post_keys > 0 and not startup_ready_time and (structural.final_startup_flags & 0x0f) == 0x0f then
		startup_ready_time = emulation_seconds()
	end
	if #post_keys == 0 or not startup_ready_time then return end

	local elapsed = emulation_seconds() - startup_ready_time
	local active = nil
	if elapsed >= post_delay then
		if post_period ~= 0 and #post_keys == 1 then
			if ((elapsed - post_delay) % post_period) < post_duration then active = post_keys[1] end
		else
			local sequence_elapsed = elapsed - post_delay
			local slot_width = post_duration + post_gap
			local slot = math.floor(sequence_elapsed / slot_width) + 1
			if slot <= #post_keys and (sequence_elapsed % slot_width) < post_duration then active = post_keys[slot] end
		end
	end
	if active ~= post_key_active then
		if post_key_active then release(post_key_active) end
		post_key_active = active
		if active then press(active) end
	end
end

local function sample_structural_state()
	structural.final_pc = cpu.state["PC"].value
	structural.irq_seen = structural.irq_seen | space:read_u8(0x20009)
	structural.final_irq_status = space:read_u8(0x20009)
	structural.fiq_seen = structural.fiq_seen | space:read_u8(0x20008)
	structural.final_fiq_status = space:read_u8(0x20008)
	structural.final_current_task = space:read_u8(0x100002)
	structural.final_startup_mode = space:read_u16(v501 and 0x11224c or 0x1123f0)
	structural.final_startup_event = space:read_u16(v501 and 0x11224a or 0x1123ee)
	structural.final_startup_flags = space:read_u8(v501 and 0x1121c5 or 0x112399)
	structural.final_contact_status = space:read_u16(0x11fed0)
	structural.final_no_sim = space:read_u8(v501 and 0x111a94 or 0x111c64)
	structural.final_sim_enable = space:read_u8(v501 and 0x111aa9 or 0x111c79)
	structural.startup_modes[structural.final_startup_mode] = true
end

emu.add_machine_frame_notifier(function()
	frames = frames + 1
	if lcd_dirty then
		queue_lcd_dump()
		lcd_dirty = false
	end
	write_lcd_dump()
	sample_structural_state()
	if frames % 30 == 0 then write_boot_summary() end

	if not post_sequence_driven then update_post_ready_key() end
end)

-- Frame notifications stop while this firmware gates the LCD clock.  Drive
-- delayed input from an emulation-time coroutine so headless runs do not depend
-- on either video frames or frontend periodic callbacks.
if #post_keys > 0 then
	local input_timer = coroutine.create(function()
		if post_sequence_driven then
			if is_five_row_product then
				-- The structural addresses above belong to the 3210 ROMs.  A
				-- product-spanning input fixture uses an explicit boot-relative
				-- delay for five-row products rather than treating unrelated RAM
				-- as ready.
				startup_ready_time = emulation_seconds()
			else
				repeat
					emu.wait(0.01)
					sample_structural_state()
				until (structural.final_startup_flags & 0x0f) == 0x0f
				startup_ready_time = emulation_seconds()
			end
			emu.wait(post_delay)
			for _, name in ipairs(post_keys) do
				local wait_ms = string.match(name, "^wait(%d+)$")
				if wait_ms then
					emu.wait(tonumber(wait_ms) / 1000)
				elseif name == "waitbuzzer" then
					-- Acceptance orchestration only: observe the mapped MAD2 PUP
					-- buzzer gate, then deliver the next ordinary physical key.
					-- This avoids coupling an answer fixture to firmware timing
					-- without modifying call, UI or device state.
					local deadline = emulation_seconds() + 20
					repeat emu.wait(0.005) until
							(space:read_u8(0x20015) & 0x20) ~= 0 or
							emulation_seconds() >= deadline
					if (space:read_u8(0x20015) & 0x20) == 0 then
						machine:logerror("input-wait: buzzer timeout\n")
						return
					end
				elseif name == "waitalerting" then
					-- Observe the firmware-owned CC Alerting publication.  This
					-- output is diagnostic only; the following key still crosses
					-- the ordinary physical matrix and firmware UI.
					local deadline = emulation_seconds() + 20
					repeat emu.wait(0.005) until
							call_alerting_output:get() ~= 0 or
							call_active_output:get() ~= 0 or
							emulation_seconds() >= deadline
					if call_alerting_output:get() == 0 and
							call_active_output:get() == 0 then
						machine:logerror("input-wait: call alerting timeout\n")
						return
					end
				else
					press(name)
					emu.wait(post_duration)
					release(name)
					emu.wait(post_gap)
				end
			end
			if post_capture_delay >= 0 then
				emu.wait(post_capture_delay)
				pending_lcd = {}
				queue_lcd_dump()
				write_lcd_dump()
			end
		else
			repeat
				emu.wait(0.01)
				sample_structural_state()
				update_post_ready_key()
			until false
		end
	end)
	assert(coroutine.resume(input_timer))
end

if charger_pulse_at >= 0 and charger_field then
	local charger_timer = coroutine.create(function()
		emu.wait(charger_pulse_at)
		charger_field:set_value(1)
		emu.wait(charger_pulse_duration)
		charger_field:clear_value()
	end)
	assert(coroutine.resume(charger_timer))
end

if mbus_rx_fixture and mbus_rx_field then
	local mbus_rx_timer = coroutine.create(function()
		emu.wait(mbus_rx_fixture_at)
		mbus_rx_field:set_value(mbus_rx_fixture & 0xff)
	end)
	assert(coroutine.resume(mbus_rx_timer))
end

-- Focused MAD2 conformance fixtures use physical inputs or the mapped MAD2
-- registers only. They never write firmware RAM or construct RTOS messages.
if irq_overlap_at >= 0 and charger_field and key_fields.up then
	local overlap_timer = coroutine.create(function()
		emu.wait(irq_overlap_at)
		-- Input callbacks can let the CPU service the first edge before Lua
		-- asserts the second. Gate delivery briefly so both physical sources
		-- reach MAD2 pending state before testing aggregation.
		local old_ctrl = space:read_u8(0x2000c) & 0xdf
		space:write_u8(0x2000c, old_ctrl & 0xfb)
		press("up")
		charger_field:set_value(1)
		space:write_u8(0x2000c, old_ctrl | 0x04)
		emu.wait(0.05)
		release("up")
		charger_field:clear_value()
	end)
	assert(coroutine.resume(overlap_timer))
end

if irq_mask_fixture_at >= 0 and key_fields.up then
	local mask_timer = coroutine.create(function()
		emu.wait(irq_mask_fixture_at)
		-- This fixture assumes firmware does not intentionally reprogram these
		-- controller gates during its bounded 40 ms test window.
		local old_mask = space:read_u8(0x2000b)
		local old_ctrl = space:read_u8(0x2000c) & 0xdf
		-- Keep global delivery disabled while creating a masked pending IRQ0.
		space:write_u8(0x2000c, old_ctrl & 0xfb)
		space:write_u8(0x2000b, old_mask | 0x01)
		press("up")
		-- Hold across a video/input sampling boundary while delivery is gated.
		emu.wait(0.02)
		space:write_u8(0x2000b, old_mask & 0xfe)
		space:write_u8(0x2000c, old_ctrl | 0x04)
		-- Disable delivery and acknowledge before firmware can consume the key.
		space:write_u8(0x2000c, old_ctrl & 0xfb)
		space:write_u8(0x20009, 0x01)
		release("up")
		emu.wait(0.02)
		space:write_u8(0x20009, 0x01)
		space:write_u8(0x2000b, old_mask)
		space:write_u8(0x2000c, old_ctrl)
	end)
	assert(coroutine.resume(mask_timer))
end

if fiq8_fixture_at >= 0 then
	local fiq8_timer = coroutine.create(function()
		emu.wait(fiq8_fixture_at)
		local old_ctrl = space:read_u8(0x2000c) & 0xdf
		local old_fiq8 = space:read_u8(0x20016) & 0x05
		-- Enable FIQ8 while both its local mask and the global FIQ gate are set.
		space:write_u8(0x2000c, old_ctrl & 0xfe)
		space:write_u8(0x20016, 0x05)
		emu.wait(0.003)
		space:read_u8(0x20016)
		space:write_u8(0x20016, 0x01)
		space:write_u8(0x2000c, old_ctrl | 0x01)
		-- Remove delivery before acknowledging the extended pending bit.
		space:write_u8(0x2000c, old_ctrl & 0xfe)
		space:write_u8(0x20016, 0x07)
		space:write_u8(0x20016, old_fiq8)
		space:write_u8(0x2000c, old_ctrl)
	end)
	assert(coroutine.resume(fiq8_timer))
end

if buzzer_fixture_at >= 0 then
	local buzzer_timer = coroutine.create(function()
		emu.wait(buzzer_fixture_at)
		local old_pup = space:read_u8(0x20015)
		-- 13 MHz / 6500 = 2 kHz. This is a controller conformance fixture,
		-- not an emulated firmware ringtone or application-state shortcut.
		space:write_u8(0x2001c, 0x19)
		space:write_u8(0x2001d, 0x64)
		space:write_u8(0x2001e, 0x7f)
		space:write_u8(0x20015, old_pup | 0x20)
		emu.wait(0.05)
		space:write_u8(0x20015, old_pup & 0xdf)
	end)
	assert(coroutine.resume(buzzer_timer))
end

if vibrator_fixture_at >= 0 then
	local vibrator_timer = coroutine.create(function()
		emu.wait(vibrator_fixture_at)
		local old_pup = space:read_u8(0x20015)
		local old_control = space:read_u8(0x2001b)
		-- Controller conformance only: the firmware-owned incoming-call path has
		-- not yet exercised these registers organically.
		space:write_u8(0x2001b, 0x55)
		space:write_u8(0x20015, old_pup | 0x10)
		emu.wait(0.05)
		space:write_u8(0x20015, old_pup & 0xef)
		emu.wait(0.01)
		space:write_u8(0x2001b, old_control)
	end)
	assert(coroutine.resume(vibrator_timer))
end

if rtc_fixture_at >= 0 then
	local rtc_timer = coroutine.create(function()
		emu.wait(rtc_fixture_at)
		local old_control = space:read_u8(0x2002d)
		local function ccont_write(reg, value)
			-- Re-selecting CCONT begins a two-byte register transaction.
			space:write_u8(0x2002d, old_control | 0x04)
			space:write_u8(0x2002c, reg << 3)
			space:write_u8(0x2002c, value)
		end
		-- Cross 12:00:58 -> 12:01 and assert the matching alarm. This is a
		-- controller conformance fixture, not a firmware clock-state shortcut.
		ccont_write(0x07, 58)
		ccont_write(0x08, 0)
		ccont_write(0x09, 12)
		ccont_write(0x0b, 1)
		ccont_write(0x0c, 12)
		ccont_write(0x0f, 0x50) -- unmask alarm bit 7 for this controller test
		space:write_u8(0x2002d, old_control)
	end)
	assert(coroutine.resume(rtc_timer))
end

if mad2_reset_fixture_at >= 0 then
	local reset_timer = coroutine.create(function()
		emu.wait(mad2_reset_fixture_at)
		-- Exercise the mapped MAD2 reset controller only. The device callback,
		-- not this harness, determines reset extent and the resulting cause.
		space:write_u8(0x20001, space:read_u8(0x20001) | 0x04)
	end)
	assert(coroutine.resume(reset_timer))
end

if mad2_watchdog_fixture_at >= 0 then
	local watchdog_timer = coroutine.create(function()
		emu.wait(mad2_watchdog_fixture_at)
		-- Load a one-tick ASIC-watchdog interval through mapped MMIO. Device
		-- timing and reset-domain behavior remain entirely driver-owned.
		space:write_u8(0x20003, 0x01)
	end)
	assert(coroutine.resume(watchdog_timer))
end

if ccont_watchdog_fixture_at >= 0 then
	local watchdog_timer = coroutine.create(function()
		emu.wait(ccont_watchdog_fixture_at)
		local old_control = space:read_u8(0x2002d)
		-- Load CCONT register 5 through the mapped GENSIO transaction. The
		-- device owns countdown timing, reset extent and retained status.
		space:write_u8(0x2002d, old_control | 0x04)
		space:write_u8(0x2002c, 0x05 << 3)
		space:write_u8(0x2002c, 0x01)
		space:write_u8(0x2002d, old_control)
	end)
	assert(coroutine.resume(watchdog_timer))
end

if ccont_mask_fixture_at >= 0 then
	local mask_timer = coroutine.create(function()
		emu.wait(ccont_mask_fixture_at)
		local old_control = space:read_u8(0x2002d)
		local function ccont_write(reg, value)
			space:write_u8(0x2002d, old_control | 0x04)
			space:write_u8(0x2002c, reg << 3)
			space:write_u8(0x2002c, value)
		end
		local function ccont_read(reg)
			space:write_u8(0x2002d, old_control | 0x04)
			space:write_u8(0x2002c, (reg << 3) | 0x04)
			space:read_u8(0x2006d)
			return space:read_u8(0x2006c)
		end
		-- Hold every source masked, then create a minute+alarm transition. Reading
		-- status proves the source remained pending without acknowledging it.
		ccont_write(0x0f, 0xf8)
		ccont_write(0x07, 58)
		ccont_write(0x08, 0)
		ccont_write(0x09, 12)
		ccont_write(0x0b, 1)
		ccont_write(0x0c, 12)
		emu.wait(2.2)
		ccont_read(0x0e)
		-- Unmask only alarm bit 7. The device must assert its existing pending
		-- source now; firmware remains responsible for reading and acknowledging it.
		ccont_write(0x0f, 0x78)
		space:write_u8(0x2002d, old_control)
	end)
	assert(coroutine.resume(mask_timer))
end


if eeprom_fixture_at >= 0 then
	local eeprom_timer = coroutine.create(function()
		emu.wait(eeprom_fixture_at)
		local signal_address, direction_address = 0x20020, 0x20024
		local old_signal = space:read_u8(signal_address)
		local old_direction = space:read_u8(direction_address)
		local signal, direction = old_signal, old_direction

		local function set_scl(scl)
			signal = (signal & 0xf7) | ((scl & 1) << 3)
			space:write_u8(signal_address, signal)
		end
		local function set_sda(sda)
			if sda == nil then
				direction = direction & 0xfe
			else
				signal = (signal & 0xfe) | (sda & 1)
				space:write_u8(signal_address, signal)
				direction = direction | 1
			end
			space:write_u8(direction_address, direction)
		end
		local function start()
			set_scl(0); set_sda(nil); set_scl(1); set_sda(0); set_scl(0)
		end
		local function stop()
			set_scl(0); set_sda(0); set_scl(1); set_sda(nil)
		end
		local function write_byte(value)
			for bit = 7, 0, -1 do
				set_scl(0); set_sda((value >> bit) & 1); set_scl(1)
			end
			set_scl(0); set_sda(nil); set_scl(1)
			local ack = (space:read_u8(signal_address) & 1) == 0
			set_scl(0)
			return ack
		end
		local function read_byte(nack)
			local value = 0
			for _ = 1, 8 do
				set_scl(0); set_sda(nil); set_scl(1)
				value = (value << 1) | (space:read_u8(signal_address) & 1)
			end
			set_scl(0); set_sda(nack and 1 or 0); set_scl(1); set_scl(0); set_sda(nil)
			return value
		end
		local function select_address(address)
			start()
			local ok = write_byte(0xa0)
			ok = write_byte((address >> 8) & 0xff) and ok
			ok = write_byte(address & 0xff) and ok
			return ok
		end
		local function random_read(address)
			local address_ack = select_address(address)
			start()
			local read_ack = write_byte(0xa1)
			local value = read_byte(true)
			stop()
			return address_ack and read_ack, value
		end

		local addresses = { 0x3ffe, 0x3fff, 0x3fc0, 0x3fc1 }
		local expected = { 0xa1, 0xb2, 0xc3, 0xd4 }
		local initial_ack, busy_ack, ready_ack = true, false, true
		if eeprom_fixture_mode == "write" then
			initial_ack = select_address(0x3ffe)
			for _, value in ipairs(expected) do
				initial_ack = write_byte(value) and initial_ack
			end
			stop()
			start(); busy_ack = write_byte(0xa0); stop()
			emu.wait(0.006)
			start(); ready_ack = write_byte(0xa0); stop()
		end
		local values, reads_ok = {}, true
		for index, address in ipairs(addresses) do
			local ack, value = random_read(address)
			reads_ok = reads_ok and ack
			values[index] = value
		end
		local data_ok = true
		for index, value in ipairs(expected) do
			data_ok = data_ok and values[index] == value
		end
		machine:logerror(string.format(
			"eeprom_fixture: mode=%s initial_ack=%u busy_ack=%u ready_ack=%u reads_ok=%u data_ok=%u data=%02x,%02x,%02x,%02x t=%.6f\n",
			eeprom_fixture_mode, initial_ack and 1 or 0, busy_ack and 1 or 0,
			ready_ack and 1 or 0, reads_ok and 1 or 0, data_ok and 1 or 0,
			values[1], values[2], values[3], values[4], emulation_seconds()))
		space:write_u8(direction_address, old_direction)
		space:write_u8(signal_address, old_signal)
	end)
	assert(coroutine.resume(eeprom_timer))
end

if mad2_sleep_fixture_at >= 0 then
	local sleep_timer = coroutine.create(function()
		emu.wait(mad2_sleep_fixture_at)
		local old_fiq_mask = space:read_u8(0x2000a)
		local old_irq_mask = space:read_u8(0x2000b)
		local old_ctrl = space:read_u8(0x2000c) & 0xdf
		local old_clock = space:read_u8(0x2000d)
		local old_fiq8 = space:read_u8(0x20016) & 0x05
		if mad2_sleep_fixture_source == "timer1" then
			-- Timer 1/FIQ5 is the internal sleep-counter wake source. Its 0x7fff
			-- destination is hardware-owned and read-only, so this fixture waits
			-- for the real product-rate counter rather than changing either one.
			space:write_u8(0x2000a, old_fiq_mask & 0xdf)
			space:write_u8(0x2000c, old_ctrl | 0x01)
		elseif mad2_sleep_fixture_source == "keypad" then
			-- A physical key edge is one documented external wake source.
			space:write_u8(0x2000b, old_irq_mask & 0xfe)
			space:write_u8(0x2000c, old_ctrl | 0x04)
		else
			-- Firmware leaves FIQ8 eligible across clock-stop. Acknowledge stale
			-- state, enable its periodic source and expose the extended FIQ route.
			space:write_u8(0x2000c, old_ctrl & 0xfe)
			space:write_u8(0x20016, 0x03)
			space:write_u8(0x2000c, old_ctrl | 0x01)
		end
		space:write_u8(0x2000d, old_clock | 0x02)
		if mad2_sleep_fixture_source == "keypad" then
			emu.wait(0.05)
			press("up")
			emu.wait(0.02)
			release("up")
		else
			emu.wait(0.05)
		end
		space:write_u8(0x2000a, old_fiq_mask)
		space:write_u8(0x2000b, old_irq_mask)
		if mad2_sleep_fixture_source == "fiq8" then
			space:write_u8(0x2000c, old_ctrl & 0xfe)
			space:write_u8(0x20016, 0x07)
			space:write_u8(0x20016, old_fiq8)
		end
		space:write_u8(0x2000c, old_ctrl)
		space:write_u8(0x2000d, old_clock)
	end)
	assert(coroutine.resume(sleep_timer))
end

if state_roundtrip_at >= 0 then
	local state_timer = coroutine.create(function()
		emu.wait(state_roundtrip_at)
		if state_roundtrip_wait_release then
			local deadline = emulation_seconds() + 60
			repeat emu.wait(0.001) until
					call_release_waiting_output:get() ~= 0 or
					emulation_seconds() >= deadline
			if call_release_waiting_output:get() == 0 then
				machine:logerror("state_roundtrip: release wait timeout\n")
				return
			end
		end
		local state_roundtrip_requested_at = emulation_seconds()
		local snapshot = {
			mode = space:read_u16(v501 and 0x11224c or 0x1123f0),
			counter = space:read_u16(0x20010),
			fiq_mask = space:read_u8(0x2000a),
			irq_mask = space:read_u8(0x2000b),
			irq_ctrl = space:read_u8(0x2000c),
			gensio = space:read_u8(0x2006d),
		}
		if state_roundtrip_replay > 0 then
			machine:logerror(string.format(
				"state_replay: phase=reference event=begin t=%.6f\n",
				emulation_seconds()))
		end
		machine:save("nokia_dct3_mad2_contract")
		if state_roundtrip_replay > 0 then
			-- Saving completes on the next scheduler boundary. Keep that
			-- settling time inside the marked interval so the reference and
			-- restored branches cover identical emulated timestamps.
			emu.wait(0.01)
			if state_roundtrip_replay > 0.01 then
				emu.wait(state_roundtrip_replay - 0.01)
			end
			machine:logerror(string.format(
				"state_replay: phase=reference event=end t=%.6f\n",
				emulation_seconds()))
		else
			emu.wait(0.05)
		end
		machine:load("nokia_dct3_mad2_contract")
		emu.wait(0.01)
		if state_roundtrip_replay > 0 then
			machine:logerror(string.format(
				"state_replay: phase=restored event=begin t=%.6f\n",
				emulation_seconds()))
		end
		local counter = space:read_u16(0x20010)
		local counter_delta = (counter - snapshot.counter) & 0xffff
		local passed =
			space:read_u16(v501 and 0x11224c or 0x1123f0) == snapshot.mode and
			space:read_u8(0x2000a) == snapshot.fiq_mask and
			space:read_u8(0x2000b) == snapshot.irq_mask and
			space:read_u8(0x2000c) == snapshot.irq_ctrl and
			space:read_u8(0x2006d) == snapshot.gensio and
			counter_delta < 0x1000
		structural.state_roundtrip = passed and "pass" or "fail"
		machine:logerror(string.format(
			"state_roundtrip: result=%s timer_delta=%04x mode=%04x "
				.. "requested_at=%.6f t=%.6f\n",
			structural.state_roundtrip, counter_delta, snapshot.mode,
			state_roundtrip_requested_at, emulation_seconds()))
		if state_roundtrip_replay > 0 then
			emu.wait(state_roundtrip_replay)
			machine:logerror(string.format(
				"state_replay: phase=restored event=end t=%.6f\n",
				emulation_seconds()))
		end
		-- Lua coroutine timers are deliberately outside the machine image.
		-- Start any continuation keys from the restored branch itself so a
		-- save taken at a firmware UI boundary can be followed by ordinary
		-- physical navigation without relying on a pre-save input timer.
		if #state_roundtrip_keys > 0 then
			emu.wait(state_roundtrip_key_delay)
			for _, name in ipairs(state_roundtrip_keys) do
				local wait_ms = string.match(name, "^wait(%d+)$")
				if wait_ms then
					emu.wait(tonumber(wait_ms) / 1000)
				else
					press(name)
					emu.wait(post_duration)
					release(name)
					emu.wait(post_gap)
				end
			end
		end
		-- Optional acceptance orchestration: issue an ordinary physical End
		-- only after the restored machine has run for the requested interval.
		-- A pre-existing Lua wait is deliberately unsuitable because Lua
		-- coroutine timers are outside MAME's machine save image.
		if state_roundtrip_end_delay >= 0 then
			emu.wait(state_roundtrip_end_delay)
			press("enter")
			emu.wait(post_duration)
			release("enter")
		end
	end)
	assert(coroutine.resume(state_timer))
end

-- Keep the semantic oracle current independently of display activity.
emu.register_periodic(function()
	if ram_dump_path and emulation_seconds() >= ram_dump_at then write_ram_dump() end
	if lcd_dirty then
		pending_lcd = {}
		queue_lcd_dump()
		write_lcd_dump()
		lcd_dirty = false
	end
	sample_structural_state()
	write_boot_summary()
end)

emu.add_machine_stop_notifier(function()
	for name in pairs(active_fields) do release(name) end
	write_ram_dump()
	-- Always publish the terminal mirror. Some firmware states update only
	-- part of the LCD, so no later 504-byte wrap exists for make frame to use.
	pending_lcd = {}
	queue_lcd_dump()
	write_lcd_dump()
	write_boot_summary()
end)

info("Nokia DCT3 oracle/input harness installed")
if cpu.debug then cpu.debug:go() end
