"""Expose vertex pipeline metadata using the pinned assembler's own layout rules."""


def adapt_vertex(vertex, replace_once):
    # This code shares the implementation-local helpers with Assemble. It never
    # reads a vertex/index buffer or calls the data-producing assembly paths.
    describe = r'''
bool DescribePipelineLayout(const State& s, uint32_t prim, Assembled& out) {
    out = Assembled{};
    if (prim < 1 || prim > 10) return false;
    VertexShader fvf_layout;
    const VertexShader* layout = PickLayout(s, fvf_layout);
    if (!layout) return false;
    for (const auto& input : layout->inputs)
        if (input.stream >= int(s.streams.size())) return false;
    out.pretransformed = Pretransformed(s);
    int regs[16];
    const int count = CollectRegs(s, *layout, regs, out);
    if (!count) return false;
    const Inputs inputs{*layout, regs, count};
    out.topology = NativeTopology(prim);
    if (kRawVertices && RawReadable(s, inputs)) {
        int slots[16];
        std::fill(std::begin(slots), std::end(slots), -1);
        for (int k = 0; k < count; ++k) {
            const int reg = regs[k];
            const auto& input = layout->inputs[size_t(reg)];
            int& slot = slots[input.stream];
            if (slot < 0) {
                slot = int(out.buffer_count++);
                out.buffers[size_t(slot)].stride = StrideOf(s, *layout, input);
            }
            const HostElement element = HostFormat(input.type);
            out.elements[size_t(reg)] = {element.format, uint8_t(slot), uint16_t(input.offset)};
            out.decode |= uint64_t(element.decode) << (reg * 4);
        }
        out.strip = NativePrimitive(prim) && (prim == 4 || prim == 6);
    } else {
        for (int k = 0; k < count; ++k)
            out.elements[size_t(regs[k])] = {DXGI_FORMAT_R32G32B32A32_FLOAT, 0, uint16_t(k * 16)};
        out.buffer_count = 1;
        out.buffers[0].stride = uint32_t(count * 16);
    }
    return true;
}

'''
    return replace_once(vertex, '}  // namespace gpu\n', describe + '}  // namespace gpu\n')
