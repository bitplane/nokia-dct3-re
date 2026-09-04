// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_DSP_BACKEND_H
#define MAME_NOKIA_NOKIA_DSP_BACKEND_H

// Swappable semantic endpoint behind the MAD2 DSP transport.  The current
// implementation is an HLE peer; a C54x implementation can replace it without
// changing DSPIF, MAD2 interrupt routing, or the phone driver.
class nokia_dsp_backend_device : public device_t
{
public:
	auto tone_update_cb() { return m_tone_update_cb.bind(); }

	virtual void tx_commit_w(int state) = 0;
	virtual void service_pending_w(int state) = 0;
	virtual void doorbell_w(int state) = 0;
	virtual void shared_002_write_w(int state) = 0;
	virtual void shared_006_write_w(int state) = 0;
	virtual void shared_0fe_read_w(int state) = 0;
	virtual void shared_0fe_write_w(int state) = 0;
	virtual void shared_100_read_w(int state) = 0;
	virtual void shared_100_write_w(int state) = 0;
	virtual void mcu_shared_write(u16 byte_offset) = 0;

	virtual u32 tone_frequency1() const = 0;
	virtual u32 tone_frequency2() const = 0;
	virtual u16 tone_amplitude() const = 0;

protected:
	nokia_dsp_backend_device(
			const machine_config &mconfig, device_type type, const char *tag,
			device_t *owner, u32 clock) :
		device_t(mconfig, type, tag, owner, clock),
		m_tone_update_cb(*this)
	{
	}

	void notify_tone_update() { m_tone_update_cb(1); }

private:
	devcb_write_line m_tone_update_cb;
};

#endif // MAME_NOKIA_NOKIA_DSP_BACKEND_H
