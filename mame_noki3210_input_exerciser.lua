local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local output_dir = os.getenv("NOKI3210_SNAPSHOT_DIR") or ".."
local boot_summary_path = os.getenv("NOKI3210_BOOT_SUMMARY")
local quiet = os.getenv("NOKI3210_LUA_QUIET") == "1"

if quiet then emu.print_info = function(...) end end

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
local startup_ready_time = nil
local post_key_active = nil

local structural = {
	gensio_controls = {}, ccont_commands = {}, startup_modes = {},
	ccont_bytes = 0, ccont_reads = 0, eeprom_starts = 0,
	eeprom_signal_writes = 0, irq_seen = 0, fiq_seen = 0, soft_resets = 0,
	final_current_task = 0, final_startup_mode = 0, final_startup_event = 0,
	final_startup_flags = 0, final_contact_status = 0,
	final_no_sim = 0, final_sim_enable = 0,
}

for i = 0, (84 * 6) - 1 do lcd_vram[i] = 0 end

local function env_number(name, fallback)
	local value = tonumber(os.getenv(name) or "")
	return value or fallback
end

local function emulation_seconds()
	return machine.time:as_double()
end

local function field_by_mask(tag, mask)
	local port = machine.ioport.ports[":" .. tag] or machine.ioport.ports[tag]
	return port and port:field(mask) or nil
end

local key_fields = {
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

local function press(name)
	local field = key_fields[name]
	if field and not active_fields[name] then
		field:set_value(1)
		active_fields[name] = field
		emu.print_info(string.format("input-press:frame=%d name=%s", frames, name))
	end
end

local function release(name)
	local field = active_fields[name]
	if field then
		field:clear_value()
		active_fields[name] = nil
		emu.print_info(string.format("input-release:frame=%d name=%s", frames, name))
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
	for i = 0, (84 * 6) - 1 do
		local value = lcd_vram[i]
		snapshot[i] = value
		if value == 0 then zero = zero + 1 elseif value == 0xff then ff = ff + 1 else other = other + 1 end
	end
	pending_lcd[#pending_lcd + 1] = {
		seq = lcd_full_dumps, zero = zero, ff = ff, other = other,
		vram = snapshot, control = lcd_control,
	}
end

space:install_write_tap(0x20000, 0x200ff, "noki3210_oracle_mmio", function(offset, data, mask)
	local address, value = bus_byte(offset, data, mask)
	record_mmio(address, value)
	local reg = address & 0xff
	if reg == 0x2e then
		lcd_dirty = true
		lcd_data_writes = lcd_data_writes + 1
		if value ~= 0 then nonzero_lcd_data_writes = nonzero_lcd_data_writes + 1 end
		local old_x, old_y = lcd_x, lcd_y
		lcd_vram[(lcd_y * 84) + lcd_x] = value
		if (lcd_mode & 0x02) ~= 0 then
			lcd_y = lcd_y + 1
			if lcd_y > 5 then lcd_y = 0; lcd_x = (lcd_x + 1) % 84 end
		else
			lcd_x = lcd_x + 1
			if lcd_x > 83 then lcd_x = 0; lcd_y = (lcd_y + 1) % 6 end
		end
		if old_x == 83 and old_y == 5 and lcd_x == 0 and lcd_y == 0 then queue_lcd_dump() end
	elseif reg == 0x6e then
		lcd_dirty = true
		lcd_cmd_writes = lcd_cmd_writes + 1
		if (lcd_mode & 0x01) ~= 0 then
			if (value & 0xf8) == 0x20 then lcd_mode = value & 0x07 end
		elseif (value & 0x80) ~= 0 then lcd_x = (value & 0x7f) % 84
		elseif (value & 0xf8) == 0x40 then lcd_y = value & 0x07
		elseif (value & 0xf8) == 0x20 then lcd_mode = value & 0x07
		elseif (value & 0xf8) == 0x08 then lcd_control = ((value & 0x04) >> 1) | (value & 0x01) end
	end
end)

space:install_read_tap(0x2006c, 0x2006f, "noki3210_oracle_ccont_read", function(offset, data, mask)
	local address = bus_byte(offset, data, mask)
	if (address & 0xff) == 0x6c then structural.ccont_reads = structural.ccont_reads + 1 end
end)

local function write_lcd_dump()
	while #pending_lcd > 0 do
	local pending = table.remove(pending_lcd, 1)
	local filename = string.format("%s/noki3210_lcdmirror_%04d_f%03d_z%03d_ff%03d_o%03d.pgm",
		output_dir, pending.seq, frames, pending.zero, pending.ff, pending.other)
	local f = io.open(filename, "wb")
	if not f then return end
	f:write("P5\n84 48\n255\n")
	for y = 0, 47 do
		local row, bit = y >> 3, y & 7
		for x = 0, 83 do
			local on = (pending.vram[(row * 84) + x] >> bit) & 1
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
		string.format("gensio_controls=%s", hex_set(structural.gensio_controls, 2)),
		string.format("ccont_bytes=%d", structural.ccont_bytes), string.format("ccont_reads=%d", structural.ccont_reads),
		string.format("ccont_commands=%s", hex_set(structural.ccont_commands, 2)),
		string.format("eeprom_starts=%d", structural.eeprom_starts),
		string.format("eeprom_signal_writes=%d", structural.eeprom_signal_writes),
		string.format("startup_modes=%s", hex_set(structural.startup_modes, 4)),
		string.format("final_current_task=%02X", structural.final_current_task),
		string.format("final_startup_mode=%04X", structural.final_startup_mode),
		string.format("final_startup_event=%04X", structural.final_startup_event),
		string.format("final_startup_flags=%02X", structural.final_startup_flags),
		string.format("final_contact_status=%04X", structural.final_contact_status),
		string.format("final_no_sim=%02X", structural.final_no_sim),
		string.format("final_sim_enable=%02X", structural.final_sim_enable),
	}
	local temporary = boot_summary_path .. ".tmp"
	local f = io.open(temporary, "w")
	if f then f:write(table.concat(summary, "\n"), "\n"); f:close(); os.rename(temporary, boot_summary_path) end
