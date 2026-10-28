// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_DSP_C54X_H
#define MAME_NOKIA_NOKIA_DSP_C54X_H

#include "cpu/tms320c54x/tms320c54x.h"
#include "nokia_cobba.h"
#include "nokia_ccont.h"
#include "nokia_dsp_backend.h"
#include "nokia_dspif.h"
#include "tms320c54x_bsp_state.h"

#include <array>

class nokia_dsp_c54x_device : public device_t, public nokia_dsp_backend_interface
{
public:
	nokia_dsp_c54x_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);
	void set_overlay_end(u16 end) { m_overlay_end = end; }
	// An attached clock owns every codec transfer, including boot loopback;
	// do not also complete a word synchronously from the BDXR write.
	void set_external_codec_clock(bool enabled) { m_external_codec_clock = enabled; }
	// Completed external serial frames; these methods do not supply clocks.
	bool codec_transmit_frame(u16 &word);
	bool codec_receive_frame(u16 word);
	// Explicit completion of a frame whose TX/RX clock relationship is known.
	bool codec_duplex_frame(u16 input, u16 &output);
	// Explicit external receive pins; no clock is generated here. False means
	// the configured mode is outside the non-DLB, non-autobuffer receive subset
	// (burst, or continuous with FIG=0/1).
	bool codec_receive_clock_w(bool clock, bool sync, bool data);
	// External CLKX/FSX output state (burst, or continuous FIG=0/1);
	// driven=false represents high impedance.
	tms320c54x_bsp_state::transmit_clock_result codec_transmit_clock_w(bool clock, bool sync);

	virtual void tx_commit_w(int state) override;
	virtual void service_pending_w(int state) override { }
	virtual void doorbell_w(int state) override;
	virtual void reset_line_w(int released) override;
	virtual void power_off() override { reset_line_w(0); }
	virtual void shared_002_write_w(int state) override { }
	virtual void shared_006_write_w(int state) override { }
	virtual void shared_0fe_read_w(int state) override { }
	virtual void shared_0fe_write_w(int state) override { }
	virtual void shared_100_read_w(int state) override { }
	virtual void shared_100_write_w(int state) override { }
	virtual void mcu_shared_write(u16 byte_offset) override;

	virtual u32 tone_frequency1() const override { return 0; }
	virtual u32 tone_frequency2() const override { return 0; }
	virtual u16 tone_amplitude() const override { return 0; }

	void set_transport_tag(char const *tag) { m_transport.set_tag(tag); }
	void set_cobba_tag(char const *tag) { m_cobba.set_tag(tag); }
	void set_ccont_tag(char const *tag) { m_ccont.set_tag(tag); }
	void set_program_rom_tag(char const *tag) { m_program_rom.set_tag(tag); }
	void set_data_rom_tag(char const *tag) { m_data_rom.set_tag(tag); }

protected:
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;

private:
	TIMER_CALLBACK_MEMBER(slot_timer_expired);
	TIMER_CALLBACK_MEMBER(transfer_timer_expired);
	TIMER_CALLBACK_MEMBER(frame_timer_expired);
	void arm_slot_timer(u16 quarter_symbols);
	void arm_transfer_timer(u16 quarter_symbols);
	void arm_compare_timer(emu_timer *timer, u16 quarter_symbols, u16 &delay);
	void arm_frame_timer();
	u32 frame_period() const;
	u16 frame_counter() const;
	void program_map(address_map &map) ATTR_COLD;
	void data_map(address_map &map) ATTR_COLD;
	void io_map(address_map &map) ATTR_COLD;
	u16 program_r(offs_t offset);
	void program_w(offs_t offset, u16 data);
	u16 data_r(offs_t offset);
	void data_w(offs_t offset, u16 data);
	u16 io_r(offs_t offset);
	void io_w(offs_t offset, u16 data);
	bool overlay_address(u16 address) const;
	u16 host_request() const;
	void acknowledge_host_command(u16 mask);
	void update_host_command_line();

	required_device<tms320c54x_device> m_cpu;
	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_cobba_device> m_cobba;
	required_device<nokia_ccont_device> m_ccont;
	required_region_ptr<u16> m_program_rom;
	required_region_ptr<u16> m_data_rom;
	std::array<u16, 0x10000> m_program{};
	std::array<u16, 0x10000> m_data{};
	std::array<u16, 0x100> m_io{};
	tms320c54x_bsp_state m_bsp;
	bool m_host_command_line = false;
	bool m_reset_released = false;
	u16 m_overlay_end = 0x2800;
	bool m_external_codec_clock = false;
	u16 m_host_command_vector = 0;
	emu_timer *m_slot_timer = nullptr;
	emu_timer *m_transfer_timer = nullptr;
	emu_timer *m_frame_timer = nullptr;
	u16 m_slot_frame_length = 0;
	u16 m_slot_delay = 0;
	u16 m_transfer_delay = 0;
	u16 m_frame_counter_base = 0;
	attotime m_frame_counter_epoch;
	bool m_slot_compare_written = false;
	bool m_slot_request_pending = false;
	bool m_transfer_compare_written = false;
	bool m_transfer_request_pending = false;
	u64 m_slot_timer_expiries = 0;
	u64 m_transfer_timer_expiries = 0;
	u64 m_frame_timer_expiries = 0;
	u32 m_io_trace_count = 0;
	u32 m_rf_trace_count = 0;
	u32 m_rf_port38_reads = 0;
	u32 m_rf_port39_reads = 0;
	u16 m_rf_port31 = 0;
	u16 m_rf_port32 = 0;
	u32 m_rf_port32_writes = 0;
	u32 m_completion_strobes = 0;
	u32 m_boot_mailbox_writes = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_C54X, nokia_dsp_c54x_device)

#endif // MAME_NOKIA_NOKIA_DSP_C54X_H
