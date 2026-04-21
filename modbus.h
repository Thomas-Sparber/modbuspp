/* modbus.h
 *
 * Copyright (C) 20017-2021 Fanzhe Lyu <lvfanzhe@hotmail.com>, all rights reserved.
 *
 * modbuspp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

#ifdef USE_BOOST_ASIO
#include <boost/asio.hpp>
#else
#define ASIO_STANDALONE 
#include <asio.hpp>
#endif

#ifdef ENABLE_MODBUSPP_LOGGING
#include <cstdio>
#define LOG(fmt, ...) printf("[ modbuspp ] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG(...) (void)0
#endif

#define MAX_MSG_LENGTH 260

///Function Code
#define READ_COILS 0x01
#define READ_INPUT_BITS 0x02
#define READ_REGS 0x03
#define READ_INPUT_REGS 0x04
#define WRITE_COIL 0x05
#define WRITE_REG 0x06
#define WRITE_COILS 0x0F
#define WRITE_REGS 0x10

///Exception Codes
#define EX_ILLEGAL_FUNCTION 0x01
#define EX_ILLEGAL_ADDRESS 0x02
#define EX_ILLEGAL_VALUE 0x03
#define EX_SERVER_FAILURE 0x04
#define EX_ACKNOWLEDGE 0x05
#define EX_SERVER_BUSY 0x06
#define EX_NEGATIVE_ACK 0x07
#define EX_MEM_PARITY_PROB 0x08
#define EX_GATEWAY_PROBLEMP 0x0A
#define EX_GATEWAY_PROBLEMF 0x0B
#define EX_BAD_DATA 0XFF

/// Modbus Operator Class
class modbus
{

public:
    modbus(const std::string &host, uint16_t port=502) :
		_connected(false),
		PORT(port),
		_msg_id(1),
		_slaveid(1),
		HOST(host),
		_io_context(),
		_socket(_io_context)
	{}

    void modbus_connect()
	{
		if (HOST.empty() || PORT == 0)
		{
			LOG("Missing Host and Port");
			throw std::runtime_error("Missing Host and Port");
		}

		LOG("Found Proper Host %s and Port %d", HOST.c_str(), PORT);

		asio::error_code ec;
		asio::ip::tcp::resolver resolver(_io_context);
		auto endpoints = resolver.resolve(HOST, std::to_string(PORT), ec);
		
		if (ec)
		{
			LOG("Resolve Error: %s", ec.message().c_str());
			throw std::runtime_error("Resolve Error: " + ec.message());
		}

		asio::connect(_socket, endpoints, ec);
		
		if (ec)
		{
			LOG("Connection Error: %s", ec.message().c_str());
			throw std::runtime_error("Connection Error: " + ec.message());
		}

		//20 seconds timeout
	#ifdef _WIN32
		DWORD timeout = 20000;
		setsockopt(_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
		setsockopt(_socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
	#else
		struct timeval timeout = {};
		timeout.tv_sec = 20; 
		timeout.tv_usec = 0;
		setsockopt(_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		setsockopt(_socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	#endif

		LOG("Connected");
		_connected = true;
	}

    void modbus_close()
	{
		asio::error_code ec;
		_socket.close(ec);
		_connected = false;
		LOG("Socket Closed");
	}

    bool is_connected() const
	{
		return _connected;
	}

    void modbus_set_slave_id(int id)
	{
		_slaveid = id;
	}

    std::vector<bool> modbus_read_coils(uint16_t address, uint16_t amount)
	{
		if (!_connected) throw std::runtime_error("Bad connection");
		if (amount > 2040) throw std::runtime_error("Bad data");

		modbus_read(address, amount, READ_COILS);
		std::vector<uint8_t> to_rec = modbus_receive();
		modbuserror_handle(to_rec, READ_COILS);
		
		std::vector<bool> buffer(amount);
		for (auto i = 0; i < amount; i++)
		{
			if (to_rec.size() < 9u + i / 8u + 1) throw std::runtime_error("Not enough data sent");
			buffer[i] = (bool)((to_rec[9u + i / 8u] >> (i % 8u)) & 1u);
		}
		return buffer;
	}

    std::vector<bool> modbus_read_input_bits(uint16_t address, uint16_t amount)
	{
		if (!_connected) throw std::runtime_error("Bad connection");
		if (amount > 2040) throw std::runtime_error("Bad data");

		modbus_read(address, amount, READ_INPUT_BITS);
		std::vector<uint8_t> to_rec = modbus_receive();
		
		std::vector<bool> buffer(amount);
		for (auto i = 0; i < amount; i++)
		{
			if (to_rec.size() < 9u + i / 8u + 1) throw std::runtime_error("Not enough data sent");
			buffer[i] = (bool)((to_rec[9u + i / 8u] >> (i % 8u)) & 1u);
		}
		modbuserror_handle(to_rec, READ_INPUT_BITS);
		return buffer;
	}

    std::vector<uint16_t> modbus_read_holding_registers(uint16_t address, uint16_t amount)
	{
		if (!_connected) throw std::runtime_error("Bad connection");

		modbus_read(address, amount, READ_REGS);
		std::vector<uint8_t> to_rec = modbus_receive();
		modbuserror_handle(to_rec, READ_REGS);
		
		std::vector<uint16_t> buffer(amount);
		for (auto i = 0; i < amount; i++)
		{
			if (to_rec.size() < 10u + 2u * i + 1) throw std::runtime_error("Not enough data sent");
			buffer[i] = ((uint16_t)to_rec[9u + 2u * i]) << 8u;
			buffer[i] += (uint16_t)to_rec[10u + 2u * i];
		}
		return buffer;
	}

    std::vector<uint16_t> modbus_read_input_registers(uint16_t address, uint16_t amount)
	{
		if (!_connected) throw std::runtime_error("Bad connection");
		
		modbus_read(address, amount, READ_INPUT_REGS);
		std::vector<uint8_t> to_rec = modbus_receive();
		modbuserror_handle(to_rec, READ_INPUT_REGS);
		
		std::vector<uint16_t> buffer(amount);
		for (auto i = 0; i < amount; i++)
		{
			if (to_rec.size() < 10u + 2u * i + 1) throw std::runtime_error("Not enough data sent");
			buffer[i] = ((uint16_t)to_rec[9u + 2u * i]) << 8u;
			buffer[i] += (uint16_t)to_rec[10u + 2u * i];
		}
		return buffer;
	}

    void modbus_write_coil(uint16_t address, bool to_write)
	{
		if (!_connected) throw std::runtime_error("Bad connection");

		std::vector<uint16_t> value = { static_cast<uint16_t>(to_write ? 0xFF00 : 0x0000) };
		modbus_write(address, WRITE_COIL, value);
		std::vector<uint8_t> to_rec = modbus_receive();
		modbuserror_handle(to_rec, WRITE_COIL);
	}

    void modbus_write_register(uint16_t address, uint16_t v)
	{
		if (!_connected) throw std::runtime_error("Bad connection");

		std::vector<uint16_t> value = { v };
		modbus_write(address, WRITE_REG, value);
		std::vector<uint8_t> to_rec = modbus_receive();
		modbuserror_handle(to_rec, WRITE_REG);
	}

    void modbus_write_coils(uint16_t address, const std::vector<bool> &value)
	{
		if (!_connected) throw std::runtime_error("Bad connection");
		
		std::vector<uint16_t> temp(value.size());
		for (size_t i = 0; i < value.size(); i++)
		{
			temp[i] = (uint16_t)value[i];
		}
		modbus_write(address, WRITE_COILS, temp);
		std::vector<uint8_t> to_rec = modbus_receive();
		modbuserror_handle(to_rec, WRITE_COILS);
	}

    void modbus_write_registers(uint16_t address, const std::vector<uint16_t> &value)
	{
		if (!_connected) throw std::runtime_error("Bad connection");

		modbus_write(address, WRITE_REGS, value);
		std::vector<uint8_t> to_rec = modbus_receive();
		modbuserror_handle(to_rec, WRITE_REGS);
	}

private:
    void modbus_build_request(std::vector<uint8_t> &to_send, uint16_t address, int func) const
	{
		to_send[0] = (uint8_t)(_msg_id >> 8u);
		to_send[1] = (uint8_t)(_msg_id & 0x00FFu);
		to_send[2] = 0;
		to_send[3] = 0;
		to_send[4] = 0;
		to_send[6] = (uint8_t)_slaveid;
		to_send[7] = (uint8_t)func;
		to_send[8] = (uint8_t)(address >> 8u);
		to_send[9] = (uint8_t)(address & 0x00FFu);
	}

    void modbus_read(uint16_t address, uint16_t amount, int func)
	{
		std::vector<uint8_t> to_send(12);
		modbus_build_request(to_send, address, func);
		to_send[5] = 6;
		to_send[10] = (uint8_t)(amount >> 8u);
		to_send[11] = (uint8_t)(amount & 0x00FFu);
		size_t sent = modbus_send(to_send);
		if (sent != to_send.size()) throw std::runtime_error("Invalid number of bytes sent");
	}

    void modbus_write(uint16_t address, int func, const std::vector<uint16_t> &value)
	{
		uint16_t amount = value.size();
		std::vector<uint8_t> to_send;
		if (func == WRITE_COIL || func == WRITE_REG)
		{
			to_send.resize(12);
			modbus_build_request(to_send, address, func);
			to_send[5] = 6;
			to_send[10] = (uint8_t)(value[0] >> 8u);
			to_send[11] = (uint8_t)(value[0] & 0x00FFu);
		}
		else if (func == WRITE_REGS)
		{
			to_send.resize(13 + 2 * amount);
			modbus_build_request(to_send, address, func);
			to_send[5] = (uint8_t)(7 + 2 * amount);
			to_send[10] = (uint8_t)(amount >> 8u);
			to_send[11] = (uint8_t)(amount & 0x00FFu);
			to_send[12] = (uint8_t)(2 * amount);
			for (int i = 0; i < amount; i++)
			{
				to_send[13 + 2 * i] = (uint8_t)(value[i] >> 8u);
				to_send[14 + 2 * i] = (uint8_t)(value[i] & 0x00FFu);
			}
		}
		else if (func == WRITE_COILS)
		{
			to_send.resize(14 + (amount - 1) / 8);
			modbus_build_request(to_send, address, func);
			to_send[5] = (uint8_t)(7 + (amount + 7) / 8);
			to_send[10] = (uint8_t)(amount >> 8u);
			to_send[11] = (uint8_t)(amount & 0x00FFu);
			to_send[12] = (uint8_t)((amount + 7) / 8);
			for (int i = 0; i < (amount + 7) / 8; i++)
				to_send[13 + i] = 0; 
			for (int i = 0; i < amount; i++)
			{
				to_send[13 + i / 8] += (uint8_t)(value[i] << (i % 8u));
			}
		}
		else
		{
			throw std::runtime_error("Invalid funtion " + std::to_string(func));
		}

		size_t sent = modbus_send(to_send);
		if (sent != to_send.size()) throw std::runtime_error("Invalid number of bytes sent");
	}

    size_t modbus_send(const std::vector<uint8_t> &to_send)
	{
		_msg_id++;
		asio::error_code ec;
		size_t sent = asio::write(_socket, asio::buffer(to_send.data(), to_send.size()), ec);
		if (ec)
		{
			LOG("Send Error: %s", ec.message().c_str());
			throw std::runtime_error("Send Error: " + ec.message());
		}
		return sent;
	}

    std::vector<uint8_t> modbus_receive() const
	{
		asio::error_code ec;
		std::vector<uint8_t> buffer(MAX_MSG_LENGTH);
		size_t rec = _socket.read_some(asio::buffer(buffer.data(), MAX_MSG_LENGTH), ec);
		if (ec)
		{
			LOG("Receive Error: %s", ec.message().c_str());
			throw std::runtime_error("Receive Error: " + ec.message());
		}
		buffer.resize(rec);
		return buffer;
	}

    void modbuserror_handle(const std::vector<uint8_t> &msg, int func)
	{
		if (msg.size() < 9) throw std::runtime_error("Not enough data sent");

		if (msg[7] == func + 0x80)
		{
			switch (msg[8])
			{
				case EX_ILLEGAL_FUNCTION:	throw std::runtime_error("1 Illegal Function");
				case EX_ILLEGAL_ADDRESS:	throw std::runtime_error("2 Illegal Address");
				case EX_ILLEGAL_VALUE:		throw std::runtime_error("3 Illegal Value");
				case EX_SERVER_FAILURE:		throw std::runtime_error("4 Server Failure");
				case EX_ACKNOWLEDGE:		throw std::runtime_error("5 Acknowledge");
				case EX_SERVER_BUSY:		throw std::runtime_error("6 Server Busy");
				case EX_NEGATIVE_ACK:		throw std::runtime_error("7 Negative Acknowledge");
				case EX_MEM_PARITY_PROB:	throw std::runtime_error("8 Memory Parity Problem");
				case EX_GATEWAY_PROBLEMP:	throw std::runtime_error("10 Gateway Path Unavailable");
				case EX_GATEWAY_PROBLEMF:	throw std::runtime_error("11 Gateway Target Device Failed to Respond");
				default:					throw std::runtime_error("UNK");
			}
		}
	}

private:
    bool _connected;
    uint16_t PORT;
    uint32_t _msg_id;
    int _slaveid;
    std::string HOST;

    mutable asio::io_context _io_context;
    mutable asio::ip::tcp::socket _socket;

}; //end class modbus