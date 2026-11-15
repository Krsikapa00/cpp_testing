#pragma once

// Interrupt-driven UART transmit, written two ways so the designs can be compared:
//
//   UartStatic<Port, Baud>   everything static, no object. The handler calls UartStatic::onInterrupt().
//
// Both share the same Port traits, so the only difference is where the state lives and how the
// interrupt handler reaches it. Pick one and delete the other.
//
// Neither driver defines USART2_IRQHandler. A template can't, because the driver doesn't know which
// instantiation owns the interrupt, so the application defines it with one line (see main.cpp).
//
// Transmit only. write() puts a whole message in the ring buffer or none of it, then enables TXEIE.
// The interrupt sends one character per TXE and disables TXEIE when the buffer is empty.
// write() is the ring buffer's single producer: callers must not run it concurrently from two
// tasks. The Logger's mutex will make sure of that.

#include "stm32f4xx.h"
#include "gpio.hpp"
#include "ringBuffer.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hal {

// ---------------------------------------------------------------------------------------------
// Per-peripheral facts. Each Port (USART1) would need its own struct (APB2, PA9/PA10, AF7)
// ---------------------------------------------------------------------------------------------
struct Usart2Port {
    static constexpr std::uintptr_t base   = USART2_BASE;
    static constexpr IRQn_Type      irq    = USART2_IRQn;
    static constexpr std::uint32_t  pclkHz = 45000000u;   // APB1, after hal::clock::init()
    static constexpr std::uint8_t   af     = 7u;
    using Tx = Gpio<GPIOA_BASE, 2>;
    using Rx = Gpio<GPIOA_BASE, 3>;

    static void enableClock() {
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
        (void)RCC->APB1ENR;
    }
};

namespace detail {

// BRR with 16x oversampling is USARTDIV * 16 = pclk / baud, with the fraction already in the low
// 4 bits, so no shift. Rounded to the nearest value: 45 MHz / 115200 = 390.6, so 391 (0x187).
template<typename Port, std::uint32_t Baud>
struct BaudDivider {
    static_assert(Baud > 0, "Baud rate must be non-zero");
    static constexpr std::uint32_t brr = (Port::pclkHz + Baud / 2u) / Baud;
    static_assert(brr >= 16u && brr <= 0xFFFFu, "Baud rate out of range for this clock");

    // The real baud is pclk / brr. A UART link tolerates a few percent in total, so stay under 2%.
    static constexpr std::uint32_t actual = Port::pclkHz / brr;
    static_assert((actual > Baud ? actual - Baud : Baud - actual) * 50u <= Baud,
                  "Baud rate error over 2% at this clock");
};

template<typename Port>
inline USART_TypeDef& regs() {
    return *reinterpret_cast<USART_TypeDef*>(Port::base);
}

// Shared by both designs: everything up to, but not including, enabling the interrupt.
template<typename Port, std::uint32_t Baud>
inline void configureHardware() {
    Port::enableClock();

    // Select AF7 before switching the pins to alternate mode, so TX never sees AF0.
    Port::Tx::enableClock();
    Port::Tx::setAF(Port::af);
    Port::Tx::setMode(PinMode::Alternate);
    Port::Rx::enableClock();
    Port::Rx::setAF(Port::af);
    Port::Rx::setMode(PinMode::Alternate);

    USART_TypeDef& u = regs<Port>();
    u.CR1 = 0;                                   // disabled while configuring
    u.BRR = BaudDivider<Port, Baud>::brr;
    u.CR1 = USART_CR1_UE | USART_CR1_TE;         // 8N1 is the reset default. No RE until there's an RX path.
}

// Called from task context after the characters are pushed. This read-modify-write of CR1 races
// with the interrupt clearing TXEIE, but harmlessly: the interrupt can preempt us, we can't preempt
// it, and the data was pushed before this line. The worst case is one extra interrupt that finds
// the buffer empty and switches itself off again.
template<typename Port>
inline void startTransmit() {
    regs<Port>().CR1 |= USART_CR1_TXEIE;
}

// The body of the interrupt, given the buffer to drain.
template<typename Port, typename Buffer>
inline void serviceTx(Buffer& buf) {
    USART_TypeDef& u = regs<Port>();
    // Check both: TXE is set most of the time, and later RXNE will share this vector.
    if ((u.SR & USART_SR_TXE) && (u.CR1 & USART_CR1_TXEIE)) {
        char c;
        if (buf.pop(c)) {
            u.DR = static_cast<std::uint8_t>(c);    // writing DR clears TXE
        } else {
            u.CR1 &= ~USART_CR1_TXEIE;              // nothing left: stop asking
        }
    }
}

} // namespace detail

// ---------------------------------------------------------------------------------------------
// Design 1: everything static, templated on the peripheral. There's no object to construct.
// ---------------------------------------------------------------------------------------------
template<typename Port, std::uint32_t Baud, std::size_t TxSize = 256>
class UartStatic {
public:
    UartStatic() = delete;                       // nothing to instantiate

    static void init(std::uint32_t nvicPriority) {
        detail::configureHardware<Port, Baud>();
        NVIC_SetPriority(Port::irq, nvicPriority);
        NVIC_EnableIRQ(Port::irq);
    }

    // Queues all of data[0..len) or none of it. Returns false if it doesn't fit.
    static bool write(const char* data, std::size_t len) {
        if (len > txBuf_.freeSpace()) {
            return false;
        }
        for (std::size_t i = 0; i < len; ++i) {
            txBuf_.push(data[i]);                // can't fail: we're the only producer and checked space
        }
        detail::startTransmit<Port>();
        return true;
    }

    static bool write(const char* str) { return write(str, std::strlen(str)); }

    // Room left in the transmit buffer. Only meaningful to the producer: the interrupt can only
    // make it bigger, so a check followed by writes that fit can't fail. Logger relies on that.
    static std::size_t freeSpace() { return txBuf_.freeSpace(); }

    // Call this, and only this, from the extern "C" handler.
    static void onInterrupt() { detail::serviceTx<Port>(txBuf_); }

private:
    static control::RingBuffer<char, TxSize> txBuf_;
};

// A static member of a class template can be defined in the header: each instantiation gets
// exactly one, however many files include this. For a plain class this would have to go in one
// .cpp file, because `static inline` members are C++17.
template<typename Port, std::uint32_t Baud, std::size_t TxSize>
control::RingBuffer<char, TxSize> UartStatic<Port, Baud, TxSize>::txBuf_;

} // namespace hal
