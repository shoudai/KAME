/***************************************************************************
        Copyright (C) 2002-2017 Kentaro Kitagawa
                           kitagawa@phys.s.u-tokyo.ac.jp

        This program is free software; you can redistribute it and/or
        modify it under the terms of the GNU Library General Public
        License as published by the Free Software Foundation; either
        version 2 of the License, or (at your option) any later version.

        You should have received a copy of the GNU Library General
        Public License and a list of authors along with this program;
        see the files COPYING and AUTHORS.
***************************************************************************/

#include "thamwayrealtimedso.h"
#include "dsorealtimeacq_impl.h"
#include <cstring>

constexpr double SMPL_PER_SEC = 5e6; //5MSmps

REGISTER_TYPE(XDriverList, ThamwayPROT3DSO, "Thamway PROT3 digital streaming DSO");

constexpr unsigned int NUM_MAX_CH = 2;

XThamwayPROT3DSO::XThamwayPROT3DSO(const char *name, bool runtime,
    Transaction &tr_meas, const shared_ptr<XMeasure> &meas) :
    XRealTimeAcqDSO<XCharDeviceDriver<XDSO, XThamwayFX3USBInterface>>(name, runtime, tr_meas, meas) {

    std::vector<shared_ptr<XNode>> unnecessary_ui{
        trace3(), trace4(),
        vFullScale1(), vFullScale2(), vFullScale3(), vFullScale4(),
        vOffset1(), vOffset2(), vOffset3(), vOffset4(),
        trigLevel(),
        timeWidth()
    };
    iterate_commit([=](Transaction &tr){
        tr[ *recordLength()] = lrint(SMPL_PER_SEC * 0.01);
        tr[ *fetchMode()] = "Averaging";
        for(auto &&x: {trace1(), trace2()}) {
            tr[ *x].add({"CH1", "CH2"});
        }
        tr[ *trace1()] = "CH2";
        tr[ *trace2()] = "CH1";
        tr[ *average()] = 1;
        for(auto &&x: unnecessary_ui)
            tr[ *x].disable();
    });
}

void
XThamwayPROT3DSO::startAcquision() {
    if(m_acqThreads.empty())
        commitAcquision();
}
void
XThamwayPROT3DSO::commitAcquision() {
    {
        XScopedLock<XMutex> lock(m_acqMutex);
        //allocates buffers.
        m_chunks.resize(NumChunks);
        m_totalSmpsPerCh = 0;
        m_wrChunkEnd = 0;
        m_wrChunkBegin = 0;
        m_currRdChunk = 0;
        m_currRdPos = 0;
        for(auto &&x: m_chunks) {
            x.data.reserve(ChunkSize);
            x.data.clear();
        }
        if(isMemLockAvailable()) {
            mlock(this, sizeof(XThamwayPROT3DSO));
            for(auto &&x: m_chunks) {
                mlock(&x.data[0], x.data.capacity() * sizeof(tRawAI));
            }
        }
    }

    m_acqThreads.resize(NumThreads);
    for(auto &&x: m_acqThreads) {
        x.reset(new  XThread<XThamwayPROT3DSO>(shared_from_this(), &XThamwayPROT3DSO::executeAsyncRead));
        x->resume();
    }

    //waits until async IOs have been submitted.
    for(;;) {
        msecsleep(10);
        {
            XScopedLock<XMutex> lock(m_acqMutex);
            if(m_wrChunkEnd >= NumThreads)
                break;
        }
    }
}
void
XThamwayPROT3DSO::stopAcquision() {
    XScopedLock<XMutex> lock(m_acqMutex);
    for(auto &&x: m_acqThreads) {
        x->terminate();
    }
    for(auto &&x: m_acqThreads) {
        x->waitFor();
    }
    m_acqThreads.clear();
    m_chunks.clear();
}

void
XThamwayPROT3DSO::clearAcquision() {
}

unsigned int
XThamwayPROT3DSO::getNumOfChannels() {
    return NUM_MAX_CH;
}

XString
XThamwayPROT3DSO::getChannelInfoStrings() {
    return {};
}

std::deque<XString>
XThamwayPROT3DSO::hardwareTriggerNames() {
    return {};
}

double
XThamwayPROT3DSO::setupTimeBase() {
    return 1.0 / SMPL_PER_SEC;
}

void
XThamwayPROT3DSO::setupChannels() {
    for(int ch_num: {0,1,2,3}) {
        for(unsigned int i = 0; i < CAL_POLY_ORDER; i++)
            m_coeffAI[ch_num][i] = 0.0;
        m_coeffAI[ch_num][1] = 1.0 / 32768.0; //+-1V F.S.
    }
}

