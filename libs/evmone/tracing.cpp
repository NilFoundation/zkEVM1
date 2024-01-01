// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "tracing.hpp"
#include "execution_state.hpp"
#include "instructions_traits.hpp"
#include <evmc/hex.hpp>
#include <stack>
#include <fstream>

namespace evmone
{
namespace
{
std::string get_name(uint8_t opcode)
{
    // TODO: Create constexpr tables of names (maybe even per revision).
    const auto name = instr::traits[opcode].name;
    return (name != nullptr) ? name : "0x" + evmc::hex(opcode);
}

/// @see create_histogram_tracer()
class HistogramTracer : public Tracer
{
    struct Context
    {
        const int32_t depth;
        const uint8_t* const code;
        uint32_t counts[256]{};

        Context(int32_t _depth, const uint8_t* _code) noexcept : depth{_depth}, code{_code} {}
    };

    std::stack<Context> m_contexts;
    std::ostream& m_out;

    void on_execution_start(
        evmc_revision /*rev*/, const evmc_message& msg, bytes_view code) noexcept override
    {
        m_contexts.emplace(msg.depth, code.data());
    }

    void on_instruction_start(uint32_t pc, const intx::uint256* /*stack_top*/, int /*stack_height*/,
        int64_t /*gas*/, const ExecutionState& /*state*/) noexcept override
    {
        auto& ctx = m_contexts.top();
        ++ctx.counts[ctx.code[pc]];
    }

    void on_execution_end(const evmc_result& /*result*/) noexcept override
    {
        const auto& ctx = m_contexts.top();

        m_out << "--- # HISTOGRAM depth=" << ctx.depth << "\nopcode,count\n";
        for (size_t i = 0; i < std::size(ctx.counts); ++i)
        {
            if (ctx.counts[i] != 0)
                m_out << get_name(static_cast<uint8_t>(i)) << ',' << ctx.counts[i] << '\n';
        }

        m_contexts.pop();
    }

public:
    explicit HistogramTracer(std::ostream& out) noexcept : m_out{out} {}
};


class InstructionTracer : public Tracer
{
    struct Context
    {
        const int32_t depth;
        const uint8_t* const code;  ///< Reference to the code being executed.
        const int64_t start_gas;

        Context(int32_t d, const uint8_t* c, int64_t g) noexcept : depth{d}, code{c}, start_gas{g}
        {}
    };

    std::stack<Context> m_contexts;
    std::ostream& m_out;  ///< Output stream.

    void output_stack(const intx::uint256* stack_top, int stack_height)
    {
        m_out << R"(,"stack":[)";
        const auto stack_end = stack_top + 1;
        const auto stack_begin = stack_end - stack_height;
        for (auto it = stack_begin; it != stack_end; ++it)
        {
            if (it != stack_begin)
                m_out << ',';
            m_out << R"("0x)" << to_string(*it, 16) << '"';
        }
        m_out << ']';
    }

    void on_execution_start(
        evmc_revision /*rev*/, const evmc_message& msg, bytes_view code) noexcept override
    {
        m_contexts.emplace(msg.depth, code.data(), msg.gas);
    }

    void on_instruction_start(uint32_t pc, const intx::uint256* stack_top, int stack_height,
        int64_t gas, const ExecutionState& state) noexcept override
    {
        const auto& ctx = m_contexts.top();

        const auto opcode = ctx.code[pc];
        m_out << "{";
        m_out << R"("pc":)" << std::dec << pc;
        m_out << R"(,"op":)" << std::dec << int{opcode};
        m_out << R"(,"gas":"0x)" << std::hex << gas << '"';
        m_out << R"(,"gasCost":"0x)" << std::hex << instr::gas_costs[state.rev][opcode] << '"';

        // Full memory can be dumped as evmc::hex({state.memory.data(), state.memory.size()}),
        // but this should not be done by default. Adding --tracing=+memory option would be nice.
        m_out << R"(,"memSize":)" << std::dec << state.memory.size();

#if 1  // Dump memory if it was changed
        static bool memory_was_changed = false;
        if (memory_was_changed) {
            m_out << R"(,"memory":")" << evmc::hex({state.memory.data(), state.memory.size()})
                  << '"';
            memory_was_changed = false;
        }
        if (opcode == OP_MSTORE || opcode == OP_MSTORE8 || opcode == OP_CODECOPY ||
            opcode == OP_MSTORE16 || opcode == OP_MSTORE32 || opcode == OP_MSTORE64)
        {
            memory_was_changed = true;
        }
#endif

        output_stack(stack_top, stack_height);
        if (!state.return_data.empty())
            m_out << R"(,"returnData":"0x)" << evmc::hex(state.return_data) << '"';
        m_out << R"(,"depth":)" << std::dec << (ctx.depth + 1);
        m_out << R"(,"refund":)" << std::dec << state.gas_refund;
        m_out << R"(,"opName":")" << get_name(opcode) << '"';

        m_out << "}\n";
    }

    void on_execution_end(const evmc_result& /*result*/) noexcept override { m_contexts.pop(); }

public:
    explicit InstructionTracer(std::ostream& out) noexcept : m_out{out}
    {
        m_out << std::dec;  // Set number formatting to dec, JSON does not support other forms.
    }
};


class InstructionTracerFast : public Tracer
{

public:
    explicit InstructionTracerFast(std::ostream& out) noexcept
      : m_out(std::ofstream("trace.bin", std::ios::out | std::ios::binary))
    {
        (void)out;
    }

private:
    void on_execution_start([[maybe_unused]] evmc_revision rev,
        [[maybe_unused]] const evmc_message& msg, bytes_view code) noexcept override
    {
        m_code = code;
    }

    void on_execution_end([[maybe_unused]] const evmc_result& result) noexcept override
    {
    }

    void on_instruction_start(uint32_t pc,
        const intx::uint256* stack_top,
        int stack_height,
        int64_t gas,
        [[maybe_unused]] const ExecutionState& state) noexcept override
    {

        static bool first = true;
        const auto opcode = m_code[pc];
        auto& traits = instr::traits[m_last_opcode];
        [[maybe_unused]] int push_num = traits.stack_height_required + traits.stack_height_change;
        if (m_last_opcode >= OP_DUP1 && m_last_opcode <= OP_DUP16) {
            push_num = 1;
        } else if (m_last_opcode >= OP_SWAP1 && m_last_opcode <= OP_SWAP16) {
            push_num = 0;
        }

        assert((push_num & ~1) == 0);

        if (!first)
        {
            if ( stack_height != 0)
            {
                m_out.write((const char*)&stack_top[0], stack_top->num_bits/8);
            }
            else
            {
                constexpr const char s[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                m_out.write(s, 32);
            }
        } else {
            first = false;
        }
        m_out.write((char*)&pc, sizeof(uint32_t));
        m_out.write((char*)&opcode, sizeof(char));
        m_out.write((char*)&gas, sizeof(uint32_t));
        m_last_opcode = (Opcode)opcode;
    }

private:
    bytes_view m_code;
    std::ofstream m_out;
    Opcode m_last_opcode{OP_INVALID};
};

}  // namespace

std::unique_ptr<Tracer> create_histogram_tracer(std::ostream& out)
{
    return std::make_unique<HistogramTracer>(out);
}

std::unique_ptr<Tracer> create_instruction_tracer(std::ostream& out)
{
    return std::make_unique<InstructionTracer>(out);
}

std::unique_ptr<Tracer> create_instruction_fast_tracer(std::ostream& out)
{
    return std::make_unique<InstructionTracerFast>(out);
}

}  // namespace evmone
