//MIT License
//
//Copyright (c) 2017 Mindaugas Vinkelis
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.



#ifndef BITSERY_BUFFER_WRITER_H
#define BITSERY_BUFFER_WRITER_H

#include "common.h"
#include <cassert>
#include <algorithm>
#include <iterator>

namespace bitsery {

    struct MeasureSize {

        template<size_t SIZE, typename T>
        void writeBytes(const T &) {
            static_assert(std::is_integral<T>(), "");
            static_assert(sizeof(T) == SIZE, "");
            _bitsCount += BITS_SIZE<T>::value;
        }

        template<typename T>
        void writeBits(const T &, size_t bitsCount) {
            static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "");
            assert(bitsCount <= BITS_SIZE<T>::value);
            _bitsCount += bitsCount;
        }

        template<size_t SIZE, typename T>
        void writeBuffer(const T *, size_t count) {
            static_assert(std::is_integral<T>(), "");
            static_assert(sizeof(T) == SIZE, "");
            _bitsCount += BITS_SIZE<T>::value * count;
        }

        //get size in bytes
        size_t getSize() const {
            return _bitsCount / 8;
        }

    private:
        size_t _bitsCount{};

    };


    struct BufferWriter {
        using value_type = uint8_t;

        BufferWriter(std::vector<uint8_t> &buffer) : _buf{buffer}, _outIt{std::back_inserter(buffer)} {
            static_assert(std::is_unsigned<value_type>::value, "");
        }

        template<size_t SIZE, typename T>
        void writeBytes(const T &v) {
            static_assert(std::is_integral<T>(), "");
            static_assert(sizeof(T) == SIZE, "");

            if (!m_scratchBits) {
                directWrite(&v, 1);
            } else {
                using UT = typename std::make_unsigned<T>::type;
                writeBits(reinterpret_cast<const UT &>(v), BITS_SIZE<T>::value);
            }
        }

        template<size_t SIZE, typename T>
        void writeBuffer(const T *buf, size_t count) {
            static_assert(std::is_integral<T>(), "");
            static_assert(sizeof(T) == SIZE, "");
            if (!m_scratchBits) {
                directWrite(buf, count);
            } else {
                using UT = typename std::make_unsigned<T>::type;
                //todo improve implementation
                const auto end = buf + count;
                for (auto it = buf; it != end; ++it)
                    writeBits(reinterpret_cast<const UT &>(*it), BITS_SIZE<T>::value);
            }
        }

        template<typename T>
        void writeBits(const T &v, size_t bitsCount) {
            static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "");
            assert(bitsCount <= BITS_SIZE<T>::value);
            assert(v <= ((1ULL << bitsCount) - 1));
            writeBitsInternal(v, bitsCount);
        }

        void align() {
            if (m_scratchBits)
                writeBitsInternal(value_type{}, BITS_SIZE<value_type>::value - m_scratchBits);
        }

        void flush() {
            if (m_scratchBits) {
                auto tmp = static_cast<value_type>( m_scratch & bufTypeMask );
                directWrite(&tmp, 1);
                m_scratch >>= m_scratchBits;
                m_scratchBits -= m_scratchBits;
            }
        }


    private:

        template<typename T>
        void directWrite(const T *v, size_t count) {
            const auto bytesSize = sizeof(T) * count;
            const auto pos = _buf.size();
            _buf.resize(pos + bytesSize);
            std::copy_n(reinterpret_cast<const value_type *>(v), bytesSize, _buf.data() + pos);
        }

        template<typename T>
        void writeBitsInternal(const T &v, size_t size) {
            auto value = v;
            auto bitsLeft = size;
            while (bitsLeft > 0) {
                auto bits = std::min(bitsLeft, BITS_SIZE<value_type>::value);
                m_scratch |= static_cast<SCRATCH_TYPE>( value ) << m_scratchBits;
                m_scratchBits += bits;
                if (m_scratchBits >= BITS_SIZE<value_type>::value) {
                    auto tmp = static_cast<value_type>(m_scratch & bufTypeMask);
                    directWrite(&tmp, 1);
                    m_scratch >>= BITS_SIZE<value_type>::value;
                    m_scratchBits -= BITS_SIZE<value_type>::value;

                    value >>= BITS_SIZE<value_type>::value;
                }
                bitsLeft -= bits;
            }
        }
        void writeBitsInternal(const value_type &v, size_t size) {
            if (size > 0) {
                m_scratch |= static_cast<SCRATCH_TYPE>( v ) << m_scratchBits;
                m_scratchBits += size;
                if (m_scratchBits >= BITS_SIZE<value_type>::value) {
                    auto tmp = static_cast<value_type>(m_scratch & bufTypeMask);
                    directWrite(&tmp, 1);
                    m_scratch >>= BITS_SIZE<value_type>::value;
                    m_scratchBits -= BITS_SIZE<value_type>::value;
                }
            }
        }


        const value_type bufTypeMask = 0xFF;
        using SCRATCH_TYPE = typename BIGGER_TYPE<value_type>::type;
        std::vector<value_type> &_buf;
        std::back_insert_iterator<std::vector<value_type>> _outIt;
        SCRATCH_TYPE m_scratch{};
        size_t m_scratchBits{};


        //size_t _bufSize{};

    };
}

#endif //BITSERY_BUFFER_WRITER_H
