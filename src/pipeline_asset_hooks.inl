// Capture after both constructors and the source-geometry repair hooks return.
X86_EXTERN_FUNC(__imp__sub_00133A60);
X86_FUNC(sub_00133A60) {
    const uint32_t source = kernel::R32(ctx.esp + 4);
    const uint32_t source_type = source ? kernel::R32(source + 4) : 0;
    __imp__sub_00133A60(ctx, base);
    if (ctx.eax) openshao::pipelines::CapturePipelineObject(ctx.eax, source_type);
}

// Post-process programs and their states are known before the first scene renders.
X86_EXTERN_FUNC(__imp__sub_00139E30);
X86_FUNC(sub_00139E30) {
    __imp__sub_00139E30(ctx, base);
    openshao::pipelines::WarmShaderLibraryPipelines();
}
