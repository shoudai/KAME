/***************************************************************************
		Copyright (C) 2002-2012 Kentaro Kitagawa
		                   kitag@kochi-u.ac.jp

		This program is free software; you can redistribute it and/or
		modify it under the terms of the GNU Library General Public
		License as published by the Free Software Foundation; either
		version 2 of the License, or (at your option) any later version.

		You should have received a copy of the GNU Library General
		Public License and a list of authors along with this program;
		see the files COPYING and AUTHORS.
***************************************************************************/
#ifndef MODBUSRTUINTERFACE_H_
#define MODBUSRTUINTERFACE_H_

#include "charinterface.h"
#include "chardevicedriver.h"

class XModbusRTUInterface : public XCharInterface {
public:
	XModbusRTUInterface(const char *name, bool runtime, const shared_ptr<XDriver> &driver);
	virtual ~XModbusRTUInterface() {}

	void readHoldingResistors(uint16_t res_addr, int count, std::vector<uint16_t> &data);
	void presetSingeResistor(uint16_t res_addr, uint16_t data);
	void presetMultipleResistors(uint16_t res_no, int count, const std::vector<uint16_t> &data);
	void diagnostics();

	uint32_t readHoldingTwoResistors(uint16_t res_addr) {
		std::vector<uint16_t> data(2);
		readHoldingResistors(res_addr, 2, data);
		return data[0] * 0xffffuL + data[1];
	}
	void presetTwoResistors(uint16_t res_addr, uint32_t dword) {
		std::vector<uint16_t> data(2);
		data[0] = dword / 0xffffu;
		data[1] = dword % 0xffffu;
		presetMultipleResistors(res_addr, 2, data);
	}
protected:
	void query(unsigned int func_code, const std::vector<char> &bytes, std::vector<char> &buf);
private:
	static void set_word(char *ptr, uint16_t word) {
		ptr[0] = static_cast<unsigned char>(word / 0xffu);
		ptr[1] = static_cast<unsigned char>(word % 0xffu);
	}
	static void set_dword(char *ptr, uint32_t dword) {
		set_word(ptr, static_cast<uint16_t>(dword / 0xffffu));
		set_word(ptr + 2, static_cast<uint16_t>(dword % 0xffffu));
	}
	static uint16_t get_word(char *ptr) {
		return static_cast<unsigned char>(ptr[1]) + static_cast<unsigned char>(ptr[0]) * 0xffu;
	}
	static uint32_t get_dword(char *ptr) {
		return get_word(ptr + 2) + get_word(ptr) * 0xffffuL;
	}
	uint16_t crc16(const char *bytes, ssize_t count);
};

template <class T>
class XModbusRTUDriver : public XCharDeviceDriver<T, XModbusRTUInterface> {
public:
	XModbusRTUDriver(const char *name, bool runtime,
	Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
		XCharDeviceDriver<T, XModbusRTUInterface>(name, runtime, ref(tr_meas), meas) {}
	virtual ~XModbusRTUDriver() {};
};

#endif /*MODBUSRTUINTERFACE_H_*/
