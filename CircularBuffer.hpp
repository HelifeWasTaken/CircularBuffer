/**
 * CircularBuffer.hpp
 * This program is free software: you can redistribute it and/or modify
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <cinttypes>
#include <cassert>
#include <vector>
#include <string>
#include <exception>
#include <cstring>
#include <mutex>

#ifndef HELIFE_CIRCULAR_BUFFER_NAMESPACE
    #define HELIFE_CIRCULAR_BUFFER_NAMESPACE hl
#endif

namespace HELIFE_CIRCULAR_BUFFER_NAMESPACE {
template<typename T>
class CircularBuffer {
public:
    class Error : public std::exception {
    private:
        const std::string _errMsg;

    public:
        Error(const std::string& errMsg) : _errMsg(errMsg) {}
        const char *what() const noexcept override { return _errMsg.c_str(); }
    };

    class ErrorNothingToRead : public Error {
        ErrorNothingToRead(const std::string& errMsg="Nothing is waiting in the buffer")
            : Error(errMsg) {}
    };

    class ErrorBufferBusy : public Error {
        ErrorBufferBusy(const std::string& errMsg="Buffer is currently full and nothing can be written in")
            : Error(errMsg) {}
    };

public:
    //
    // Construct a new CircularBuffer of 20 slots by default
    //
    CircularBuffer(const std::size_t& bufferSize=20)
        : _packetSize(sizeof(T))
        , _bufferSize(bufferSize)
        , _rawBufferSize(_packetSize * _bufferSize)
        , _buffer(new std::uint8_t[_rawBufferSize])
    {
        assert(bufferSize > 1);
    }

    //
    // Destruct the circular buffer
    //
    ~CircularBuffer() { delete[] _buffer; }

    //
    // Returns all the elements that have been readed in the buffer
    // in the form a vector
    //
    // If nothing can be readed an error is thrown ErrorNothingToRead
    //
    // The function reads as much as possible it is up to you to manage what it returns
    //
    // This function is thread safe
    //
    std::vector<T> read() {
        std::lock_guard<std::mutex> lock(_mut);
        std::vector<T> values;

        if (canReadNotThreadSafe() == false) {
            throw ErrorNothingToRead();
        }
        for (; _readPtr != _writePtr; _readPtr = nextIndex(_readPtr)) {
            T value;

            std::memcpy(&value, _buffer + _readPtr, _packetSize);

            values.push_back(value);
        }
        return values;
    }

    //
    // Returns the number of packet that have been written
    // If no packet can be written at the moment an error ErrorBufferBusy is sent
    //
    // The function takes in parameter all the packets that must be written in the buffer
    // And returns how many packets have written
    //
    // All written packets are removed from the vector by default with the boolean removeWrittenPackets
    //
    // This function is thread safe
    //
    ssize_t write(std::vector<T>& packets, const bool& removeWrittenPackets=true) {
        std::lock_guard<std::mutex> lock(_mut);
        size_t packetIndex = 0;

        if (packets.empty()) {
            return 0;
        }

        if (canWriteNotThreadSafe() == false) {
            throw ErrorBufferBusy();
        }

        while (_readPtr != nextIndex(_writePtr) && packetIndex < packets.size()) {
            std::memcpy(_buffer + _writePtr, &packets[packetIndex], _packetSize);
            _writePtr = nextIndex(_writePtr);
        }
        if (removeWrittenPackets)
            packets.erase(packets.begin(), packets.begin() + packetIndex);
        return packetIndex;
    }

    //
    // Tells you whether you can read on the buffer
    // If false that means the buffer is empty
    //
    // This function is thread safe
    //
    bool canRead() {
        std::lock_guard<std::mutex> lock(_mut);
        return canReadNotThreadSafe();
    }

    //
    // Tells you whether you can write on the buffer
    // If false that means you can read (Buffer is full)
    //
    // This function is thread safe
    //
    bool canWrite() {
        std::lock_guard<std::mutex> lock(_mut);
        return canWriteNotThreadSafe();
    }

private:

    //
    // This function is dangerous and only exist
    // to support the calls when the mutex is already locked
    // You should not have to use this if you are not developping internally
    //
    bool canReadNotThreadSafe() const
    { return _readPtr != _writePtr; }

    //
    // This function is dangerous and only exist
    // to support the calls when the mutex is already locked
    // You should not have to use this if you are not developping internally
    //
    bool canWriteNotThreadSafe() const
    { return nextIndex(_writePtr) != _readPtr; }

    // Gives you the expected next index in the buffer
    std::size_t nextIndex(const std::size_t& index) const
    { return (index + _packetSize) % _rawBufferSize; }

    const std::size_t  _packetSize;
    const std::size_t  _bufferSize;
    const std::size_t  _rawBufferSize;

    std::uint8_t *_buffer;

    std::size_t _readPtr  = 0;
    std::size_t _writePtr = 0;

    std::mutex _mut;
};
}