void
XThamwayPROT3DSO::setupHardwareTrigger() {
}

void
XThamwayPROT3DSO::disableHardwareTriggers() {
}

uint64_t
XThamwayPROT3DSO::getTotalSampsAcquired() {
    return m_totalSmpsPerCh;
}

uint32_t
XThamwayPROT3DSO::getNumSampsToBeRead() {
    XScopedLock<XMutex> lock(m_acqMutex);
    uint64_t rdpos_abs_per_ch = m_chunks[m_currRdChunk].posAbsPerCh + m_currRdPos / getNumOfChannels();
    return m_totalSmpsPerCh - rdpos_abs_per_ch;
}

bool
XThamwayPROT3DSO::setReadPositionAbsolute(uint64_t pos) {
    XScopedLock<XMutex> lock(m_acqMutex);
    //searching for corresponding chunk.
    for(m_currRdChunk = m_wrChunkEnd; m_currRdChunk != m_wrChunkBegin;) {
        uint64_t pos_abs_per_ch = m_chunks[m_currRdChunk].posAbsPerCh;
        uint64_t pos_abs_per_ch_end = pos_abs_per_ch +
            m_chunks[m_currRdChunk].data.size() / getNumOfChannels();
        if((pos >= pos_abs_per_ch) && (pos < pos_abs_per_ch_end)) {
            m_currRdPos = (pos - pos_abs_per_ch) * getNumOfChannels();
            return true;
        }
        m_currRdChunk++;
        if(m_currRdChunk == m_chunks.size()) m_currRdChunk = 0;
    }
    return false;
}
void
XThamwayPROT3DSO::setReadPositionFirstPoint() {
    XScopedLock<XMutex> lock(m_acqMutex);
    m_currRdChunk = 0;
    m_currRdPos = 0;
}

uint32_t
XThamwayPROT3DSO::readAcqBuffer(uint32_t size, tRawAI *buf) {
    Snapshot shot( *this);
    bool swap_traces = (shot[ *trace1()].to_str() == "CH2");
    uint32_t samps_read = 0;
    size *= getNumOfChannels();

    auto memcpy_wordswap = [](tRawAI *dst, const tRawAI *src, size_t byte_size) {
        size_t len = byte_size / sizeof(tRawAI);
        if(((uintptr_t)dst % 8 == 0) && (sizeof(tRawAI) == 2)) {
            tRawAI *src_end = (tRawAI*)(((uintptr_t)src + 15) / 16 * 16);
            while(src < src_end) {
                tRawAI ch1, ch2;
                ch2 = *src++; ch1 = *src++; *dst++ = ch1; *dst++ = ch2;
            }
            //unrolls loop.
            auto src_end64 = reinterpret_cast<uint64_t*>((uintptr_t)(src + len)/ 16 * 16);
            assert((uintptr_t)src % 16 == 0);
            auto *src64 = reinterpret_cast<const uint64_t*>(src);
            auto *dst64 = reinterpret_cast<uint64_t*>(dst);
            while(src64 < src_end64) {
//                    ch2 = *rdpos++; ch1 = *rdpos++; *buf++ = ch1; *buf++ = ch2;
//                    ch2 = *rdpos++; ch1 = *rdpos++; *buf++ = ch1; *buf++ = ch2;
//                    ch2 = *rdpos++; ch1 = *rdpos++; *buf++ = ch1; *buf++ = ch2;
//                    ch2 = *rdpos++; ch1 = *rdpos++; *buf++ = ch1; *buf++ = ch2;
                //equiv to above.
                uint64_t f = 0xffff0000ffffuLL;
                auto llw_swapped =
                    ((*src64 & f) * 0x10000u) + ((*src64 / 0x10000u) & f);
                *dst64++ = llw_swapped;
                src64++;
                llw_swapped =
                        ((*src64 & f) * 0x10000u) + ((*src64 / 0x10000u) & f);
                *dst64++ = llw_swapped;
                src64++;
            }
            src = (const tRawAI*)src64;
            dst = (tRawAI*)dst64;
        }
        const tRawAI *src_end = src + len;
        while(src < src_end) {
            tRawAI ch1, ch2;
            ch2 = *src++; ch1 = *src++; *dst++ = ch1; *dst++ = ch2;
        }
    };

    while(size) {
        if(m_currRdChunk == m_wrChunkBegin) {
            break; //nothing to read.
        }
        readBarrier();
        auto &chunk = m_chunks[m_currRdChunk];
        assert(chunk.ioInProgress == false);
        ssize_t len = std::min((uint32_t)chunk.data.size() - m_currRdPos, size);
        if(swap_traces) {
            //copies data with word swapping.
            //test results i7 2.5GHz, OSX10.12, 3.7GB/s
            memcpy_wordswap(buf, &chunk.data[m_currRdPos], len * sizeof(tRawAI));
        }
        else {
            //Simple copy without swap.
            //test results i7 2.5GHz, OSX10.12, 4.5GB/s
            std::memcpy(buf, &chunk.data[m_currRdPos], len * sizeof(tRawAI));
        }
        buf += len;
        samps_read += len;
        size -= len;
        {
            XScopedLock<XMutex> lock(m_acqMutex);
            m_currRdChunk++;
            if(m_currRdChunk == m_chunks.size()) m_currRdChunk = 0;
            m_currRdPos = 0;
        }
    }
    return samps_read / getNumOfChannels();
}