end

local post_key = os.getenv("NOKI3210_POST_READY_KEY")
local post_keys = {}
for name in string.gmatch(os.getenv("NOKI3210_POST_READY_KEYS") or post_key or "", "[^,%s]+") do
	post_keys[#post_keys + 1] = name
end
local post_delay = env_number("NOKI3210_POST_READY_KEY_DELAY_MS", 250) / 1000
local post_duration = env_number("NOKI3210_POST_READY_KEY_DURATION_MS", 50) / 1000
local post_gap = env_number("NOKI3210_POST_READY_KEY_GAP_MS", 100) / 1000
local post_period = env_number("NOKI3210_POST_READY_KEY_PERIOD_MS", 0) / 1000
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
	structural.irq_seen = structural.irq_seen | space:read_u8(0x20009)
	structural.fiq_seen = structural.fiq_seen | space:read_u8(0x20008)
	structural.final_current_task = space:read_u8(0x100002)
	structural.final_startup_mode = space:read_u16(0x1123f0)
	structural.final_startup_event = space:read_u16(0x1123ee)
	structural.final_startup_flags = space:read_u8(0x112399)
	structural.final_contact_status = space:read_u16(0x11fed0)
	structural.final_no_sim = space:read_u8(0x111c64)
	structural.final_sim_enable = space:read_u8(0x111c79)
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
			repeat
				emu.wait(0.01)
				sample_structural_state()
			until (structural.final_startup_flags & 0x0f) == 0x0f
			startup_ready_time = emulation_seconds()
			emu.wait(post_delay)
			for _, name in ipairs(post_keys) do
				press(name)
				emu.wait(post_duration)
				release(name)
				emu.wait(post_gap)
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

-- Keep the semantic oracle current independently of display activity.
emu.register_periodic(function()
	sample_structural_state()
	write_boot_summary()
end)

emu.add_machine_stop_notifier(function()
	for name in pairs(active_fields) do release(name) end
	write_lcd_dump()
	write_boot_summary()
end)

emu.print_info("noki3210 oracle/input harness installed")
if cpu.debug then cpu.debug:go() end