void
XThamwayPROT3DSO::open() throw (XKameError &) {
    XRealTimeAcqDSO<XCharDeviceDriver<XDSO, XThamwayFX3USBInterface>>::open();
}
void
XThamwayPROT3DSO::close() throw (XKameError &) {
    XScopedLock<XInterface> lock( *interface());
    stopAcquision();

    iterate_commit([=](Transaction &tr){
        for(auto &&x: {trace1(), trace2(), trace3(), trace4()})
            tr[ *x].clear();
    });
    XRealTimeAcqDSO<XCharDeviceDriver<XDSO, XThamwayFX3USBInterface>>::close();
}

void*
XThamwayPROT3DSO::executeAsyncRead(const atomic<bool> &terminated) {
    Transactional::setCurrentPriorityMode(Priority::HIGHEST);

    enum class Collision {BufferUnderflow, IOStall};
    while( !terminated) {
        ssize_t wridx; //index of a chunk for async. IO.
        auto issue_async_read = [&]() {
            //Lambda fn to issue async IO and reserves a chunk atomically.
            XScopedLock<XMutex> lock(m_acqMutex);
            ssize_t next_idx = m_wrChunkEnd;
            wridx = next_idx;
            auto &chunk = m_chunks[next_idx++];
            if(chunk.ioInProgress)
                throw Collision::IOStall;
            if(next_idx == m_chunks.size())
                next_idx = 0;
            if(next_idx == m_currRdChunk)
                throw Collision::BufferUnderflow;
            m_wrChunkEnd = next_idx;
            chunk.data.resize(ChunkSize);
            chunk.ioInProgress = true;
            return interface()->asyncReceive( (char*)&chunk.data[0],
                    chunk.data.size() * sizeof(tRawAI));
        };
        try {
            auto async = issue_async_read();
            fprintf(stderr, "asyncRead for %u initiated\n", (unsigned int)wridx);
            while( !async->hasFinished() && !terminated)
                msecsleep(20);
            if(terminated)
                break;
            auto count = async->waitFor() / sizeof(tRawAI);
            fprintf(stderr, "read for %u count=%u\n", (unsigned int)wridx, (unsigned int)count);
            auto &chunk = m_chunks[wridx];
            {
                XScopedLock<XMutex> lock(m_acqMutex);
                chunk.ioInProgress = false;
//                auto expected = chunk.data.size();
                chunk.data.resize(count);
                if(wridx == m_wrChunkBegin) {
                    //rearranges indices to indicate ready for read.
                    while( !m_chunks[wridx].ioInProgress && (wridx != m_wrChunkEnd)) {
                        m_chunks[wridx].posAbsPerCh = m_totalSmpsPerCh;
                        m_totalSmpsPerCh += m_chunks[wridx].data.size() / getNumOfChannels();
                        wridx++;
                        if(wridx == m_chunks.size()) wridx = 0;
                        m_wrChunkBegin = wridx;
                    }
                    fprintf(stderr, "wrBegin=%u, total=%f sec\n", (unsigned int)wridx, (double)m_totalSmpsPerCh / 5e6);
                }
                if(count == 0) {
                //Pulse generation has stopped.
                    fprintf(stderr, "Pulse generation has stopped.\n");
                    throw Collision::IOStall;
                }
            }
        }
        catch (XInterface::XInterfaceError &e) {
            e.print();
            XScopedLock<XMutex> lock(m_acqMutex);
            for(auto &&x: m_acqThreads) {
                x->terminate();
            }
            break;
        }
        catch (Collision &c) {
            switch (c) {
            case Collision::IOStall:
                fprintf(stderr, "IO stall.\n");
                msecsleep(20);
                continue;
            case Collision::BufferUnderflow:
                gErrPrint(i18n("Buffer Underflow."));
                msecsleep(20);
                continue;
            }
        }
    }
    fprintf(stderr, "Thread fin.\n");
    return nullptr;
}

